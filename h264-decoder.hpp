#ifndef __h264_decoder_hpp__a6a227a1_2578_4768_80b8_c96ec1305fa1__
#define __h264_decoder_hpp__a6a227a1_2578_4768_80b8_c96ec1305fa1__

#include "types.hpp"
#include "msvd.hpp"
#include "utils/utils/byte-sequence.hpp"
#include "h264-context.hpp"

#include <typeinfo>
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
  
  async_decode_slice(d, std::move(cx), [p = utils::move_on_copy(std::move(p))](std::error_code const& ec, decode_result const& r) mutable {
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
    for(auto i = 0u; i != frames.size(); ++i)
      p->dpb_data[i].phys_addr = phys_addr(frames[i].get());
    
    p->curr_pic.phys_addr = phys_addr(frames.back().get());

    return async_decode_slice(d, std::move(p));
  }).then([s = std::move(slice)](auto f) { return f; });
}

}

namespace h264 {

video::resolution get_resolution(seq_parameter_set const& sps) {
  return {(sps.pic_width_in_mbs_minus1 + 1) * 16, (sps.pic_height_in_map_units_minus1 + 1) * (2 - sps.frame_mbs_only_flag) * 16};
}

video::aspect_ratio get_aspect_ratio(seq_parameter_set const& sps) {
  auto aspect_ratio = 1.0;
  if(sps.vui_parameters && sps.vui_parameters->aspect_ratio_information)
    aspect_ratio = double(sps.vui_parameters->aspect_ratio_information->sar_width) / sps.vui_parameters->aspect_ratio_information->sar_width;
  return {aspect_ratio};
}

template<typename Source, typename Sink>
struct decoder {
  std::unique_ptr<msvd::decoder> hw;
  Source frame_source;
  Sink sink;

  decoder(asio::io_service& io, Source src, Sink sink) : hw(new msvd::decoder{io}), frame_source(std::move(src)), sink(std::move(sink)) {}

  using frame_type = std::decay_t<decltype(pull(frame_source))>;

  context<frame_type> cx;

  utils::optional<std::pair<video::resolution, video::aspect_ratio>> dimensions;

  template<typename BS>
  friend utils::shared_future<void> push(decoder& d, timestamp const& ts, annexb::access_unit<BS> au) noexcept {
    try {
      std::vector<utils::future<msvd::decode_result>> slices;

      for(auto r = next_nal_unit(std::move(au)); !empty(r.first); r = next_nal_unit(std::move(r.second))) {
        auto pos = d.cx(r.first);
        if(d.cx.is_new_slice()) {
          if(d.cx.is_new_picture() && pic_type(*d.cx.current_picture()) != picture_type::bot)
            frame_buffer(*d.cx.current_picture()->frame, pull(d.frame_source));

          slices.push_back(async_decode_slice(*d.hw, d.cx, utils::tag<coded_slice_tag>(std::move(r.first)), pos));
        }
      }

      auto f = when_all(slices.begin(), slices.end());
      utils::shared_future<void> r;
    
      if(d.cx.current_picture() && pic_type(*d.cx.current_picture()) != picture_type::top) {
        auto m = std::make_pair(get_resolution(d.cx.sps()), get_aspect_ratio(d.cx.sps()));
        if(!d.dimensions || m != *d.dimensions) set_dimensions(d.sink, m.first, m.second);
        d.dimensions = m;

//        r = f.then([frame = frame_buffer(*d.cx.current_picture()), ts, sink = d.sink](auto) mutable { push(sink, ts, frame); });
        r = f.then([](auto f) {}).share();
        push(d.sink, ts, r.then([frame = frame_buffer(*d.cx.current_picture())](auto) { return frame.get(); }).share());
        mark_as_not_needed_for_output(*d.cx.current_picture()->frame);
      
        d.cx.erase(h264::remove_unused_pictures(d.cx.begin(), d.cx.current_picture()->frame), d.cx.current_picture()->frame);      
      } 
      else
        r = f.then([](auto f) {}).share();
        
      return r; 
    }
    catch(...) {
      return utils::make_exceptional_future<void>(std::current_exception());
    }
  }
};

template<typename Source, typename Sink>
auto make_decoder(asio::io_service& io, Source src, Sink sk) {
  return decoder<Source, Sink>(io, std::move(src), std::move(sk));
}

}} // namespace media { namespace h264 {
#endif
