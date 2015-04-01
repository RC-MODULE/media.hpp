#include "msvd.hpp"
#include "utils/utils/byte-sequence.hpp"

namespace media {

namespace mpeg {
using namespace ::mpeg;

struct access_unit_tag {};
template<typename Data>
using access_unit = utils::tagged_byte_sequence<access_unit_tag, Data>;

struct coded_picture_tag {};
template<typename Data>
using coded_picture = utils::tagged_byte_sequence<coded_picture_tag, Data>;

struct picture_data_tag {};
template<typename Data>
using picture_data = utils::tagged_byte_sequence<picture_data_tag, Data>;

}

using timestamp = std::chrono::duration<std::int64_t, std::ratio<1, 90000>>;
}

namespace msvd {
template<typename Buffer, typename Data>
utils::future<void> async_decode_picture(decoder& d, 
  mpeg::sequence_header_t const& sh,
  mpeg::picture_header_t const& ph,
  mpeg::picture_coding_extension_t const* pcx,
  mpeg::quant_matrix_extension_t const* qmx,
  Buffer curpic,
  Buffer ref1,
  Buffer ref2,
  media::mpeg::picture_data<Data> data) 
{
  auto p = std::make_shared<utils::promise<void>>();
  auto s = as_asio_sequence(data);
  std::cout << "decoding" << std::endl;
  async_decode_picture(d, sh, ph, pcx, qmx, std::move(curpic), std::move(ref1), std::move(ref2), s, [p, d = std::move(data)](std::error_code const& ec, decode_result r) {
    std::cout << "decoded:" << std::distance(begin(d), end(d)) << " bytes " << r.num_of_decoded_macroblocks << std::endl;
    if(ec)
      p->set_exception(std::make_exception_ptr(std::system_error(ec)));
    else
      p->set_value();
  });

  return p->get_future();
}

template<typename Buffer, typename Data>
utils::future<void> async_decode_picture(decoder& d,
  mpeg::sequence_header_t const& sh,
  mpeg::picture_header_t const& ph,
  utils::optional<mpeg::picture_coding_extension_t> const& pcx,
  utils::optional<mpeg::quant_matrix_extension_t> const& qmx,
  utils::shared_future<Buffer> curpic,
  utils::shared_future<Buffer> ref1,
  utils::shared_future<Buffer> ref2,
  media::mpeg::picture_data<Data> data)
{
  ref1 = ref1.valid() ? ref1 : utils::make_ready_future(Buffer());
  ref2 = ref2.valid() ? ref2 : utils::make_ready_future(Buffer());

  return when_all(curpic, ref1, ref2).then([=, &d, data = std::move(data)](auto bfrs) mutable {
    auto buffers = bfrs.get();
    return async_decode_picture(d, sh, ph, pcx ? &*pcx : 0, qmx ? &*qmx : 0,
      std::get<0>(buffers).get(), std::get<1>(buffers).get(), std::get<2>(buffers).get(), std::move(data));
  });
}
}

namespace media { namespace mpeg {

template<typename Allocator, typename Sink>
struct decoder {
  msvd::decoder hw;
  Allocator frame_source;
  Sink sink; 

  decoder(asio::io_service& io, Allocator fsrc, Sink sk) : hw(io), frame_source(fsrc), sink(sk) {}

  using frame_type = std::decay_t<decltype(pull(frame_source, timestamp{}))>;

  struct stored_frame {
    mpeg::picture_type pt;
    frame_type frame; 
  };

  utils::optional<stored_frame> frames[3];
  utils::optional<sequence_header_t> sh;

  template<typename BS>
  struct parsed_picture {
    picture_header_t ph;
    utils::optional<picture_coding_extension_t> pcx;
    utils::optional<quant_matrix_extension_t> qmx;
    picture_data<BS> data;
  };

  template<typename BS>
  auto parse_picture(picture_data<BS> data) {
    auto parser = bitstream::make_bit_parser(bitstream::make_bit_range(utils::make_range(begin(data), end(data))));

    if(u(parser, 32) != (0x00000100 | mpeg::picture_start_code)) throw std::runtime_error("expected picture_header");

    auto ph = mpeg::picture_header(parser);

    utils::optional<mpeg::picture_coding_extension_t> pcx;
    utils::optional<mpeg::quant_matrix_extension_t> qmx;

    for(;;) {
      auto startcode = u(parser, 32);
      if(startcode == (0x00000100 | mpeg::extension_start_code)) {
        auto id = u(parser, 4);
        if(id == mpeg::picture_coding_extension_id) 
          pcx = mpeg::picture_coding_extension(parser);
        else if(id == mpeg::quant_matrix_extension_id)
          qmx = mpeg::quant_matrix_extension(parser);
        else
          mpeg::unknown_extension(parser);
      }
      else if(startcode == (0x00000100 | mpeg::user_data_start_code)) {
        while(next_bits(parser, 24) != 1)
          u(parser, 8);
      }
      else
        break;
    }
    using return_type = parsed_picture<decltype(split(std::move(data), begin(data)).second)>;
    return return_type{ph, pcx, qmx, split(std::move(data), parser.begin().base() - 4).second};
  }

  template<typename Data>
  void decode_picture(timestamp ts, picture_data<Data> data) {
    auto p = parse_picture(std::move(data));

    auto pt = p.pcx ? p.pcx->picture_structure : mpeg::picture_type::frame;
    ts += p.ph.temporal_reference * std::chrono::milliseconds(40);

    if(frames[0] && frames[0]->pt != mpeg::picture_type::frame) {
      assert(is_opposite(frames[0]->pt, pt));
      frames[0]->pt = mpeg::picture_type::frame;
    }
    else
      frames[0] = stored_frame{pt, pull(frame_source, ts)};
  
    auto f = msvd::async_decode_picture(hw, *sh, p.ph, p.pcx, p.qmx,
      frames[0]->frame, frames[1] ? frames[1]->frame : frame_type(), frames[2] ? frames[2]->frame : frame_type(),
      std::move(p.data)
    );
    
    if(frames[0]->pt == mpeg::picture_type::frame) {
      f.then([frame = frames[0]->frame, sink = sink](auto f) { push(sink, frame); });
 
      if(p.ph.picture_coding_type != mpeg::picture_coding::B) std::move_backward(&frames[0], &frames[2], &frames[3]);
    }
  }

  template<typename Data>
  friend void push(decoder& d, timestamp ts, access_unit<Data> data) {
    auto p = split(std::move(data), find_next_sequence_or_picture_header(begin(data), end(data)));
    
    if(*(begin(p.first) + 3) == mpeg::sequence_header_code)
      d.sh = mpeg::sequence_header(bitstream::make_bit_parser(bitstream::make_bit_range(utils::make_range(begin(p.first)+4, end(p.first)))));
    else if(d.sh)
      d.decode_picture(ts, utils::tag<picture_data_tag>(std::move(p.first)));

    if(begin(p.second) != end(p.second))
      push(d, ts, utils::tag<access_unit_tag>(std::move(p.second)));
  }
};
/*
template<typename Source, typename Sink>
auto make_decoder(asio::io_service& io, Source&& src, Sink&& sk) {
  return decoder<std::decay_t<Source>, std::decay_t<Sink>>{io, std::forward<Source>(src), std::forward<Sink>(sk)};
}
*/
}}
