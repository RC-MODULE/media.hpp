#ifndef __clock_hpp__0480d667_1120_4559_a912_88d061b8669e__
#define __clock_hpp__0480d667_1120_4559_a912_88d061b8669e__

#include <utility>
#include <chrono>
#include <vector>
#include <algorithm>
#include <functional>

namespace media {

struct system_clock {
  system_clock(asio::io_service& io) : timer(io, std::chrono::system_clock::time_point::max()) {}

  void on_timer(std::error_code const& ec) {
    if(!ec) {
      while(!agenda.empty() && now(*this) >= agenda.front().first) {
        auto f = std::move(agenda.front().second);
        agenda.erase(agenda.begin());
        f();
      }

      set_timer();
    }
  }

  auto to_time_point(timestamp const& ts) const {
    return *epoch + std::chrono::duration_cast<std::chrono::system_clock::duration>(ts);
  }
  
  void set_timer(asio::system_timer::time_point t) {
    timer.expires_at(t);
    timer.async_wait([this](std::error_code const& ec) { on_timer(ec); });
  }

  void set_timer() {
    if(epoch && !agenda.empty() && (to_time_point(agenda.front().first) > timer.expires_at() || to_time_point(agenda.front().first + 10ms) < timer.expires_at()))
      set_timer(to_time_point(agenda.front().first));
  }

  template<typename C>
  friend void schedule(system_clock& clk, timestamp ts, C c) {
    std::cout << "clock now:" << now(clk).count() << ", ts" << ts.count() << std::endl; 
    auto v = std::make_pair(ts, std::function<void()>(utils::move_on_copy(std::move(c))));
    clk.agenda.insert(std::lower_bound(clk.agenda.begin(), clk.agenda.end(), v, [](auto& a, auto& b) { return a.first < b.first; }), std::move(v));
    
    clk.set_timer();
  }
  
  friend void sync(system_clock& clk, timestamp ts) {
    std::cout << "clock sync: " << ts.count() << std::endl;
    clk.epoch = std::chrono::system_clock::now() - std::chrono::duration_cast<std::chrono::system_clock::duration>(ts);
    clk.set_timer();
  }

  friend timestamp now(system_clock const& clk) {
    if(!clk.epoch) return timestamp::min();
    return std::chrono::duration_cast<timestamp>(std::chrono::system_clock::now() - *clk.epoch);
  }

  utils::optional<std::chrono::system_clock::time_point> epoch;
  std::vector<std::pair<timestamp, std::function<void()>>> agenda;
  asio::system_timer timer;
};

}
#endif
