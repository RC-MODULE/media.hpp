#ifndef __ALSA_HPP_3d08dfb0_4a74_4ea5_854e_e51e9934ce3c__
#define __ALSA_HPP_3d08dfb0_4a74_4ea5_854e_e51e9934ce3c__

#include <alsa/asoundlib.h>
#include <system_error>
#include <memory>
#include <asio.hpp>

namespace ALSA {

inline
const std::error_category& error_category() noexcept {
	static struct : public std::error_category {
		const char* name() const noexcept { return "ALSA"; }
 
    virtual std::string message(int ev) const {
		  return snd_strerror(ev);
    }
	} cat;
	return cat;
}

inline 
int check(int err) {
  if(err < 0) throw std::system_error(err, error_category());
  return err;
}

namespace Details {

struct PcmWriteBuffer {
  using const_iterator = asio::const_buffer const*;
 
  const_iterator begin() const { return &buffer; }
  auto end() const -> decltype(begin()) { return begin()+1; }

  asio::const_buffer buffer;
  snd_pcm_t* handle;
};

}

}

namespace asio { namespace detail {
template<>
class descriptor_write_op_base<ALSA::Details::PcmWriteBuffer> : public reactor_op {
public:
  descriptor_write_op_base(int descriptor,
      ALSA::Details::PcmWriteBuffer const& buffer, func_type complete_func)
    : reactor_op(&descriptor_write_op_base::do_perform, complete_func),
      buffer(buffer)
  {}

  static bool do_perform(reactor_op* base) {
    descriptor_write_op_base* o(static_cast<descriptor_write_op_base*>(base));

    int r = snd_pcm_writei(o->buffer.handle, asio::buffer_cast<void const*>(o->buffer.buffer), asio::buffer_size(o->buffer.buffer) / 4);
    if(r < 0 && r != -EAGAIN)
      r = snd_pcm_recover(o->buffer.handle, r, 0);
 
    o->ec_ = r < 0 ? std::error_code(-r, ALSA::error_category()) : std::error_code();
    
    if(!o->ec_)
      o->bytes_transferred_ = 2*sizeof(std::int16_t)*r;

    return r != -EAGAIN; 
  }
private:
  ALSA::Details::PcmWriteBuffer buffer;
};
}} // alsa::detail

namespace ALSA {
struct Playback {
  struct Closer {
    void operator()(snd_pcm_t* p) const { snd_pcm_close(p); }
  };

  static Playback open(asio::io_service& io, const char* name) {
    snd_pcm_t* handle;
    check(snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK));
    std::unique_ptr<snd_pcm_t, Closer> h(handle);
    
    pollfd pfd;
    check(snd_pcm_poll_descriptors(handle, &pfd, 1));
    
    return Playback{io, pfd.fd, std::move(h)};
  }

  friend inline void set_params(Playback& pb) {
    snd_pcm_hw_params_t* hw;
    snd_pcm_hw_params_alloca(&hw);
  
    check(snd_pcm_hw_params_any(pb.handle.get(), hw));
    check(snd_pcm_hw_params_set_format(pb.handle.get(), hw, SND_PCM_FORMAT_S16_LE));
    check(snd_pcm_hw_params_set_channels(pb.handle.get(), hw, 2));
    check(snd_pcm_hw_params_set_rate(pb.handle.get(), hw, 48000, 0));
    check(snd_pcm_hw_params_set_access(pb.handle.get(), hw, SND_PCM_ACCESS_RW_INTERLEAVED));
    check(snd_pcm_hw_params(pb.handle.get(), hw));
  }

  template<typename Callback>
  inline
  friend void async_play(Playback& pb, std::pair<int16_t, int16_t>* frames, std::size_t n, Callback cb) {
    pb.fd.async_write_some(Details::PcmWriteBuffer{asio::const_buffer(frames, n*sizeof(*frames)), pb.handle.get()}, 
      [=](std::error_code const& ec, std::size_t bytes) { cb(ec, bytes/sizeof(*frames)); });
  }

  friend std::chrono::duration<snd_pcm_sframes_t, std::ratio<1, 48000>> delay(Playback& pb) {
    snd_pcm_sframes_t d = 0;
    snd_pcm_delay(pb.handle.get(), &d);

    return std::chrono::duration<snd_pcm_sframes_t, std::ratio<1, 48000>>{d};
  }

  ~Playback() { fd.release(); }

  Playback(Playback&&) = default;
  Playback& operator=(Playback&&) = default;
private:
  Playback(asio::io_service& io, int fd, std::unique_ptr<snd_pcm_t, Closer> p) : handle(std::move(p)), fd(io, fd) {}

  std::unique_ptr<snd_pcm_t, Closer> handle;
  asio::posix::stream_descriptor fd;
};

struct Timer {
  struct Closer {
    void operator()(snd_timer_t* p) { snd_timer_close(p); }
  };

  static Timer open(asio::io_service& io, const char* name) {
    snd_timer_t* handle;
    check(snd_timer_open(&handle, name, SND_TIMER_OPEN_NONBLOCK));
    return Timer{io, handle};
  }

  std::unique_ptr<snd_timer_t, Closer> handle;
private:
  Timer(asio::io_service& io, snd_timer_t* handle) : handle(handle) {}
};

}

#endif //__ALSA_HPP_3d08dfb0_4a74_4ea5_854e_e51e9934ce3c__
