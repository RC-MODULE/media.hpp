#ifndef __MSVD_HPP_5cab0e5f_3058_498e_857a_a1e6bfea1e98__
#define __MSVD_HPP_5cab0e5f_3058_498e_857a_a1e6bfea1e98__

#include "mpeg.hpp"
#include <asio.hpp>
#include <asio/system_timer.hpp>
#include "utils.hpp"

namespace msvd {

enum class errc : int {
  success,
  unspecified_failure,
  out_of_extmem,
  reflist_is_too_long,
  no_free_frame_id,
  parse_error,
  unexpected_end_of_picture
};

enum class structure { top = 1, bot = 2, frame = 3 };

enum class coding_type { D = 0, I = 1, P = 2, B = 3 };

inline
std::error_category const& error_category() noexcept {
 	static struct : public std::error_category {
		const char* name() const noexcept { return "MSVD"; }
 
    virtual std::string message(int ev) const {
			switch(static_cast<errc>(ev)) {
      case errc::success: return "The operation was successfully completed";
			case errc::out_of_extmem: return "out of external memory";
      default: return "unknown error";
			};
		}
	} cat;
  return cat;
}

inline
std::error_code make_error_code(errc e) { return {int(e), error_category()}; }

struct buffer_geometry {
  std::uint32_t width;
  std::uint32_t height;

  std::uint32_t luma_offset;    
  std::uint32_t chroma_offset;
};

template<typename Buffer>
struct buffer_traits;

struct device {
  class device_impl;
private:  
  device_impl* p;
  
  device_impl* release() {
    auto t = p;
    p = 0;
    return t;
  }

public:
  device() noexcept = default;
  device(device&& r) noexcept : p(r.release()) {}

  device(device_impl* p) : p(p) {}
  
  device& operator = (device&& r) noexcept {
    assert(p == 0);
    p = r.release();
  }

  device_impl* impl() const { return p; }

  ~device();
};

void async_open(asio::io_service& io, std::function<void (std::error_code const&, device)> func);

asio::io_service& get_io_service(device&);
  
void async_decode(
    device d,
    mpeg::sequence_header_t const& sh,
    mpeg::picture_header_t const& ph,
    mpeg::picture_coding_extension_t const* pcx,
    buffer_geometry const& geometry,
    std::uint32_t curpic,
    std::uint32_t refpic1,
    std::uint32_t refpic2,
    utils::range<asio::const_buffer const*> picture_data,
    std::function<void (std::error_code const& ec, device)>);

struct h264_pic_reference {
  structure pt;
  std::size_t index;

  bool long_term;
  std::uint16_t poc_top;
  std::uint16_t poc_bot;
  
  struct {
    std::int8_t weight;
    std::int8_t offset;
  } luma, cb, cr;
};

using scaling_list_4x4_t = std::array<std::array<std::uint8_t, 16>, 6>;
using scaling_list_8x8_t = std::array<std::array<std::uint8_t, 64>, 6>;

struct h264_decode_params {
  buffer_geometry geometry;

  unsigned hor_pic_size_in_mbs;
  unsigned vert_pic_size_in_mbs;
  unsigned mb_mode;
  bool     frame_mbs_only_flag;
  bool     mbaff_frame_flag;
  bool     constr_intra_pred_flag;    
  bool     direct_8x8_inference_flag;
  bool     transform_8x8_mode_flag;
  bool     entropy_coding_mode_flag;
  unsigned weight_mode;  
  int      chroma_qp_index_offset;
  int      second_chroma_qp_index_offset;

  scaling_list_4x4_t scaling_list_4x4;
  scaling_list_8x8_t scaling_list_8x8; 

  structure pic_type;
  coding_type slice_type;

  unsigned first_mb_in_slice;
  unsigned cabac_init_idc;
  unsigned disable_deblocking_filter_idc;
  unsigned slice_qpy; 
  bool     direct_spatial_mv_pred_flag;
  unsigned luma_log2_weight_denom;
  unsigned chroma_log2_weight_denom;

  std::uint32_t curr_pic;
  std::uint16_t poc_top;
  std::uint16_t poc_bot;

  utils::range<std::uint32_t const*> dpb;
  std::array<utils::range<h264_pic_reference const*>,2> reflist;

  structure col_pic_type;
  bool      col_abs_diff_poc_flag; //  topAbsDiffPOC >= bottomAbsDiffPoc
};

void async_decode_slice(
  device d,
  h264_decode_params const& params,
  utils::range<asio::const_buffer const*> buffers,
  std::size_t slice_data_bit_offset,
  std::function<void (std::error_code const& ec, device d, std::size_t num_of_decoded_mbs)> cb);
}
#endif

