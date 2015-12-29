#include <media.hpp>

using namespace std::literals::chrono_literals;

int main(int argc, char* argv[]) {
  asio::io_service io;
  
  media::system_clock clk{io};
  
  media::video::sink sink{io};
  set_mode(sink, media::video::video_mode::hd);

  media::h264::decoder<decltype(std::ref(sink)), decltype(make_clocked_sink(std::ref(clk), std::ref(sink)))> decoder{
    io, std::ref(sink), make_clocked_sink(std::ref(clk), std::ref(sink))
  };

  auto ts_reader = [=](asio::mutable_buffer const& m) {
    auto n = ::read(0, asio::buffer_cast<void*>(m), asio::buffer_size(m));
    if(n < 0) throw std::system_error(errno, std::system_category());
    return n;
  };
  
  auto video_pid = (unsigned)atoi(argv[1]);
  auto dmx = media::mpeg::ts::make_demuxer(ts_reader, video_pid);

  media::timestamp ts;
  
  pull(dmx, video_pid).then([&](auto f) {
    auto p = f.get();
    ts = pts(p);
    push(decoder, ts, utils::tag<media::h264::annexb::access_unit_tag>(data(std::move(p))));
    
    ts -= 400ms;
    sync(clk, ts);

    utils::recursion([&](auto next) {
      schedule(clk, ts += 40ms, [&, next]() {
        pull(dmx, video_pid).then([&, next](auto f) {
          auto p = f.get();
          if(!empty(p)) {
            auto ts = pts(p);
            push(decoder, ts, utils::tag<media::h264::annexb::access_unit_tag>(data(std::move(p))));
            next();
          }
        });
      });
    });
  });

  io.run();
  std::cout << "done" << std::endl;
}

