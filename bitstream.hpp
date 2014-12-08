#ifndef __BITSTREAM_HPP__ec196b53_6264_47d8_9eb7_69c29a436518__
#define __BITSTREAM_HPP__ec196b53_6264_47d8_9eb7_69c29a436518__

#include "utils.hpp"
#include <cstdint>
#include <cassert>
#include <asio.hpp>

namespace bitstream {

template<typename I>
class bit_iterator {
  I pos;
  int offset = 0;

//  static_assert(std::is_integral<typename std::iterator_traits<I>::value_type>::value, "bit_iterator required iterator over integral type");

  static constexpr int width = sizeof(typename std::iterator_traits<I>::value_type) * 8;
public:
  using value_type = bool;
  using difference_type = std::ptrdiff_t;
  using iterator_category = typename std::iterator_traits<I>::iterator_category;

  bit_iterator() noexcept = default;
  bit_iterator(I i, int  offset = 0) noexcept : pos(i), offset(0) {
    *this += offset;
  }

  bit_iterator(bit_iterator const&) noexcept = default;
  bit_iterator(bit_iterator&&) noexcept = default;

  bit_iterator& operator = (bit_iterator const&) noexcept = default;
  bit_iterator& operator = (bit_iterator&&) noexcept = default;
  
  I base() const { return pos; }
  std::size_t shift() const { return offset; }

  //bool operator* () const { return (*pos >> offset) & 0x1; }
  bool operator* () const { return *pos & (0x1 << (width - 1 - offset)); }

  bit_iterator& operator += (int n) {
    n += offset;
    offset = n % width;
    if(offset >= 0)
      std::advance(pos, static_cast<difference_type>(n / width));
    else {
      offset += width;
      std::advance(pos, static_cast<difference_type>(n / width - 1));
    }
    return *this;  
  }

  bit_iterator& operator -= (int n) {
    return *this += -n;
  }

  friend std::ptrdiff_t operator - (bit_iterator const& a, bit_iterator const& b) {
    return (a.pos - b.pos) * width + (a.offset - b.offset);
  }

  friend bool operator == (bit_iterator const& a, bit_iterator const& b) {
    return a.pos == b.pos && a.offset == b.offset;
  }
  
  friend bool operator != (bit_iterator const& a, bit_iterator const& b) {
    return !(a == b);
  }

  friend bool operator < (bit_iterator const& a, bit_iterator const& b) {
    if(a.p == b.p) 
      return a.offset < b.offset;

    return a.p < b.p;
  }

  friend bool operator <= (bit_iterator const& a, bit_iterator const& b) {
    return a < b || a == b;
  }

  friend bool operator >= (bit_iterator const& a, bit_iterator const& b) {
    return !(a < b);
  }

  friend bool operator > (bit_iterator const& a, bit_iterator const& b) {
    return !(a <= b);
  }

  friend bit_iterator operator + (bit_iterator const& a, int n) {
    auto t = a;
    t += n;
    return t;
  }

  friend bit_iterator operator - (bit_iterator const& a, int n) {
    return a + (-n);
  }

  bit_iterator& operator ++ () {
    return *this += 1;
  }

  bit_iterator operator ++ (int) {
    auto t = *this;
    ++(*this);
    return t;
  }

  bit_iterator& operator --() {
    return *this -= 1;
  }

  bit_iterator operator -- (int) {
    auto t = *this;
    --(*this);
    return *this;
  }
};

template<typename I> 
utils::range<bit_iterator<I>> make_bit_range(utils::range<I> const& r) {
  return r;
}

template<typename I>
unsigned u(utils::range<bit_iterator<I>>& r, std::size_t n) {
  unsigned t = 0;
  for(auto i = 0u; i != n && r.begin() != r.end(); ++i, ++r.first)
    t = (t << 1) | *r.first;
  return t;
}

template<typename I>
std::size_t clz(utils::range<bit_iterator<I>>& a) {
  std::size_t n;
  for(n = 0; a.first != a.last; ++a.first, ++n)
    if(*a.first) return n; 
  return n;

  return n;
}

template<typename I>
class bit_parser {
  std::uint32_t accumulator = 0;
  std::size_t unused = 32;
  I pos;

  bit_iterator<I> last;

  friend bool read_byte(bit_parser& bits) {
    assert(bits.unused >= 8);
    if(bits.pos != bits.last.base()) {
      bits.accumulator |= ((*bits.pos++) & 0xFF)  << (bits.unused - 8);
      bits.unused -= 8;
      return true;
    }
    else if(bits.last.shift()) {
      bits.accumulator |= (*bits.pos & 0xFF)  << (bits.unused - 8);
      bits.unused -= bits.last.shift();
      return true;
    }
    return false;
  }

  friend bool read(bit_parser& bits) {
    if(!read_byte(bits)) return false;
    while(bits.unused >= 8 && read_byte(bits));
    return true;
  }

  friend std::size_t available(bit_parser const& bits) { return 32 - bits.unused; }
public:
  bit_parser(bit_iterator<I> const& first, bit_iterator<I> const& last) : pos(first.base()), last(last)  {
    u(*this, first.shift());
  }

