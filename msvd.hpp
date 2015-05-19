#ifndef __MSVD_HPP_5cab0e5f_3058_498e_857a_a1e6bfea1e98__
#define __MSVD_HPP_5cab0e5f_3058_498e_857a_a1e6bfea1e98__

#include "mpeg.hpp"
#include "h264-syntax.hpp"
#include "h264-slice.hpp"
#include "h264-context.hpp"
#include "bitstream.hpp"
#include <asio.hpp>
#include <asio/system_timer.hpp>
#include "utils.hpp"
#include "utils/utils/asio_utils.hpp"
#include <linux/msvdhd.h>
#include "video.hpp"

namespace media {
namespace msvd {

struct decode_result {
  std::size_t num_of_decoded_macroblocks;
};

template<typename Buffer>
struct buffer_traits;

template<typename U>
struct buffer_traits<utils::shared_future<U>> {
  static constexpr std::uint32_t width = buffer_traits<U>::width;
  static constexpr std::uint32_t height = buffer_traits<U>::height;
  static constexpr std::uint32_t luma_offset = buffer_traits<U>::luma_offset;
  static constexpr std::uint32_t chroma_offset = buffer_traits<U>::chroma_offset;
};

struct decoder {
  decoder(asio::io_service& io) : fd(io) {
    fd.assign(::open("/dev/msvdhd", O_RDWR));
  }
  
  decoder(decoder const&) = delete;
  decoder& operator=(decoder const&) = delete;
  
  asio::posix::stream_descriptor fd;
};

//mpeg decoding impl
namespace detail {

template<typename fb>
auto phys_addr(utils::shared_future<fb> const& f) -> decltype(phys_addr(f.get())) {
  return 0;
}

msvd_coding_type to_msvd(mpeg::picture_coding t) {
  switch(t) {
  case mpeg::picture_coding::P: return msvd_coding_type_P;
  case mpeg::picture_coding::B: return msvd_coding_type_B;
  case mpeg::picture_coding::I: return msvd_coding_type_I;
  default: throw std::logic_error("unsupported coding type");
  }
}

msvd_picture_type to_msvd(mpeg::picture_type p) {
  return static_cast<msvd_picture_type>(static_cast<unsigned>(p));
}

template<typename Buffer, typename Sequence>
struct mpeg_context {
  mpeg_context(
    mpeg::sequence_header_t const& sh,
    mpeg::picture_header_t const& ph,
    mpeg::picture_coding_extension_t const* pcx,
    mpeg::quant_matrix_extension_t const* qmx,
    Buffer curpic,
    Buffer ref0,
    Buffer ref1,
    Sequence const& coded_picture_data)
  {
    using namespace mpeg;

    params.geometry = {
      buffer_traits<Buffer>::width,
      buffer_traits<Buffer>::height,
      buffer_traits<Buffer>::luma_offset,
      buffer_traits<Buffer>::chroma_offset
    };

    params.hor_pic_size_in_mbs = sh.horizontal_size_value / 16;
    params.ver_pic_size_in_mbs = sh.vertical_size_value / 16;

    auto set_intra_quantiser_matrix = [this](quantiser_matrix_t const& m) {
      std::copy(begin(m), end(m), intra_quantiser_matrix.data);
      params.intra_quantiser_matrix = &intra_quantiser_matrix;
    };

    auto set_non_intra_quantiser_matrix = [this](quantiser_matrix_t const& m) {
      std::copy(begin(m), end(m), non_intra_quantiser_matrix.data);
      params.non_intra_quantiser_matrix = &non_intra_quantiser_matrix;
    };

    params.intra_quantiser_matrix = params.non_intra_quantiser_matrix = nullptr;
    if(sh.load_intra_quantiser_matrix) set_intra_quantiser_matrix(sh.intra_quantiser_matrix);
    if(sh.load_non_intra_quantiser_matrix) set_non_intra_quantiser_matrix(sh.non_intra_quantiser_matrix);
  
    params.mpeg2 = false;
    params.picture_coding_type = to_msvd(ph.picture_coding_type);  
    params.full_pel_forward_vector = ph.full_pel_forward_vector;
    params.forward_f_code = ph.forward_f_code;
    params.full_pel_backward_vector = ph.full_pel_backward_vector;
    params.backward_f_code = ph.backward_f_code;

    if(pcx) {
      params.mpeg2 = true;
      params.f_code[0][0]         = pcx->f_code[0][0];
      params.f_code[0][1]         = pcx->f_code[0][1];
      params.f_code[1][0]         = pcx->f_code[1][0];
      params.f_code[1][1]         = pcx->f_code[1][1];
      params.intra_dc_precision   = pcx->intra_dc_precision;
      params.picture_structure    = to_msvd(pcx->picture_structure);
      params.top_field_first      = pcx->top_field_first;
      params.frame_pred_frame_dct = pcx->frame_pred_frame_dct;
      params.concealment_motion_vectors = pcx->concealment_motion_vectors;
      params.q_scale_type         = pcx->q_scale_type;
      params.intra_vlc_format     = pcx->intra_vlc_format;
      params.alternate_scan       = pcx->alternate_scan;
      params.progressive_frame    = pcx->progressive_frame;
    }

    if(qmx && qmx->load_intra_quantiser_matrix) set_intra_quantiser_matrix(qmx->intra_quantiser_matrix);
    if(qmx && qmx->load_non_intra_quantiser_matrix) set_non_intra_quantiser_matrix(qmx->non_intra_quantiser_matrix);

    params.curr_pic = phys_addr(curpic);
    curpic = curpic;

    if(params.picture_coding_type == msvd_coding_type_P) {
      refs[0] = ref0;
      params.refpic1 = ref0 ? phys_addr(ref0) : 0;
      params.refpic2 = 0;
    }

    if(params.picture_coding_type == msvd_coding_type_B) {
      refs[0] = ref0;
      refs[1] = ref1;

      params.refpic1 = ref0 ? phys_addr(ref0) : 0;
      params.refpic2 = ref1 ? phys_addr(ref1) : 0;
    }

    buffers = bitstream::adapt_sequence(coded_picture_data);
    params.slice_data = &*buffers.begin();
    params.slice_data_n = buffers.size();
  }

