#ifndef __video_hpp__f11fa058_cb5c_4876_921e_51aa0c06aad7__
#define __video_hpp__f11fa058_cb5c_4876_921e_51aa0c06aad7__

#include <atomic>

#include <asio.hpp>
#include "utils.hpp"

#include <linux/videodev2.h>
#define this __this__f11fa058_cb5c_4876_921e_51aa0c06aad7__
#include <linux/module_vdu.h>
#undef this

namespace media { namespace video {

struct rect {
  int x;
  int y;
  unsigned w;
  unsigned h;
};

struct resolution {
  std::uint32_t width;
  std::uint32_t height;
  
  friend bool operator == (resolution const& a, resolution const& b) { return a.width == b.width && a.height == b. height; }
};

struct aspect_ratio {
  double n;

  friend bool operator == (aspect_ratio const& a, aspect_ratio const& b) { return a.n == b.n; }
};

enum class video_mode { 
  sd_486i30 = MVDU_MODE_SD_486_I_30,
  sd_576i25 = MVDU_MODE_SD_576_I_25,
  hd_480i30 = MVDU_MODE_HD_480_I_30,
  hd_576i25 = MVDU_MODE_HD_576_I_25,
  hd_480p60 = MVDU_MODE_HD_480_P_60,
  hd_576p50 = MVDU_MODE_HD_576_P_50,
  hd_720p60 = MVDU_MODE_HD_720_P_60,
  hd_720p50 = MVDU_MODE_HD_720_P_50,
  hd_1080i30 = MVDU_MODE_HD_1080_I_30,
  hd_1080i25 = MVDU_MODE_HD_1080_I_25,
  hd_1080p30 = MVDU_MODE_HD_1080_P_30,
  hd_1080p25 = MVDU_MODE_HD_1080_P_25,
  sd = sd_576i25,
  hd = hd_1080i25,
};

inline
constexpr bool is_progressive(video_mode vm) { 
  return vm == video_mode::hd_480p60 || vm == video_mode::hd_576p50 || vm == video_mode::hd_720p60 
        || vm == video_mode::hd_720p50 || vm == video_mode::hd_1080p30 || vm == video_mode::hd_1080p25;
}

inline 
constexpr bool is_hd(video_mode vm) {
  return !(vm == video_mode::sd_486i30 || vm == video_mode::sd_576i25);
}

inline 
constexpr unsigned width(video_mode vm) {
  return (video_mode::sd_486i30 == vm || video_mode::sd_576i25 == vm || video_mode::hd_576i25 == vm || 
          video_mode::hd_576p50 == vm || video_mode::hd_480i30 == vm || video_mode::hd_480p60 == vm) ? 720 :
          ((video_mode::hd_720p60 == vm || video_mode::hd_720p50 == vm) ? 1280 : 1920);
}

inline
constexpr unsigned height(video_mode vm) {
  return (video_mode::sd_486i30 == vm) ? 486:
         ((video_mode::sd_576i25 == vm || video_mode::hd_576i25 == vm || video_mode::hd_576p50 == vm) ? 576 :
         ((video_mode::hd_480i30 == vm || video_mode::hd_480p60 == vm) ? 480 :
         ((video_mode::hd_720p60 == vm || video_mode::hd_720p50 == vm) ? 720 :
         ((video_mode::hd_1080i25 == vm || video_mode::hd_1080i25 == vm || video_mode::hd_1080p30 == vm || video_mode::hd_1080p25 == vm) ? 1080 :
          -1))));  
}

inline 
constexpr int framerate(video_mode vm) {
  return (video_mode::sd_486i30 == vm || video_mode::hd_480i30 == vm || video_mode::hd_1080i30 == vm || video_mode::hd_1080p30 == vm) ? 30 :
         ((video_mode::sd_576i25 == vm || video_mode::hd_576i25 == vm || video_mode::hd_1080i25 == vm || video_mode::hd_1080p25 == vm) ? 25 :
         ((video_mode::hd_576p50 == vm || video_mode::hd_720p50 == vm) ? 50 :
         ((video_mode::hd_480p60 == vm || video_mode::hd_720p60 == vm) ? 60 : 
          -1)));
}

inline
constexpr video_mode construct_video_mode(int height, int framerate, bool progressive, bool hd) {
  return height == 1080 ? (framerate == 25 ? (progressive ? video_mode::hd_1080p25 : video_mode::hd_1080i25) :
                                             (progressive ? video_mode::hd_1080p30 : video_mode::hd_1080i30)) :
          (height == 720 ? (framerate == 50 ? video_mode::hd_720p50 : video_mode::hd_720p60) :
          (height == 576 ? (framerate == 25 ? (hd ? video_mode::hd_576i25 : video_mode::sd_576i25) : video_mode::hd_576p50) :
          (height == 480 ? (framerate == 30 ? video_mode::hd_480i30 : video_mode::hd_480p60) :
          (height == 486 ? video_mode::sd_486i30 : 
          video_mode::sd_576i25))));
}

inline
constexpr bool is_valid_video_mode(int _height, int _rate, bool progressive, bool hd) {
  return 
    height(construct_video_mode(_height, _rate, progressive, hd)) == _height 
    && framerate(construct_video_mode(_height, _rate, progressive, hd)) == _rate
    && is_progressive(construct_video_mode(_height, _rate, progressive, hd))
    && is_hd(construct_video_mode(_height, _rate, progressive, hd));
}

inline
constexpr std::initializer_list<std::initializer_list<const char*>> video_modes_names() {
  return  {
          {"486i30"},                 // sd_486i30
          {"sd", "576i25", "576i"},   // sd_576i25
          {"480i30"},                 // hd_480i30
          {"576i25_hd"},              // hd_576i25
          {"480p60"},                 // hd_480p60
          {"576p50", "576p"},         // hd_576p50
          {"720p60"},                 // hd_720p60
          {"720p50", "720p"},         // hd_720p50
          {"1080i30"},                // hd_1080i30
          {"1080i25", "1080i", "hd"}, // hd_1080i25
          {"1080p30"},                // hd_1080p30
          {"1080p25", "1080p"},       // hd_1080p25
          };
}

constexpr std::initializer_list<const char*> names(video_mode vm) {
  return *(video_modes_names().begin() + (int)(vm)-(int)(video_mode::sd_486i30));
}

struct sink {
  sink(asio::io_service& io) : fd(io) {
    fd.assign(::open("/dev/video0", O_RDWR));
    if(!fd.is_open()) throw std::system_error(errno, std::system_category());

    v4l2_requestbuffers request_buffers = {24, V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_MEMORY_MMAP};
    if(ioctl(fd.native_handle(), VIDIOC_REQBUFS, &request_buffers) < 0) throw std::system_error(errno, std::system_category());  

    v4l2_capability cap;
    if(ioctl(fd.native_handle(), VIDIOC_QUERYCAP, &cap) < 0) throw std::system_error(errno, std::system_category());
    base_addr = MVDU_VIDEO_BASE_PHYS(&cap);

    for(auto& a: buffers) {
      a.base = this;
      queue.push(&a);
    }

    int m = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if(ioctl(fd.native_handle(), VIDIOC_STREAMON, &m) < 0) throw std::system_error(errno, std::system_category());
  }

