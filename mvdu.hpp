#ifndef __mvdu_hpp_ce57ef29_28e7_4cca_bee9_a5f185346bb2__
#define __mvdu_hpp_ce57ef29_28e7_4cca_bee9_a5f185346bb2__

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#define this _this
#include <linux/module_vdu.h>
#undef this

#include <list>
#include <functional>

#include <asio.hpp>

#include "utils.hpp"

namespace mvdu {

struct device {
  asio::posix::stream_descriptor fd;
  asio::strand strand;

  std::uint32_t free_buffers;

  std::list<std::function<void(std::size_t)>> pending_alloc_rqs;
  utils::polymorphic_async_op<utils::expected<void, std::error_code>> async_deque_op;

  std::uint32_t phys_addr;
  void* virt_addr;

  void free_buffer(std::size_t i) {
    strand.dispatch([=] {
      if(!pending_alloc_rqs.empty()) {
        auto m = std::move(pending_alloc_rqs.front());
        pending_alloc_rqs.pop_front();
        m(i);
      }
      else
        free_buffers |= (1 << i);
    });
  }

  template<typename F>
  void async_allocate(F func) {
    strand.dispatch([=] () mutable {
      if(free_buffers == 0)
        pending_alloc_rqs.emplace_back(std::move(func));
      else {
        auto i = __builtin_ctz(free_buffers);
        free_buffers &= ~(1 << i);
        func(i);
      }
    });
  }
};

using handle = std::unique_ptr<device>;

inline
handle open(asio::io_service& io) {
  asio::posix::stream_descriptor fd(io, ::open("/dev/video0", O_RDWR));
  if(!fd.is_open()) throw std::system_error(errno, std::system_category());

  v4l2_requestbuffers request_buffers = {24, V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_MEMORY_MMAP};
  if(ioctl(fd.native_handle(), VIDIOC_REQBUFS, &request_buffers) < 0) throw std::system_error(errno, std::system_category());  

  v4l2_capability cap;
  if(ioctl(fd.native_handle(), VIDIOC_QUERYCAP, &cap) < 0) throw std::system_error(errno, std::system_category());

  handle h(new device{std::move(fd), asio::strand(io)});

  h->phys_addr = MVDU_VIDEO_BASE_PHYS(&cap);
  h->virt_addr = ::mmap(0, 24 * MVDU_VIDEO_MAX_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, h->fd.native_handle(), 0);
  if(h->virt_addr == MAP_FAILED) throw std::system_error(errno, std::system_category());
 
  h->free_buffers = (1 << 24) - 1;

  h->async_deque_op = utils::make_async_value(utils::make_expected<std::error_code>());

  int m = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if(ioctl(h->fd.native_handle(), VIDIOC_STREAMON, &m) < 0) throw std::system_error(errno, std::system_category());
 
  return h;
}

constexpr std::uint32_t buffer_size = MVDU_VIDEO_MAX_BUFFER_SIZE; 
constexpr std::uint32_t buffer_width = 1920;
constexpr std::uint32_t buffer_height = 1088;
constexpr std::uint32_t buffer_chroma_offset = 256*120*1088/32*2;
 
template<typename U>
class buffer {
public:
  device& d;
  std::uint32_t index;
  buffer(device& d, std::uint32_t index, U&& user_data) : d(d), index(index), user_data(std::move(user_data)) {}

  buffer(const buffer&) = delete;
  buffer(buffer&&) = delete;
  ~buffer() { d.free_buffer(index); }

  U user_data;

  friend std::uint32_t phys_addr(std::shared_ptr<buffer<U>> const& p) {
    return p->d.phys_addr + buffer_size * p->index;
  }

  friend void* luma_buffer(std::shared_ptr<buffer<U>> const& p) {
    return reinterpret_cast<std::uint8_t*>(p->d.virt_addr) + buffer_size * p->index;
  }

  friend std::uint32_t luma_buffer_phys(std::shared_ptr<buffer<U>> const& p) {
    return phys_addr(p);
  }

  friend void* chroma_buffer(std::shared_ptr<buffer<U>> const& p) {
    return luma_buffer(p) + buffer_chroma_offset;
  }

