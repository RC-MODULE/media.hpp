#include "../msvd.hpp"
#include "../h264-syntax.hpp"
#include "../h264-slice.hpp"
#include "../h264-dpb.hpp"
#include "../mvdu.hpp"

struct startcode_match_condition {
  template<typename I>
  std::pair<I, bool> operator()(I begin, I end) const {
    if(end - begin < bitstream::startcode_length) return std::make_pair(begin, false);
    auto e = bitstream::find_startcode_prefix(begin, end);
    if(e != end) return std::make_pair(e, true);
    return std::make_pair(e - (bitstream::startcode_length - 1), false);
  }
};

template<typename Stream, typename Buffer, typename Condition>
std::size_t read_until(Stream& stream, Buffer& buffer, std::size_t search_position, Condition condition) {
  for(;;) {
    auto d = buffer.data();
    auto r = bitstream::make_asio_sequence_range(d);
    auto m = condition(begin(r) + search_position, end(r));
  
    search_position = m.first - begin(r); 
    if(m.second) return search_position; 
    auto n = stream(buffer.prepare(std::max(4096u, buffer.size())));
    buffer.commit(n);
    if(n == 0) return asio::buffer_size(buffer.data());
  }
}

template<typename Stream, typename Buffer>
auto read_nalu(Stream& stream, Buffer& buffer) -> decltype(bitstream::adjust_sequence(buffer.data(), 0, 0)) {
  auto start = read_until(stream, buffer, 0, startcode_match_condition());
  buffer.consume(start);  
  if(buffer.size() != 0) {
    buffer.consume(bitstream::startcode_length);
    auto end = read_until(stream, buffer, 0, startcode_match_condition());
    return bitstream::adjust_sequence(buffer.data(), 0, end);
  }
  return bitstream::adjust_sequence(buffer.data(), 0, 0);
}

template<typename Stream, typename Buffer>
struct buffered_stream {
  buffered_stream(Stream stream, Buffer buffer) : stream(std::move(stream)), buffer(std::move(buffer)) {}

  Stream stream;
  Buffer buffer;
  std::size_t position = 0;
};

template<typename Stream, typename Buffer>
auto read_nalu(buffered_stream<Stream, Buffer>& s) -> decltype(read_nalu(s.stream, s.buffer)) {
  s.buffer.consume(s.prev_nalu_size);
  auto r = read_nalu(s.stream, s.buffer);
  s.prev_nalu_size = r.size();
  return r;
}

template<typename Stream, typename Buffer>
auto read_nalu(buffered_stream<Stream, std::reference_wrapper<Buffer>>& s) -> decltype(read_nalu(s.stream, s.buffer.get())) {
  s.buffer.get().consume(s.position);
  auto r = read_nalu(s.stream, s.buffer.get());

  s.position = buffer_size(r); 
  return r;
}

template<typename Stream, typename Buffer>
buffered_stream<Stream, Buffer> make_buffered_stream(Stream stream, Buffer buffer) {
  return buffered_stream<Stream, Buffer>{stream, std::move(buffer)};
}

struct display_order_output {
  mvdu::handle device;

  using buffer_type = std::shared_ptr<mvdu::buffer<int>>;

  friend void on_new_picture(display_order_output& output, h264::context<buffer_type>& dpb) {
    using value_type = h264::decoded_picture_buffer<buffer_type>::value_type;

    if(dpb.slice().IdrPicFlag || has_mmco5(dpb.slice())) {
      std::sort(dpb.begin(), dpb.current_picture()->frame, [](value_type const& a, value_type const& b) { return PicOrderCnt(a) < PicOrderCnt(b); });

      for(auto i = dpb.begin(), e = dpb.current_picture()->frame; i != e; ++i)
        async_render(output.device, i->buffer, [](std::error_code const& ec, buffer_type buffer) {});
    
      dpb.erase(dpb.begin(), dpb.current_picture()->frame);
    }

    while(dpb.size() > 16) {
      auto e = std::find_if(dpb.begin(), dpb.current_picture()->frame, [](value_type const& a) { return is_reference(top(a)) && is_reference(bot(a)); }); 
    
      auto i = std::min_element(dpb.begin(), e, [&](value_type const& a, value_type const& b) { 
        return std::make_tuple(PicOrderCnt(a), &a - &dpb.front()) < std::make_tuple(PicOrderCnt(b), &b - &dpb.front()); 
      });
  
      if(i == e) break;
    
      async_render(output.device, i->buffer, [](std::error_code const& ec, buffer_type buffer) {});
      dpb.erase(i);
    }
  }

  template<typename Callback>
  friend void async_allocate(display_order_output& output, Callback cb) { async_allocate(output.device, 0, cb); }
};

template<typename Stream, typename Frame, typename Output, typename Callback>
void async_decode_stream(msvd::decoder& decoder, h264::context<Frame>& cx, Stream& stream, Output& output, Callback cb) {
  for(;;) {
    auto nalu = read_nalu(stream);
    if(buffer_size(nalu) == 0) {
      cb(std::error_code());
      break;
    }
    auto pos = cx(nalu);

    if(cx.is_new_slice()) {
      if(cx.is_new_picture()) on_new_picture(output, cx);

      msvd::async_decode_slice(decoder, output, cx, nalu, pos, [&, cb](std::error_code const& ec, msvd::decode_result const& r) {
        if(ec)
          cb(ec);
        else
          async_decode_stream(decoder, cx, stream, output, cb);
      });
      break;
    }
  }
}

int main(int argc, char* argv[]) {
  int fd = ::open(argv[1], O_RDONLY);
  if(fd < 0) throw std::system_error(errno, std::system_category());

  asio::streambuf buffer(1024*1024);

  auto stream = make_buffered_stream(
    [=](asio::mutable_buffers_1 buffers) {
      auto r = ::read(fd, asio::buffer_cast<void*>(buffers), asio::buffer_size(buffers));
      if(r < 0) throw std::system_error(errno, std::system_category());
      return std::size_t(r);
    },
    std::ref(buffer));
 
  asio::io_service io;

  h264::context<std::shared_ptr<mvdu::buffer<int>>> cx;
  msvd::decoder decoder(io);
  display_order_output output{mvdu::open(io)};
  set_params(output.device, mvdu::video_mode::hd, {0,0,1920,1080}, {0,0,1920, 1080}); 
  
  async_decode_stream(decoder, cx, stream, output, [](std::error_code const& ec) {
    std::cout << "stream decoded with: " << ec << std::endl;
  });

  io.run();
}

