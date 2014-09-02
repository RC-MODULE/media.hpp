#ifndef __H264_BITSTREAM_HPP__
#define __H264_BITSTREAM_HPP__

#include <memory>
#include <algorithm>
#include <cassert>
#include <array>
#include <vector>
#include <system_error>

namespace h264 {

class cbitptr {
  std::uint8_t const* p = nullptr;
  int offset = 0;
public:
  cbitptr() noexcept = default;
  cbitptr(std::uint8_t const* p, int offset = 0) noexcept : p(p) {
    *this += offset;
  }
  
  cbitptr(cbitptr const&) noexcept = default;
  cbitptr(cbitptr&&) noexcept = default;

  cbitptr& operator = (cbitptr const&) noexcept = default;
  cbitptr& operator = (cbitptr&&) noexcept = default;

  std::uint8_t const* ptr() const { return p; }
  std::size_t shift() const { return offset; }

  friend bool operator == (cbitptr const& a, cbitptr const& b) {
    return a.p == b.p && a.offset == b.offset;
  }
  
  friend bool operator != (cbitptr const& a, cbitptr const& b) {
    return !(a == b);
  }

  friend bool operator < (cbitptr const& a, cbitptr const& b) {
    if(a.p == b.p) 
      return a.offset < b.offset;

    return a.p < b.p;
  }

  friend bool operator <= (cbitptr const& a, cbitptr const& b) {
    return a < b || a == b;
  }

  friend bool operator >= (cbitptr const& a, cbitptr const& b) {
    return !(a < b);
  }

  friend bool operator > (cbitptr const& a, cbitptr const& b) {
    return !(a <= b);
  }

  cbitptr& operator += (int n) {
    offset += n % 8;
    p += n / 8;

    if(offset < 0) {
      offset += 8;
      p -= 1;
    }

    return *this;
  }

  cbitptr& operator -= (int n) {
    return *this += -n;
  }

  explicit operator bool() const {
    return *this != cbitptr();
  }

  bool operator * () const {
    return ((*p) >> offset) & 1u; 
  }

  bool operator[](std::size_t i) const {
    return *(*this + i);
  }

  cbitptr& operator ++ () {
    return *this += 1;
  }

  cbitptr operator ++ (int) {
    auto t = *this;
    ++(*this);
    return t;
  }

  cbitptr& operator --() {
    return *this -= 1;
  }

  cbitptr operator -- (int) {
    auto t = *this;
    --(*this);
    return *this;
  }

  friend cbitptr operator + (cbitptr const& a, int n) {
    auto t = a;
    t += n;
    return t;
  }

  friend cbitptr operator - (cbitptr const& a, int n) {
    return a + (-n);
  }

  friend std::size_t operator - (cbitptr const& a, cbitptr const& b) {
    return (a.p - b.p) * 8 + (a.offset - b.offset);
  }
};


const auto startcode = {0,0,1};

template<typename A>
inline
bool is_at_startcode(std::reference_wrapper<A>& a) { return is_at_startcode(a.get()); }

template<typename A>
inline
std::uint8_t byte(std::reference_wrapper<A>& a) { return byte(a.get()); }

template<typename A>
inline
bool search(std::reference_wrapper<A>& a, std::initializer_list<std::uint8_t> const& pattern) {
  return search(a.get(), pattern);
}

template<typename A>
inline
void advance(std::reference_wrapper<A>& a, std::size_t n) {
   advance(a.get(), n);
}

template<typename A>
struct Stream {
  template<typename T>
  Stream(T&& t) : a_(std::forward<T>(t)) {}

  bool make_avail(std::size_t n) {
    if(std::size_t(egptr_ - gptr_) >= n) return true;

    std::size_t count = egptr_ - gptr_;

    if(gptr_) data_.erase(data_.begin(), data_.begin() + (gptr_ - &data_[0]));
    data_.resize(std::max(count+4096, n));
    gptr_ = &*data_.begin();
    egptr_ = gptr_ + count;

    while(egptr_ - gptr_ < n && !eof_) { 
      std::size_t r = a_(&*(data_.begin() + count), data_.size() - count);
      eof_ = r == 0;
      egptr_ += r;
    }

    return !eof_; 
  }

