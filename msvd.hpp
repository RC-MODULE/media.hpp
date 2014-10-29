#ifndef __MSVD_HPP_5cab0e5f_3058_498e_857a_a1e6bfea1e98__
#define __MSVD_HPP_5cab0e5f_3058_498e_857a_a1e6bfea1e98__

#include "mpeg.hpp"
#include "bitstream.hpp"
#include <asio.hpp>
#include <asio/system_timer.hpp>
#include "utils.hpp"
#include "utils/utils/asio_ioctl.hpp"
#include <linux/msvdhd.h>
#include "mvdu.hpp"

namespace msvd {

struct decode_result {
  std::size_t num_of_decoded_macroblocks;
};

template<typename Buffer>
struct buffer_traits;

template<typename U>
struct buffer_traits<std::shared_ptr<mvdu::buffer<U>>> {
  static constexpr std::uint32_t width = mvdu::buffer_width;
  static constexpr std::uint32_t height = mvdu::buffer_height;
  static constexpr std::uint32_t luma_offset = 0;
  static constexpr std::uint32_t chroma_offset = mvdu::buffer_chroma_offset;
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
    mpeg::picture_coding_extension_t* pcx,
    mpeg::quant_matrix_extension_t* qmx,
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
  auto ctx = cx.get();
  auto mc = utils::move_on_copy(std::move(cx));

  utils::async_write_some(d.fd, utils::make_ioctl_write_buffer<MSVD_DECODE_MPEG_FRAME>(std::ref(ctx->params)))
  >> [ctx,&d](std::size_t) { 
    return utils::async_read_some(d.fd, asio::mutable_buffers_1(&ctx->result, sizeof(ctx->result)));
  }
  >> [mc](std::size_t bytes) {
    return decode_result{unwrap(mc)->result.num_of_decoded_mbs};
  }
  += callback;
}

} // detail

template<typename Buffer, typename Sequence, typename F>
auto async_decode_picture(
  decoder& d,
  mpeg::sequence_header_t const& sh,
  mpeg::picture_header_t const& ph,
  mpeg::picture_coding_extension_t* pcx,
  mpeg::quant_matrix_extension_t* qmx,
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

} // namespace msvd

#endif