  mpeg_context(mpeg_context const&) = delete;
  mpeg_context& operator=(mpeg_context const&) = delete;

  msvd_mpeg_decode_params params;
  msvd_mpeg_quantiser_matrix intra_quantiser_matrix;
  msvd_mpeg_quantiser_matrix non_intra_quantiser_matrix;
  Buffer refs[2];
  Buffer curr;
  decltype(bitstream::adapt_sequence(std::declval<Sequence>())) buffers;
  msvd_decode_result result; 
};

template<typename B, typename S, typename F>
auto async_decode_picture(decoder& d, std::unique_ptr<mpeg_context<B, S>> cx, F callback) ->
  typename std::enable_if<utils::is_callable<F(std::error_code, msvd::decode_result)>::value>::type
{
  auto mc = utils::move_on_copy(std::move(cx));
  auto cb = utils::move_on_copy(std::move(callback));
   
  d.fd.async_write_some(utils::make_ioctl_write_buffer<MSVD_DECODE_MPEG_FRAME>(std::ref(unwrap(mc)->params)),
    [=,&d](std::error_code const& ec, std::size_t bytes) mutable {
      if(ec)
        cb(ec, msvd::decode_result{0});
      else
        d.fd.async_read_some(asio::mutable_buffers_1(&unwrap(mc)->result, sizeof(unwrap(mc)->result)),
          [cb, mc](std::error_code const& ec, std::size_t bytes) mutable {
            cb(ec, decode_result{unwrap(mc)->result.num_of_decoded_mbs});
          });
    });
}

} // detail

template<typename Buffer, typename Sequence, typename F>
auto async_decode_picture(
  decoder& d,
  mpeg::sequence_header_t const& sh,
  mpeg::picture_header_t const& ph,
  mpeg::picture_coding_extension_t const* pcx,
  mpeg::quant_matrix_extension_t const* qmx,
  Buffer curpic,
  Buffer ref0,
  Buffer ref1,
  Sequence const& coded_picture_data,
  F callback) -> typename std::enable_if<utils::is_callable<F(std::error_code, msvd::decode_result)>::value>::type
{
  async_decode_picture(
    d, 
    std::unique_ptr<detail::mpeg_context<Buffer, Sequence>>(new detail::mpeg_context<Buffer, Sequence>(sh,ph,pcx,qmx,curpic, ref0, ref1, coded_picture_data)),
    callback);
}


namespace detail {

msvd_picture_type to_msvd(h264::picture_type p) {
  switch(p) {
  case h264::picture_type::top: return msvd_picture_type_top;
  case h264::picture_type::bot: return msvd_picture_type_bot;
  default:
  case h264::picture_type::frame: return msvd_picture_type_frame;
  }
}

msvd_coding_type to_msvd(h264::coding_type p) {
  switch(p) {
  case h264::coding_type::P: return msvd_coding_type_P;
  case h264::coding_type::B: return msvd_coding_type_B;
  case h264::coding_type::I: return msvd_coding_type_I;
  default: throw std::runtime_error("unsupported coding_type"); 
  }
}

template<typename Sequence> 
struct h264_context : public msvd_h264_decode_params, public msvd_decode_result {
  template<typename I, typename Frame>
  h264_context(
    h264::seq_parameter_set const& sps,
    h264::pic_parameter_set const& pps,
    h264::slice_header const& slice,
    I frames_begin, I frames_end,
    Frame const& curr_pic,
    Sequence const& slice_data,
    std::size_t slice_data_offset)
  {
    using namespace h264;

    memset(this, 0, sizeof(*this));

    using frame_buffer_type = typename std::decay<decltype(frame_buffer(curr_pic))>::type;

    geometry = {
      buffer_traits<frame_buffer_type>::width,
      buffer_traits<frame_buffer_type>::height,
      buffer_traits<frame_buffer_type>::luma_offset,
      buffer_traits<frame_buffer_type>::chroma_offset
    };

    decoded_picture_buffer_size = std::transform(frames_begin, frames_end, dpb_data, 
      [&](decltype(*frames_begin)& f) { 
        if(is_reference(top(f)) || is_reference(bot(f)))
          return msvd_h264_frame{phys_addr(frame_buffer(f)), (std::int16_t)TopFieldOrderCnt(f), (std::int16_t)BotFieldOrderCnt(f)};
        else
          return msvd_h264_frame{phys_addr(frame_buffer(curr_pic)), (std::int16_t)TopFieldOrderCnt(curr_pic), (std::int16_t)BotFieldOrderCnt(curr_pic)}; 
      }) - dpb_data;
    decoded_picture_buffer = dpb_data;

    std::array<picture_reference<I>, 32> rl[2];
    picture_reference<I>* rl_end[2] = {rl[0].begin(), rl[1].begin()};

    if(slice.slice_type == coding_type::P) {
      rl_end[0] = generate_reflist_for_p_slice(MaxFrameNum(sps), curr_pic, utils::make_range(frames_begin, frames_end),
        utils::make_range(slice.ref_pic_list_modification[0]),
        rl[0].begin(), slice.num_ref_idx_l0_active_minus1 + 1);
    }
    else if(slice.slice_type == coding_type::B) {
      std::tie(rl_end[0], rl_end[1]) = 
        generate_reflists_for_b_slice(MaxFrameNum(sps), curr_pic, utils::make_range(frames_begin, frames_end),
          utils::make_range(slice.ref_pic_list_modification[0]), 
          utils::make_range(slice.ref_pic_list_modification[1]),
          rl[0].begin(), slice.num_ref_idx_l0_active_minus1 + 1,
          rl[1].begin(), slice.num_ref_idx_l1_active_minus1 + 1);
    }

    for(auto i = 0; i != 2; ++i) {
      reflist[i].size = rl_end[i] - rl[i].begin();
      reflist[i].data = reflist_data[i];

      auto to_msvd_reference = [&](picture_reference<I> const& r, slice_header::weight_pred_table_element const& w) {
        return msvd_h264_picture_reference{
          to_msvd(pic_type(r)), std::size_t(r.frame - frames_begin), is_long_term_reference(r),
          {w.luma.weight, w.luma.offset},
          {w.cb.weight, w.cb.offset},
          {w.cr.weight, w.cr.offset}
        }; 
      }; 

      if(!slice.weight_pred_table[i].empty())
        std::transform(rl[i].begin(), rl_end[i],  slice.weight_pred_table[i].begin(), reflist_data[i], to_msvd_reference);
      else
        std::transform(rl[i].begin(), rl_end[i], reflist_data[i], [&](picture_reference<I> const& r) {
          return to_msvd_reference(r, {
            {std::int8_t(1 << slice.luma_log2_weight_denom), 0},
            {std::int8_t(1 << slice.chroma_log2_weight_denom), 0},
            {std::int8_t(1 << slice.chroma_log2_weight_denom), 0} 
          }); 
        });
    } 
  
    col_pic_type = msvd_picture_type_frame; 
    if(rl_end[1] != rl[1].begin()) {
      if(field_flag(*rl[1][0].frame))
        col_pic_type = /*(rl[1][0].pt == h264::picture_type::frame) ? msvd_picture_type_top :*/ to_msvd(rl[1][0].pt);
      
      col_abs_diff_poc_flag = abs(TopFieldOrderCnt(*rl[1][0].frame) - PicOrderCnt(curr_pic)) >= abs(BotFieldOrderCnt(*rl[1][0].frame) - PicOrderCnt(curr_pic));
    }
  
    hor_pic_size_in_mbs            = sps.pic_width_in_mbs_minus1 + 1;
    vert_pic_size_in_mbs           = (sps.pic_height_in_map_units_minus1 + 1) * (2 - sps.frame_mbs_only_flag);
    mb_mode                        = sps.chroma_format_idc == 1 ? 0 : 1;
    frame_mbs_only_flag            = sps.frame_mbs_only_flag;
    mbaff_frame_flag               = sps.mb_adaptive_frame_field_flag;
    direct_8x8_inference_flag      = sps.direct_8x8_inference_flag;
    max_num_ref_frames             = sps.max_num_ref_frames;

    if(pps.scaling_matrix) 
      scaling_list                 = reinterpret_cast<msvd_h264_scaling_lists const*>(&*pps.scaling_matrix);
    else if(sps.scaling_matrix)
      scaling_list                 = reinterpret_cast<msvd_h264_scaling_lists const*>(&*sps.scaling_matrix);
    else
      scaling_list = 0;
  
    constr_intra_pred_flag         = pps.constrained_intra_pred_flag;    
    transform_8x8_mode_flag        = pps.transform_8x8_mode_flag;
    entropy_coding_mode_flag       = pps.entropy_coding_mode_flag;
    weight_mode                    = slice.slice_type == coding_type::B ? pps.weighted_bipred_idc : pps.weighted_pred_flag;
    chroma_qp_index_offset         = pps.chroma_qp_index_offset;
    second_chroma_qp_index_offset  = pps.second_chroma_qp_index_offset;

    picture_type                   = to_msvd(slice.pic_type);
    slice_type                     = to_msvd(slice.slice_type);

    first_mb_in_slice              = slice.first_mb_in_slice;
    cabac_init_idc                 = slice.cabac_init_idc;
    disable_deblocking_filter_idc  = slice.disable_deblocking_filter_idc;
    slice_qpy                      = 26 + pps.pic_init_qp_minus26 + slice.slice_qp_delta;
    direct_spatial_mv_pred_flag    = slice.direct_spatial_mv_pred_flag;
    luma_log2_weight_denom         = slice.luma_log2_weight_denom;
    chroma_log2_weight_denom       = slice.chroma_log2_weight_denom;

    this->curr_pic                 = {phys_addr(frame_buffer(curr_pic)), (std::int16_t)TopFieldOrderCnt(curr_pic), (std::int16_t)BotFieldOrderCnt(curr_pic)}; 
    
    buffers = bitstream::adapt_sequence(slice_data); 
    this->slice_data = &*buffers.begin();
    slice_data_n = buffers.size();
    this->slice_data_offset = slice_data_offset;
  }