  bit_iterator<I> begin() const { return {pos, static_cast<int>(-available(*this))}; }
  bit_iterator<I> end() const { return last; }

  friend std::uint32_t next_bits(bit_parser& bits, std::size_t n) {
    assert(n <= 24);
    if(n == 0) return 0;

    while(available(bits) < n && read(bits));
    return bits.accumulator >> (32 - n);
  }

  friend std::size_t bits_until_byte_aligned(bit_parser const& bits) { return available(bits) % 8; }
  friend bool byte_aligned(bit_parser const& bits) { return available(bits) % 8 == 0; }

  friend std::uint32_t u(bit_parser& bits, std::size_t n) {
    if(n > 24) return (u(bits, n - 24) << 24) | u(bits, 24);
    
    auto t = next_bits(bits, n);
    bits.accumulator <<= n;
    bits.unused += n;

    return t;
  }

  friend std::size_t clz(bit_parser& bits) {
    for(;;) {
      auto n = __builtin_clz(bits.accumulator);
      if(n < available(bits)) {
        u(bits, n);
        return n;
      }

      if(!read(bits)) return available(bits);
    }
  }
};

template<typename I>
bit_parser<I> make_bit_parser(utils::range<bit_iterator<I>> const& range) {
  return {range.begin(), range.end()};
} 

template<typename A>
unsigned ue(A& r) {
  auto n = clz(r);
  u(r, 1);
  return (1 << n) - 1 + u(r, n);
}

template<typename A>
int se(A& r) {
  auto k = ue(r);
  return ((k+1)/2) * ( k & 1 ? 1 : -1);
}

template<typename I>
bool more_data(bit_parser<I> const& r) { return r.end() != r.begin(); }

template<typename I>
bool more_data(utils::range<bit_iterator<I>> const& r) { return r.end() != r.begin(); }

const int startcode_length = 3;

template<typename I>
I find_startcode_prefix(I begin, I end) {
  static const auto sc = {0,0,1};
  return std::search(begin, end, sc.begin(), sc.end());
}

template<typename I>
class remove_startcode_emulation_prevention_iterator {
  I pos;
  uint32_t acc = 0;
public:
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::size_t;
  using value_type = std::uint8_t;
  using pointer = void;
  using reference = void;

  remove_startcode_emulation_prevention_iterator() noexcept {}
  remove_startcode_emulation_prevention_iterator(I p) : pos(p) {}
  
  I base() const { return pos; }

  std::uint8_t operator*() const {
    return *pos;
  }

  remove_startcode_emulation_prevention_iterator& operator++() {
    acc = (acc << 8) | **this;
    ++pos;
    if((acc & 0xFFFFFF) == 0x000003)
      return ++*this;
    return *this;
  }

  remove_startcode_emulation_prevention_iterator operator++(int) {
    auto t = *this;
    ++*this;
    return t;
  }

  friend bool operator == (remove_startcode_emulation_prevention_iterator const& a, remove_startcode_emulation_prevention_iterator const& b) {
    return a.base() == b.base();
  }

  friend bool operator != (remove_startcode_emulation_prevention_iterator const& a, remove_startcode_emulation_prevention_iterator const& b) {
    return !(a == b);
  }
};

template<typename I>
utils::range<remove_startcode_emulation_prevention_iterator<I>> remove_startcode_emulation_prevention(utils::range<I> const& a) {
  return a;
}

template<typename I>
class asio_sequence_iterator : public std::iterator<std::random_access_iterator_tag, const std::uint8_t> {
  I buffer; 
  std::ptrdiff_t offset;
public:
  asio_sequence_iterator(I buffer) : buffer(buffer), offset(0) {}

  std::uint8_t operator*() const {
    assert(offset < asio::buffer_size(*buffer));
    return asio::buffer_cast<std::uint8_t const*>(*buffer)[offset];
  }

  std::uint8_t operator[](std::ptrdiff_t n) {
    return *(*this + n);
  }

  I base() const { return buffer; }
  std::ptrdiff_t position() const { return offset; }
  
  asio_sequence_iterator& operator += (std::ptrdiff_t n) {
    offset += n;
    while(offset < 0) {
      --buffer;
      offset += asio::buffer_size(*buffer);
    }

    while(offset >= asio::buffer_size(*buffer))
      offset -= asio::buffer_size(*buffer++);

    return *this;
  }

  friend bool operator == (asio_sequence_iterator const& a, asio_sequence_iterator const& b) {
    return a.buffer == b.buffer && a.offset == b.offset;
  }

  friend bool operator != (asio_sequence_iterator const& a, asio_sequence_iterator const& b) {
    return !(a==b);
  }

  asio_sequence_iterator& operator++() {
    return *this += 1;
  }

  asio_sequence_iterator& operator--() {
    return *this -= 1;
  }
  
  asio_sequence_iterator operator++(int) {
    auto t = *this;
    *this += 1;
    return t;
  }

  asio_sequence_iterator operator--(int) {
    auto t = *this;
    *this -= 1;
    return t;
  }

  friend asio_sequence_iterator operator + (asio_sequence_iterator a, std::ptrdiff_t n) {
    return a += n; 
  }

