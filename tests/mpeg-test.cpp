#include "../msvd.hpp"
#include "../mpeg.hpp"
#include "../mvdu.hpp"
#include "../bitstream.hpp"

#include <map>
#include <iostream>

struct sync_read_stream {
  utils::unique_file_descriptor fd;

  template<typename MutableBufferSequence>
  std::size_t read_some(MutableBufferSequence const& seq, std::error_code& ec) {
    asio::detail::buffer_sequence_adapter<asio::mutable_buffer, MutableBufferSequence> adapted(seq);

    auto r = ::readv(fd.get().fd, adapted.buffers(), adapted.count());
    if(r < 0) ec = std::error_code(errno, std::system_category());
    return r;
  }

  template<typename MutableBufferSequence>
  std::size_t read_some(MutableBufferSequence const& seq) {
    std::error_code ec;
    auto r = read_some(seq, ec);
    if(ec) throw std::system_error(ec);
    return r;
  }
};

struct picture_data_start_condition {
  template<typename I>
  std::pair<I, bool> operator()(I first, I last) {
    if(last - first < 4) return std::make_pair(first, false);

    for(;;) {
      first = bitstream::find_startcode_prefix(first, last);
      if(first == last) return std::make_pair(first - 2, false);
      if(last - first < 4) return std::make_pair(first, false); 
      if(first[3] >= mpeg::slice_start_code_begin && first[3] < mpeg::slice_start_code_end) return std::make_pair(first, true);
      first += 3;
    }
  }
};
namespace asio { template<> struct is_match_condition<picture_data_start_condition> : std::true_type {}; };

struct picture_header_start_condition {
  template<typename I>
  std::pair<I, bool> operator()(I first, I last) {
    if(last - first < 4) return std::make_pair(first, false);

    for(;;) {
      first = bitstream::find_startcode_prefix(first, last);
      if(first == last) return std::make_pair(first - 2, false); 
      if(last - first < 4) return std::make_pair(first, false);
      if(first[3] == mpeg::picture_start_code || first[3] == mpeg::sequence_header_code) return std::make_pair(first, true);
      first += 3;
    }
  }
};
namespace asio { template<> struct is_match_condition<picture_header_start_condition> : std::true_type {}; };

using frame_t = std::shared_ptr<mvdu::buffer<int>>;

std::ostream& operator << (std::ostream& os, frame_t const& f) {
  return os << std::dec << "{" << std::dec << f->user_data << "," << std::hex << phys_addr(f) << "}";
}

struct video {
  video(asio::io_service& io) : device(mvdu::open(io)) {}

  mvdu::handle device;

  std::map<int, frame_t> queue;
  int output_temporal_reference = -1;

  void push(frame_t frame) {
    queue[frame->user_data] = frame;
    try_output();
  }

  void flush() {
    for(auto& a: queue) 
      async_render(device, a.second, [&](std::error_code const& ec, frame_t f) { std::cout << "rendered:\t" << f << std::endl;});
    queue.clear();
    output_temporal_reference = -1;
  }

  void try_output() {
    auto n = (output_temporal_reference + 1) % 1024;
    auto i = queue.find(n);
    if(i != queue.end()) {
      async_render(device, i->second,  [&](std::error_code const& ec, frame_t f) { std::cout << "rendered:\t" << f << std::endl; });
      queue.erase(i);
      output_temporal_reference = n;
    }
  }

  template<typename C>
  void async_allocate(int timestamp, C c) {
    mvdu::async_allocate(device, timestamp, c);
  }
};

struct context {
  sync_read_stream stream;
  asio::streambuf buffer;

  msvd::decoder decoder;
  video output;

  std::size_t bytes_to_consume = 0;
 
  mpeg::sequence_header_t sh;
  mpeg::picture_header_t ph;
  utils::optional<mpeg::picture_coding_extension_t> pcx;
  utils::optional<mpeg::quant_matrix_extension_t> qmx;

  frame_t ref[2];

  context(asio::io_service& io, sync_read_stream stream) : stream(std::move(stream)), buffer(1024*1024), decoder(io), output(io) {}
 
  template<typename Condition>
  asio::const_buffers_1 read_until(Condition c) {
    buffer.consume(bytes_to_consume);
    bytes_to_consume = asio::read_until(stream, buffer, c);
    return asio::const_buffers_1(asio::buffer_cast<const void*>(buffer.data()), bytes_to_consume);
  }

  asio::const_buffers_1 read_picture_headers() { return read_until(picture_data_start_condition()); }

  asio::const_buffers_1 read_picture_data() { return read_until(picture_header_start_condition()); }

  template<typename C>
  void process_picture_headers(C const& c) {
    auto parser = bitstream::make_bit_parser(bitstream::make_bit_range(bitstream::make_asio_sequence_range(c)));

    using namespace mpeg;

    while(more_data(parser)) {
      u(parser, 24); // skip startcode prefix
      
      switch(u(parser,8)) {
      case mpeg::sequence_header_code:
        sh = sequence_header(parser);
        set_params(output.device, mvdu::video_mode::hd, {0,0, sh.horizontal_size_value, sh.vertical_size_value}, {0,0,1920,1080}); 
        break;
      case mpeg::picture_start_code:
        ph = picture_header(parser);
        break;
      case mpeg::group_start_code:
        output.flush();
        group_of_pictures_header(parser);
        break;
      case extension_start_code:
        switch(u(parser, 4)) {
        case picture_coding_extension_id:
          pcx = picture_coding_extension(parser);
          break;
        case quant_matrix_extension_id:
          qmx = quant_matrix_extension(parser);
          break;
        default:
          mpeg::unknown_extension(parser);
          break;
        }
        break;
      default:
        mpeg::unknown_high_level_syntax_element(parser);
        break;
      }
    }
  }

  template<typename C>
  void async_decode_stream(C callback) {
    auto headers = read_picture_headers();
    if(asio::buffer_size(headers) == 0) {
      callback(std::error_code());
      return;
    }
    std::cout << "headers:" << asio::buffer_size(headers) << std::endl;
    process_picture_headers(headers);
    //process_picture_headers(read_picture_headers());
  
    output.async_allocate(ph.temporal_reference, [=](std::error_code const& ec, frame_t frame) {
      auto data = read_picture_data();

      if(asio::buffer_size(data) == 0) {
        callback(std::error_code());
        return;
      }

      async_decode_picture(
        decoder,
        sh, 
        ph,
        pcx ? &*pcx : 0, 
        qmx ? &*qmx : 0,
        frame,
        ref[0],
        ref[1],
        data,
        [=](std::error_code const& ec, msvd::decode_result const& result) {
          if(ec)
            callback(ec);
          else {
            output.push(std::move(frame));
        
            if(ph.picture_coding_type != mpeg::picture_coding::B) {
              ref[1] = ref[0];
              ref[0] = frame;
            } 

            async_decode_stream(callback);
          }
        }
      );
    });
  }
};

int main(int argc, char* argv[]) {
  asio::io_service io;

  context cx(io, sync_read_stream{utils::unique_file_descriptor(::open(argv[1], O_RDONLY))});

  cx.async_decode_stream([&](std::error_code const& ec) {
    std::cout << "done:" << ec << std::endl;
  });

//  set_params(cx.output.device, mvdu::video_mode::hd, {0,0,1920,1080}, {0,0,1920, 1080});

  io.run();
}