  bool advance(std::size_t n) {
    if(n > egptr_ - gptr_) {
      gptr_ += egptr_ - gptr_;
      if(!make_avail(std::max(std::size_t(4096), data_.capacity()))) return false;
      return advance(n - (egptr_ - gptr_));
    }

    gptr_ += n;
    return true; 
  }

  explicit operator bool() const {
    return !eof_;
  }

  A a_;
  std::vector<std::uint8_t> data_;
  std::uint8_t* gptr_ = 0;
  std::uint8_t* egptr_ = 0;
  bool eof_ = false;
};

template<typename A>
inline
Stream<A> make_stream(A&& a) { return Stream<A>(std::forward<A>(a)); }


template<typename A>
inline
std::uint8_t byte(Stream<A>& s) {
  s.make_avail(1);
  auto a = *s.gptr_;
  s.advance(1);
  return a;
}

template<typename A>
inline
bool advance(Stream<A>& a, size_t n) { return a.advance(n); }

template<typename A>
inline
bool search(Stream<A>& s, std::initializer_list<std::uint8_t> const& pattern) {
  for(;;) {
    if(s.make_avail(pattern.size())) {
      s.gptr_ = std::search(s.gptr_, s.egptr_, pattern.begin(), pattern.end());
      if(s.gptr_ != s.egptr_) return true;
    }
    else
      return false;
  }
}

template<typename A>
inline
bool is_at_startcode(Stream<A>& s) {
  if(s.make_avail(3))
    return s.gptr_[0] == 0 && s.gptr_[1] == 0 && s.gptr_[2] == 1;

  return false;
}

template<typename S>
struct Buffer {
  typedef typename S::iterator iterator;

  Buffer() = default;
 
  template<typename T>
  Buffer(T&& t) : storage_(std::forward<T>(t)) {}

  iterator begin() { return gi_; }
  iterator end() { return ei_; }

  iterator storage_begin() { return storage_.begin(); }
  iterator storage_end() { return storage_.end(); }

  iterator free_begin() { return end(); }
  iterator free_end() { return storage_end(); }

  iterator garbage_begin() { return storage_begin(); }
  iterator garbage_end() { return begin(); }

  std::size_t size() { return end() - begin(); }
  std::size_t capacity() { return storage_.end() - storage_.begin(); }
   
  std::size_t garbage() { return begin() - storage_begin(); }
  std::size_t free() { return storage_end() - end(); } 

  void clear_garbage() {
    std::copy(begin(), end(), storage_begin());
    ei_ = storage_begin() + size();
    gi_ = storage_begin();
  }

  void advance_begin(std::size_t n) {
    assert(n <= size());
    gi_ += n;
  }

  void advance_end(std::size_t n) {
    assert(n <= free());
    ei_ += n;
  }
private:
  S storage_;
  iterator gi_ = storage_.begin();
  iterator ei_ = storage_.begin();
};

struct MemStream {
  std::uint8_t const* first;
  std::uint8_t const* second;

  friend std::uint8_t const* get_pos(MemStream const& r) { return r.first; }
};


template<typename A>
struct RemoveStartCodeEmulationPrevention {
  template<typename T>
  RemoveStartCodeEmulationPrevention(T&& a) : a_(std::forward<T>(a)) {}

  std::uint8_t operator()() {
    std::uint8_t b = byte(a_) & 0xFF;
    acc_ = (acc_ << 8) | b;
  
    if((acc_ & 0xFFFFFF) == 0x000003) {
      b = byte(a_) & 0xFF;
      acc_ = (acc_ & 0xFFFFFF00) | b;
    }
  
    return b;
  }

  friend std::uint8_t const* get_pos(RemoveStartCodeEmulationPrevention const& r) { return get_pos(r.a_); }

  A a_;
  std::uint32_t acc_ = -1u;
};

template<typename A>
inline
bool is_at_startcode(RemoveStartCodeEmulationPrevention<A>& a) { return is_at_startcode(a.a_); }

template<typename A>
inline
std::uint8_t byte(RemoveStartCodeEmulationPrevention<A>& a) { return a(); }

template<typename A>
inline
void search_start_code(RemoveStartCodeEmulationPrevention<A>& a) {
  search(a.a_, {0,0,1});
  advance(a.a_, 3);
  a = RemoveStartCodeEmulationPrevention<A>(std::move(a.a_));
}

template<typename A>
struct RBSP {
  template<typename T>
  RBSP(T&& a) : a_(std::forward<T>(a)) {}