  friend asio_sequence_iterator operator - (asio_sequence_iterator const& a, std::ptrdiff_t n) {
    return a + (-n);
  }

  friend asio_sequence_iterator operator + (std::ptrdiff_t n, asio_sequence_iterator const& a) {
    return a + n;
  }
 
  friend asio_sequence_iterator operator - (std::ptrdiff_t n, asio_sequence_iterator const& a) {
    return a - n;
  }

  friend bool operator < (asio_sequence_iterator const& a, asio_sequence_iterator const& b) {
    return a.buffer < b.buffer || (a.buffer == b.buffer && a.offset < b.offset);
  }

  friend bool operator > (asio_sequence_iterator const& a, asio_sequence_iterator const& b) {
    return !((a < b) || a == b);
  }

  friend bool operator <= (asio_sequence_iterator const& a, asio_sequence_iterator const& b) { return !(a > b); }
  friend bool operator >= (asio_sequence_iterator const& a, asio_sequence_iterator const& b) { return !(a < b); }

  friend std::ptrdiff_t operator - (asio_sequence_iterator const& a, asio_sequence_iterator const& b) {
    if(a >= b) {
      std::ptrdiff_t r = -b.position();
      auto i = b.base();
      while(i != a.base()) r += asio::buffer_size(*i++);
      r += a.position();
      return r;
    }
    
    return -(b - a);
  }
};

template<typename I>
asio_sequence_iterator<I> make_asio_sequence_iterator(I i) { return {i}; }

template<typename C>
auto make_asio_sequence_range(C& c) -> decltype(utils::make_range(make_asio_sequence_iterator(c.begin()), make_asio_sequence_iterator(c.end()))) {
  return utils::make_range(make_asio_sequence_iterator(c.begin()), make_asio_sequence_iterator(c.end()));
}

inline
asio::const_buffers_1 adjust_sequence(asio::const_buffers_1 const& buffer, std::size_t offset, std::size_t length) {
  assert(offset+length <= asio::buffer_size(buffer));

  return asio::const_buffers_1(asio::buffer_cast<std::uint8_t const*>(buffer) + offset, length);
}

template<std::size_t N>
std::array<asio::const_buffer, N> adjust_sequence(std::array<asio::const_buffer, N> const& s, std::size_t offset, std::size_t length) {
  assert(offset + length <= asio::buffer_size(s));

  std::array<asio::const_buffer, N> r;

  std::transform(s.begin(), s.end(), r.begin(), 
    [&](asio::const_buffer const& a) {
      auto o = std::min(asio::buffer_size(a), offset);
      auto n = std::min(length, asio::buffer_size(a) - o);
      length -= n;
      offset -= o;
      return asio::const_buffer(asio::buffer_cast<std::uint8_t const*>(a) + o, n);
  });

  return r;
}

inline
std::vector<asio::const_buffer> adjust_sequence(std::vector<asio::const_buffer> s, std::size_t offset, std::size_t length) {
  std::transform(s.begin(), s.end(), s.begin(),
    [&](asio::const_buffer const& a) {
      auto o = std::min(asio::buffer_size(a), offset);
      auto n = std::min(length, asio::buffer_size(a) - o);
      length -= n;
      offset -= o;
      return asio::const_buffer(asio::buffer_cast<std::uint8_t const*>(a) + o, n);
  });

  s.erase(std::remove_if(s.begin(), s.end(), [](asio::const_buffer const& a) { return asio::buffer_size(a) == 0; }), s.end());

  return s;
}

inline 
iovec to_native_buffer(asio::const_buffer const& a) { return {const_cast<char*>(asio::buffer_cast<const char*>(a)), asio::buffer_size(a)}; }

inline
std::array<iovec, 1> adapt_sequence(asio::const_buffers_1 const& seq) {
  return {{to_native_buffer(seq)}};
}

inline
std::array<iovec, 1> adapt_adjusted_sequence(asio_sequence_iterator<asio::const_buffers_1::const_iterator> const& i, asio::const_buffers_1 const& seq) {
  return {{to_native_buffer(*seq.begin() + (i-seq.begin()))}}; 
}

template<std::size_t N>
std::array<iovec, N> adapt_adjusted_sequence(
  asio_sequence_iterator<typename std::array<asio::const_buffer, N>::iterator> const& p,
  std::array<asio::const_buffer, N> const& seq)
{
  std::array<iovec, N> r;
  for(std::size_t i = 0; i != N; ++i) {
    if(i <= p.base() - seq.begin())
      r[i] = to_native_buffer(seq[i] + (p - (seq.begin() + i)));
    else
      r[i] = to_native_buffer(seq[i]);
  }
  return r;
}

template<typename C>
std::vector<iovec> adapt_adjusted_sequence(asio_sequence_iterator<typename C::const_iterator> const& p, C const& seq) {
  std::vector<iovec> r;
  for(auto i = p.base(); i != seq.end(); ++i)
    r.push_back(i == p.base() ? to_native_buffer(*i + (p - i)) : to_native_buffer(*i));
  
  return r;
}

}
#endif
