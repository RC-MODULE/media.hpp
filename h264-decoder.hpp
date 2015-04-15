#ifndef __h264_decoder_hpp__a6a227a1_2578_4768_80b8_c96ec1305fa1__
#define __h264_decoder_hpp__a6a227a1_2578_4768_80b8_c96ec1305fa1__

#include "types.hpp"
#include "msvd.hpp"
#include "utils/utils/byte-sequence.hpp"
#include "h264-context.hpp"

#include <type_traits>

namespace media {

namespace h264 {
struct coded_slice_tag {};

template<typename BS>
using coded_slice = utils::tagged_byte_sequence<coded_slice_tag, BS>;

}

namespace msvd {

namespace detail {
template<typename S>
auto async_decode_slice(decoder& d, std::unique_ptr<detail::h264_context<S>> cx) {
  utils::promise<decode_result> p;
  auto f = p.get_future();

  std::cout << "decoding" << std::endl;

  async_decode_slice(d, std::move(cx), [p = utils::move_on_copy(std::move(p))](std::error_code const& ec, decode_result const& r) mutable {
    std::cout << "decoded" << std::endl;
    unwrap(p).set_value(r);
  });

  return f;
}
}

template<typename FB, typename BS>
utils::future<decode_result> async_decode_slice(decoder& d, h264::context<utils::shared_future<FB>> const& cx, h264::coded_slice<BS> slice, std::size_t offset) {
  std::vector<utils::shared_future<FB>> frames;
  for(auto& f: cx)
    frames.push_back(frame_buffer(f));

  auto s = as_asio_sequence(slice);
  auto p = std::make_unique<detail::h264_context<decltype(s)>>(cx.sps(), cx.pps(), cx.slice(), cx.begin(), cx.end(), *cx.current_picture(), s, offset);

  return when_all(frames.begin(), frames.end()).then([&d, p = std::move(p)](auto ff) mutable {
    auto frames = ff.get();
    for(int i = 0; i != frames.size(); ++i)
      p->dpb_data[i].phys_addr = phys_addr(frames[i].get());
    
    p->curr_pic.phys_addr = phys_addr(frames.back().get());

    return async_decode_slice(d, std::move(p));
  }).then([s = std::move(slice)](auto f) { return f; });
}

}

namespace h264 {

template<typename Source, typename Sink>
struct decoder {
  msvd::decoder hw;
  Source frame_source;
  Sink sink;

  decoder(asio::io_service& io, Source src, Sink sink) : hw(io), frame_source(std::move(src)), sink(std::move(sink)) {}

  using frame_type = std::decay_t<decltype(pull(frame_source, timestamp{}))>;

  context<frame_type> cx;

  template<typename BS>
  friend utils::future<void> push(decoder& d, timestamp const& ts, annexb::access_unit<BS> au) noexcept {
    try {
      std::vector<utils::future<msvd::decode_result>> slices;

      auto i = bitstream::find_startcode_prefix(begin(au), end(au));
      auto r = split(std::move(au), i);
      for(;begin(r.second) != end(r.second);) {
        i = bitstream::find_next_startcode_prefix(begin(r.second), end(r.second));
        r = split(std::move(r.second), i);
     
        auto pos = d.cx(utils::tag<nalu_tag>(r.first));
        if(d.cx.is_new_slice()) {
          if(d.cx.is_new_picture() && pic_type(*d.cx.current_picture()) != picture_type::bot)
            frame_buffer(*d.cx.current_picture()->frame, pull(d.frame_source, ts));
         
          slices.push_back(async_decode_slice(d.hw, d.cx, utils::tag<coded_slice_tag>(std::move(r.first)), pos));
        }
      }

      if(!slices.empty()) {
        if(pic_type(*d.cx.current_picture()) != picture_type::top) {
          push(d.sink, frame_buffer(*d.cx.current_picture()));
          mark_as_not_needed_for_output(*d.cx.current_picture()->frame);
        }

        d.cx.erase(h264::remove_unused_pictures(d.cx.begin(), d.cx.current_picture()->frame), d.cx.current_picture()->frame);      
      }

      return when_all(slices.begin(), slices.end()).then([](auto f){
        for(auto& s: f.get())
          std::cout << "decoded " << s.get().num_of_decoded_macroblocks << " mbs" << std::endl;
      });
    }
    catch(...) {
      return utils::make_exceptional_future<void>(std::current_exception());
    }
  }
};

}} // namespace media { namespace h264 {
#endif