  sink(sink const&) = delete;
  sink& operator=(sink const&) = delete;

  asio::posix::stream_descriptor fd;
  std::size_t base_addr;

  struct buf {
    std::atomic<std::size_t> refs = {0};
    sink* base;
    
    friend void intrusive_ptr_add_ref(buf* b) { ++b->refs; } 
    friend void intrusive_ptr_release(buf* b) {
      if(--(b->refs) == 0) 
        b->base->queue.push(b);
    }
  } buffers[24];
  
  using buffer = utils::intrusive_ptr<buf>;
  
  utils::future_queue<buf*> queue;

  friend utils::shared_future<buffer> pull(sink& s) {
    return s.queue.pop().then([](auto f) { return buffer{f.get()}; }).share();
  }

  friend void push(sink& s, buffer p) {
    v4l2_buffer b = {0};
    b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = p.get() - s.buffers;

    if(ioctl(s.fd.native_handle(), VIDIOC_QBUF, &b)) throw std::system_error(errno, std::system_category());
    intrusive_ptr_add_ref(p.get());

    auto dqbuf = std::make_unique<v4l2_buffer>();
    memset(dqbuf.get(), 0, sizeof(*dqbuf.get()));
    dqbuf->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    dqbuf->memory = V4L2_MEMORY_MMAP;
    auto dqp = dqbuf.get();

    s.fd.async_read_some(utils::make_ioctl_read_buffer<VIDIOC_DQBUF>(dqp), [&s, buffer = utils::move_on_copy(std::move(dqbuf))](std::error_code const& ec, std::size_t) {
      if(!ec) 
        intrusive_ptr_release(s.buffers + +unwrap(buffer)->index);
    });
  }

