#ifndef __h264_context__08a5a0d0_f054_44f8_86ac_a9910d61b1e0__
#define __h264_context__08a5a0d0_f054_44f8_86ac_a9910d61b1e0__

#include "h264-dpb.hpp"

namespace h264 {

template<typename FrameBuffer>
struct context : h264::decoded_picture_buffer<FrameBuffer> {
  template<typename I>
  bitstream::bit_iterator<I> operator()(utils::range<I> const& nalu) {
    using namespace h264;

    current_slice = utils::nullopt;

    auto parser = bitstream::make_bit_parser(bitstream::make_bit_range(nalu)); 

    auto h = parse_nal_unit_header(parser);
    
    switch(static_cast<nalu_type>(h.nal_unit_type)) {
    case nalu_type::seq_parameter_set:
      add(params, parse_sps(parser));
      break;
    case nalu_type::pic_parameter_set:
      add(params, parse_pps(params, parser));
      break;
    case nalu_type::slice_layer_non_idr:
    case nalu_type::slice_layer_idr: {
      auto s = parse_slice_header(params, parser, h.nal_unit_type, h.nal_ref_idc);
      if(s) on_slice(std::move(*s));
      break; }
    }

    return parser.begin();
  }

  template<typename BufferSequence>
  std::size_t operator()(BufferSequence s) {
    auto r = bitstream::make_asio_sequence_range(s);
    return (*this)(r) - r.begin();
  }

  bool is_new_picture() const { return new_pic_flag; }
  bool is_new_slice() const { return !!current_slice; }

  h264::slice_header const& slice() const { return *current_slice; }
  h264::seq_parameter_set const& sps() const { return *params.sps(slice()); }
  h264::pic_parameter_set const& pps() const { return *params.pps(slice()); }
private:
  void on_slice(h264::slice_header&& new_slice) {
    new_pic_flag = !current_slice || are_different_pictures(*current_slice, new_slice);

    if(new_pic_flag) {
      if(this->current_picture()) dec_ref_pic_marking(*this->current_picture(), this->begin(), this->end());
      
      if(new_slice.IdrPicFlag) poc = h264::poc_decoder(*params.sps(new_slice));

      if(poc) {
        this->new_picture(new_slice.IdrPicFlag, new_slice.pic_type, new_slice.frame_num, has_mmco5(new_slice), (*poc)(new_slice));
        dec_ref_pic_marking = h264::dec_ref_pic_marker(*params.sps(new_slice), std::move(new_slice));
      }
    }

    if(this->current_picture()) current_slice = std::move(new_slice);
  }

  h264::parsing_context                     params;
  bool                                      new_pic_flag;
  utils::optional<h264::slice_header>       current_slice;
  utils::optional<h264::poc_decoder>        poc;
  h264::dec_ref_pic_marker                  dec_ref_pic_marking;
};

}

#endif