  friend std::uint32_t chroma_buffer_phys(std::shared_ptr<buffer<U>> const& p) {
    return luma_buffer_phys(p) + buffer_chroma_offset;
  }
};

template<typename U, typename F>
auto async_allocate(handle const& h, U udata, F func) -> typename std::enable_if<
    utils::is_callable<F(std::error_code, std::shared_ptr<buffer<U>>)>::value
  >::type
{
  auto d = h.get();
  h->async_allocate([=](std::uint32_t i) mutable {
    func(std::error_code(), std::make_shared<buffer<U>>(*d, i, std::move(udata)));
  });
}

struct dequeue_buffers {
  dequeue_buffers(device* d) : d(d), b(d, sizeof(v4l2_buffer)) {}
  using const_iterator = asio::mutable_buffer const*;
 
  const_iterator begin() const { return &b; }
  auto end() const -> decltype(begin()) { return begin()+1; }

  device* d;
  asio::mutable_buffer b;
};

} // namespace mvdu

namespace asio { namespace detail { 
template<>
class descriptor_read_op_base<mvdu::dequeue_buffers> : public ::asio::detail::reactor_op {
public:
  descriptor_read_op_base(int descriptor,
      mvdu::dequeue_buffers const& buffer, func_type complete_func)
    : reactor_op(&descriptor_read_op_base::do_perform, complete_func),
      descriptor_(descriptor),
      buffer_(buffer)
  {}

  static bool do_perform(reactor_op* base) {
    using namespace asio;
    using namespace asio::detail;

    descriptor_read_op_base* o(static_cast<descriptor_read_op_base*>(base));

    v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buffer.memory = V4L2_MEMORY_MMAP;
    
    o->ec_ = asio::error_code();
    
    if(ioctl(o->descriptor_, VIDIOC_DQBUF, &buffer)) o->ec_ = asio::error_code(errno, asio::error::get_system_category());
     
    if(!o->ec_)
      o->bytes_transferred_ = sizeof(v4l2_buffer);
    
    return o->ec_ != error::would_block; 
  }
private:
  int descriptor_;
  mvdu::dequeue_buffers buffer_;
};

}} // detail } asio }

namespace mvdu {
template<typename U, typename F>
auto async_render(handle const& h, std::shared_ptr<buffer<U>> buf, F func) -> typename std::enable_if<
    utils::is_callable<F(std::error_code, std::shared_ptr<buffer<U>>)>::value
  >::type
{
  auto* d = h.get();

  h->strand.post([=]() mutable {
    v4l2_buffer b = {0};
    b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = buf->index;

    if(ioctl(d->fd.native_handle(), VIDIOC_QBUF, &b) < 0) {
      func(std::error_code(errno, std::system_category()), std::move(buf));
      return;
    }

    auto ado = std::move(d->async_deque_op);
    d->async_deque_op =
      utils::async_read_some(d->fd, dequeue_buffers{d}) >> [](std::size_t) {};
/*
      >> [=](std::size_t) {
        v4l2_buffer b = {};
        b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        b.memory = V4L2_MEMORY_MMAP;

        if(ioctl(d->fd.native_handle(), VIDIOC_DQBUF, &b) < 0) throw std::system_error(errno, std::system_category());      
      };
*/
    d->strand.post([=] {
    std::move(ado)
    += [=](std::error_code const& ec) mutable {
      d->strand.post(std::bind(func, ec, std::move(buf)));
    };
    });
  });
}

struct rect {
  int x;
  int y;
  int w;
  int h;
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
constexpr int width(video_mode vm) {
  return (video_mode::sd_486i30 == vm || video_mode::sd_576i25 == vm || video_mode::hd_576i25 == vm || 
          video_mode::hd_576p50 == vm || video_mode::hd_480i30 == vm || video_mode::hd_480p60 == vm) ? 720 :
          ((video_mode::hd_720p60 == vm || video_mode::hd_720p50 == vm) ? 1280 : 1920);
}

inline
constexpr int height(video_mode vm) {
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

inline
void set_params(handle const& h,  video_mode vm, rect const& src, rect const& dst) {
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

	if(ioctl(h->fd.native_handle(), VIDIOC_S_PARAMS, &params) < 0) throw std::system_error(errno, std::system_category()); 
}

inline
void set_params(handle const& h,  video_mode vm) {
  set_params(h, vm, rect{0, 0, width(vm), height(vm)}, rect{0, 0, width(vm), height(vm)});
}

} // mvdu

#endif
