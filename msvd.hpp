#ifndef __MSVD_HPP_5cab0e5f_3058_498e_857a_a1e6bfea1e98__
#define __MSVD_HPP_5cab0e5f_3058_498e_857a_a1e6bfea1e98__

#include "h264.hpp"
#include "mpeg.hpp"
#include <asio.hpp>
#include <asio/system_timer.hpp>
#include "utils.hpp"

namespace h264 = H264;

namespace H264 {
using sequence_parameter_set = SequenceParameterSet;
using picture_parameter_set = PictureParameterSet;
using slice_header = SliceHeader;
using poc = POC;

using picture_type = PicType;
using slice_type = SliceType;

template<typename Buffer>
using picture = Picture<Buffer>;

template<typename Buffer>
using frame = Frame<Buffer>;
}


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

}

#endif