  msvd_h264_frame dpb_data[17];
  msvd_h264_picture_reference reflist_data[2][32];
  decltype(bitstream::adapt_sequence(std::declval<Sequence>())) buffers;
};

template<typename Pr, typename Rs, typename F>
auto async_decode_slice(decoder& d, Pr pr, Rs rs, F callback) -> std::enable_if_t<
  std::is_convertible<decltype(*pr), msvd_h264_decode_params>::value
  && std::is_convertible<decltype(*rs), msvd_decode_result>::value
  && utils::is_callable<F(std::error_code)>::value>
{
  msvd_h264_decode_params const* params = &*pr;
  msvd_decode_result* result = &*rs;
  auto mpr = utils::move_on_copy(std::move(pr));
  auto cb = utils::move_on_copy(callback);
  auto r = utils::move_on_copy(rs);
  
  d.fd.async_write_some(
    utils::make_ioctl_write_buffer<MSVD_DECODE_H264_SLICE>(std::ref(*params)),
    [=, &d](std::error_code const& ec, std::size_t) mutable {
      if(ec)
        cb(ec);
       else
        d.fd.async_read_some(asio::mutable_buffers_1(static_cast<msvd_decode_result*>(&*rs), sizeof(msvd_decode_result)),
          [r, cb](std::error_code const ec, std::size_t) mutable {
            cb(ec);
          });
    });
}

template<typename S, typename F>
auto async_decode_slice(decoder& d, std::unique_ptr<h264_context<S>> cx, F callback) ->
  typename std::enable_if<utils::is_callable<F(std::error_code, msvd::decode_result)>::value>::type
{
  auto r = cx.get();
  async_decode_slice(d, r, r, [=, r = std::move(cx)](std::error_code const& ec) mutable {
    callback(ec, decode_result{r->num_of_decoded_mbs});
  });
} 

} // namespace detail

template<typename C, typename I, typename Sequence, typename F>
auto async_decode_slice(
  decoder& d,
  h264::seq_parameter_set const& sps,
  h264::pic_parameter_set const& pps,
  h264::slice_header const& header,
  C const& curr_pic,
  I dpb_begin, I dpb_end,
  Sequence const& slice_data,
  std::size_t slice_data_offset,
  F callback) -> typename std::enable_if<utils::is_callable<F(std::error_code, msvd::decode_result)>::value>::type
{
  detail::async_decode_slice(
    d, 
    std::unique_ptr<detail::h264_context<Sequence>>(
      new detail::h264_context<Sequence>(sps, pps, header, dpb_begin, dpb_end, curr_pic, slice_data, slice_data_offset)
    ),
    callback);
}

template<typename FrameBuffer, typename Sequence, typename Callback>
auto async_decode_slice(
  decoder& d,
  h264::context<FrameBuffer> const& cx,
  Sequence const& slice_data,
  std::size_t slice_data_offset,
  Callback callback) -> typename std::enable_if<utils::is_callable<Callback(std::error_code, msvd::decode_result)>::value>::type
{
  async_decode_slice(d, cx.sps(), cx.pps(), cx.slice(), *cx.current_picture(), cx.begin(), cx.end(), 
    slice_data, slice_data_offset, callback);
}

template<typename FrameBuffer, typename FrameBufferAllocator, typename Sequence, typename Callback>
auto async_decode_slice(
  decoder& d,
  FrameBufferAllocator& allocator,
  h264::context<FrameBuffer>& cx,
  Sequence const& slice_data,
  std::size_t slice_data_offset,
  Callback callback) -> typename std::enable_if<utils::is_callable<Callback(std::error_code, msvd::decode_result)>::value>::type 
{
  if(!frame_buffer(*cx.current_picture()->frame)) {
    async_allocate(allocator, [=, &d, &cx](std::error_code const& ec, FrameBuffer buffer) {
      frame_buffer(*cx.current_picture()->frame, std::move(buffer));
      async_decode_slice(d, cx, slice_data, slice_data_offset, callback);
    });
  }
  else
    async_decode_slice(d, cx, slice_data, slice_data_offset, callback);
}

} //namespace msvd
} //namespace media

#endif