  friend void push(sink& s, utils::shared_future<buffer> b) {
    b.then([&](auto f) { push(s, b.get()); });
  }

  static constexpr std::uint32_t buffer_size = MVDU_VIDEO_MAX_BUFFER_SIZE; 
  static constexpr std::uint32_t buffer_width = 1920;
  static constexpr std::uint32_t buffer_height = 1088;
  static constexpr std::uint32_t buffer_chroma_offset = 256*120*1088/32*2;

  friend void set_params(sink& s, video_mode vm, rect const& src, rect const& dst) {
  	mvdu_video_params params = {0};

    params.in_mode = is_progressive(vm) ? MVDU_VIDEO_IN_FRAME_TO_P : MVDU_VIDEO_IN_FRAME_TO_I;
    params.out_mode = (int)vm;
    params.pixel_format = MVDU_PIXEL_FORMAT_420;
    params.planes_nr = MVDU_TWO_PLANES;
    params.plane_format = MVDU_PLANE_FORMAT_MACROBLOCK;
    params.color_space =  is_hd(vm) ? MVDU_COLOR_SPACE_HD : MVDU_COLOR_SPACE_SD;
    params.valid = MVDU_VALID_FORMAT | MVDU_VALID_FILTER | MVDU_VALID_IN_MODE | MVDU_VALID_OUT_MODE | MVDU_VALID_SRC | MVDU_VALID_DST;
    params.byte_order = MVDU_VIDEO_BIG_ENDIAN;
    params.use_dropper = 0;

	  params.src.x = src.x;
	  params.src.y = src.y;
	  params.src.w = src.w;
	  params.src.h = src.h;
    params.src.h = std::min(1080, params.src.h);

	  params.dst.x = dst.x;
	  params.dst.y = dst.y;
	  params.dst.w = dst.w;
	  params.dst.h = dst.h;

    MVDU_MACROBLOCKS_PER_LINE_Y(&params) = buffer_width / 16;
    MVDU_MACROBLOCKS_PER_LINE_C(&params) = buffer_width / 16;
    MVDU_OFFSET_C(&params) = buffer_chroma_offset;

	  if(ioctl(s.fd.native_handle(), VIDIOC_S_PARAMS, &params) < 0) throw std::system_error(errno, std::system_category()); 
  }

  video_mode vm = video_mode::hd;

  friend void set_mode(sink& s, video_mode vm) {
    set_params(s, vm, rect{0, 0, width(vm), height(vm)}, rect{0, 0, width(vm), height(vm)}); 
    s.vm = vm;
  }
  
  friend void set_dimensions(sink& s, resolution r, aspect_ratio a) {
    rect dest = {0, 0, width(s.vm), height(s.vm)};
    double picture_aspect_ratio = r.width * a.n / r.height;
    auto w = dest.h * picture_aspect_ratio;
    if(w > dest.w) dest.h /= picture_aspect_ratio;
    
    set_params(s, s.vm, rect{0,0,r.width,r.height}, dest);
  }
};

inline std::uint32_t phys_addr(sink::buffer const& b) { return (b.get() - b->base->buffers) * sink::buffer_size + b->base->base_addr; }
inline std::uint32_t luma_buffer_phys(sink::buffer const& b) { return phys_addr(b); }
inline std::uint32_t chroma_buffer_phys(sink::buffer const& b) { return luma_buffer_phys(b) + sink::buffer_chroma_offset; }

}

namespace msvd {
template<typename T> struct buffer_traits;

template<>
struct buffer_traits<video::sink::buffer> {
  static constexpr std::uint32_t width = video::sink::buffer_width;
  static constexpr std::uint32_t height = video::sink::buffer_height;
  static constexpr std::uint32_t luma_offset = 0;
  static constexpr std::uint32_t chroma_offset = video::sink::buffer_chroma_offset;
};

}}


#endif
