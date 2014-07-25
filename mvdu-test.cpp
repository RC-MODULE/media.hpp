#include <asio.hpp>
#include <chrono>

#include "mvdu.hpp"

int n = 0;
auto t1 = std::chrono::system_clock::now();

void loop(mvdu::handle& video) {
  async_allocate(video, n++, [&](std::error_code const& ec, std::shared_ptr<mvdu::buffer<int>> buffer) {
    memset(luma_buffer(buffer), buffer->user_data & 0xFF, 1920*1080);
    std::cout << "queued: " << buffer->user_data << ", " << ec << ", " << (std::chrono::system_clock::now() - t1) / std::chrono::milliseconds(1) << std::endl;
    async_render(video, buffer, [&](std::error_code const& ec,  std::shared_ptr<mvdu::buffer<int>> buffer) {
      std::cout << "rendered: " << buffer->user_data << ", " << ec << ", " << (std::chrono::system_clock::now() - t1) / std::chrono::milliseconds(1) << std::endl;
      loop(video);
    });
  });
}

int main(int argc, char* argv[]) {
  asio::io_service io;

  auto video = mvdu::open(io);

  set_params(video, mvdu::video_mode::hd);

  loop(video);  
  
  io.run();
}