  std::uint32_t u(unsigned n) {
    if(n > 24) {
      auto a = u(24) << (n-24);
      return a | u(n - 24);
    }

    assert(n <= 24);
    while(available_bits() < n) read_byte();

    return pop_bits(n);
  }

  std::uint32_t ue() {
    for(;;) {
      unsigned n = __builtin_clz(acc_);
      if(n < available_bits()) {
        pop_bits(n + 1);
        return (1 << n) - 1 + u(n);
      }

      read_byte();
    }
  }

  std::int32_t se() {
    auto k = ue();
    return ((k+1)/2) * ( k & 1 ? 1 : -1);
  }

  bool more_data() {
    while(unused_bits_ >= 8 && !is_at_startcode(a_))
      read_byte();
    return acc_ != 0x80000000; 
  }

  std::uint32_t next_bits(unsigned n) {
    assert(n <= 24);
    while(available_bits() < n) read_byte();
    return acc_ >> (32 - n);
  }

  friend 
  cbitptr get_pos(RBSP const& r) { return {get_pos(r.a_), int(r.available_bits())}; }

//private:
  unsigned available_bits() const { return 32 - unused_bits_; }

  std::uint32_t pop_bits(unsigned n) {
    assert(n <= available_bits());
    if(n == 0) return 0;
    
    auto a = acc_ >> (32 - n);
    acc_ <<= n;
    unused_bits_ += n;
    return a;
  } 
  
  void read_byte() {
    if(unused_bits_ < 8) throw std::runtime_error("NAL parse error");

    acc_ |= (byte(a_) & 0xFF) << (unused_bits_ - 8);
    unused_bits_ -= 8;
  }

  A a_;
  std::uint32_t acc_ = 0;
  int unused_bits_ = 32;
};

template<typename A>
inline
std::uint32_t ue(RBSP<A>& a) { return a.ue(); }

template<typename A>
inline
std::uint32_t u(RBSP<A>& a, unsigned i) { return a.u(i); }

template<typename A>
inline
std::int32_t se(RBSP<A>& a) { return a.se(); }

template<typename A>
inline
bool more_rbsp_data(RBSP<A>& a) { return a.more_data(); }

template<typename A>
inline std::uint32_t next_bits(RBSP<A>& a, unsigned n) { return a.next_bits(n); }

template<typename A>
inline bool next_start_code(RBSP<A>& a) {
  if(u(a, a.available_bits() % 8) != 0) return false;

  while(next_bits(a, 24) != 0x000001)
    if(u(a, 8) != 0) return false;

  return true;  
}

inline
std::uint8_t byte(MemStream& p) { return *p.first++; }

inline bool search(MemStream& p, std::initializer_list<std::uint8_t> const& pattern) {
  auto i = std::search(p.first, p.second, std::begin(pattern), std::end(pattern));

  if(i == p.second) return false;

  p.first = i;
  return true;
} 

inline
void advance(MemStream& p, std::size_t n) { p.first += n; }

inline
bool is_at_startcode(MemStream const& p) { 
  auto startcode = {0,0,1};
  return p.first == p.second || ((static_cast<std::size_t>(p.second - p.first) >= startcode.size()) && std::equal(startcode.begin(), startcode.end(), p.first));
}

template<typename T>
inline
RBSP<RemoveStartCodeEmulationPrevention<typename std::decay<T>::type>> rbsp(T&& a) {
  typedef RemoveStartCodeEmulationPrevention<typename std::decay<T>::type> RSCEP;
  return RBSP<RSCEP>(RSCEP(std::forward<T>(a)));
}

template<typename A>
inline
MemStream read_nalu(Stream<A>& s) {
  auto scode= {0u,0u,1u};

  if(!search(s, {0,0,1})) throw std::runtime_error("can't find startcode in the stream");
  if(is_at_startcode(s)) advance(s, 3);

  auto e = std::search(s.gptr_ + 3, s.egptr_, std::begin(scode), std::end(scode));
  for(std::size_t n = 4096;s && e == s.egptr_; n += std::min(n, std::size_t(32*1024u))) {
    s.make_avail(n);
    e = std::search(s.gptr_ + 3, s.egptr_, std::begin(scode), std::end(scode));
  }
  return {s.gptr_, e};
}

}


#endif

