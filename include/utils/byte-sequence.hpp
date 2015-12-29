#ifndef __byte_sequence_hpp_6a060cf4_7ac1_41e1_875b_1216cac4f442__
#define __byte_sequence_hpp_6a060cf4_7ac1_41e1_875b_1216cac4f442__

#include <utility>
#include <type_traits>
#include <iterator>
#include <asio.hpp>

#include "base.hpp"

namespace asio {

inline auto subsequence(const_buffers_1 const& s, std::size_t offset, std::size_t length) {
  assert(offset + length <= buffer_size(s));
  return asio::const_buffers_1(buffer_cast<const std::uint8_t*>(s) + offset, length);
}

template<typename AsioConstBufferSequence, typename I>
I copy_buffer_sequence(AsioConstBufferSequence const& s, I output) {
  for(auto const& b: s)
    output = std::copy_n(buffer_cast<const std::uint8_t*>(b), buffer_size(b), output);

  return output;
}

template<typename AsioConstBufferSequence>
void push_back_buffer_sequence(std::vector<std::uint8_t>& output, AsioConstBufferSequence const& buffers) {
  auto n = output.size();
  output.resize(n + buffer_size(buffers));
  copy_buffer_sequence(buffers, output.begin() + n);
}

struct is_const_buffer_sequence_helper {
  template<typename T>
  static std::false_type check(...);

  template<typename T>
  static std::enable_if_t<std::is_convertible<decltype(*std::declval<T>().begin()), const_buffer>::value, std::true_type> check(int);  
};

template<typename T>
using is_const_buffer_sequence = decltype(is_const_buffer_sequence_helper::check<T>(0));

}
 
namespace utils {

/*
  ByteSequence 
    begin(ByteSequence const&) -> Iterator<std::uint8_t>
    end(ByteSequence const&) -> Iterator<std::uint8_t>

    as_asio_sequence(ByteSequence const&)

    split(ByteSeqence&& bs, std::size_t split_point) -> std::pair<ByteSequence1, ByteSequence2>
*/

asio::const_buffers_1 as_asio_sequence(std::vector<std::uint8_t> const& v);

struct is_byte_sequence_helper {
  template<typename T>
  static auto check(...) -> std::false_type;

  template<typename T>
  static auto check(int) -> std::enable_if_t<true
    && std::is_convertible<decltype(*begin(std::declval<T>())), std::uint8_t>::value 
    && std::is_convertible<decltype(*begin(std::declval<T>())), std::uint8_t>::value
    && std::is_convertible<decltype(*as_asio_sequence(std::declval<T>()).begin()), asio::const_buffer>::value
  ,std::true_type>;
};

template<typename T>
using is_byte_sequence = decltype(is_byte_sequence_helper::check<T>(0));

template<typename Data> 
struct subsequence {
  Data data;

  using iterator = std::decay_t<decltype(begin(data))>;
  iterator first;
  iterator last;

  friend iterator begin(subsequence const& s) { return s.first; }
  friend iterator end(subsequence const& s) { return s.last; }

/* GCC bug 59766 
  friend auto as_asio_sequence(subsequence const& s) {
    return asio::subsequence(as_asio_sequence(s.data), std::distance(begin(s.data), s.first), std::distance(s.first, s.last));
  }
*/

  friend std::pair<subsequence, subsequence> split(subsequence s, iterator i) {
    return {{s.data, s.first, i}, {s.data, i, s.last}};
  }
};

template<typename Data>
auto as_asio_sequence(subsequence<Data> const& s) {
  return asio::subsequence(as_asio_sequence(s.data), std::distance(begin(s.data), s.first), std::distance(s.first, s.last));
}

inline
asio::const_buffers_1 as_asio_sequence(std::vector<std::uint8_t> const& v) { return asio::const_buffers_1(&v[0], v.size()); }

struct shared_vector {
  std::shared_ptr<std::vector<std::uint8_t>> p;

  /*GCC bug 59766
  friend auto begin(shared_vector const& v) { return v.p->begin(); }
  friend auto end(shared_vector const& v) { return v.p->end(); }

  friend auto as_asio_sequence(shared_vector const& v) { return as_asio_sequence(*v.p); } 
    
  friend auto split(shared_vector&& v, std::vector<std::uint8_t>::iterator i) {
    auto e = end(v);
    return std::make_pair(subsequence<shared_vector>{v, begin(v), i,}, subsequence<shared_vector>{std::move(v), i, e}); 
  }
  */
};

inline auto begin(shared_vector const& v) { return v.p->cbegin(); }
inline auto end(shared_vector const& v) { return v.p->cend(); }

inline auto as_asio_sequence(shared_vector const& v) { return as_asio_sequence(*v.p); } 
    
inline auto split(shared_vector&& v, std::vector<std::uint8_t>::const_iterator i) {
  auto e = end(v);
  subsequence<shared_vector> s1{v, begin(v), i};
  return std::make_pair(s1, subsequence<shared_vector>{std::move(v), i, e}); 
}
 
inline
auto split(std::vector<std::uint8_t>&& v, std::vector<std::uint8_t>::const_iterator i) {
  return split(shared_vector{std::make_shared<std::vector<std::uint8_t>>(std::move(v))}, i);
}

//utils::range<std::uint8_t const*> byte_sequence_interface
inline asio::const_buffers_1 as_asio_sequence(utils::range<const std::uint8_t*> const& r) {
  return asio::const_buffers_1{begin(r), r.size()};
}

inline
std::pair<utils::range<const std::uint8_t*>, utils::range<const std::uint8_t*>> split(utils::range<const std::uint8_t*> r, const uint8_t* p) {
  assert(begin(r) <= p && p <= end(r));
  return std::make_pair(utils::make_range(begin(r), p), utils::make_range(p, end(r)));
}

template<typename Tag, typename ByteSequence>
struct tagged_byte_sequence {
  ByteSequence sequence;

  /* GCC buf 59766
  friend auto begin(tagged_sequence const& s) { return begin(s.sequence); }
  friend auto end(tagged_sequence const& s) { return end(s.sequence); }

  friend auto as_asio_sequence(tagged_sequence const& s) { return as_asio_sequence(s.sequence); }
  friend auto split(tagged_sequence&& s) { return split(std::move(s.sequence)); }
  */
};

template<typename Tag, typename BS> auto begin(tagged_byte_sequence<Tag, BS> const& s) { return begin(s.sequence); }
template<typename Tag, typename BS> auto end(tagged_byte_sequence<Tag, BS> const& s) { return end(s.sequence); }

template<typename Tag, typename BS> auto as_asio_sequence(tagged_byte_sequence<Tag, BS> const& s) { return as_asio_sequence(s.sequence); }
template<typename Tag, typename BS> auto split(tagged_byte_sequence<Tag, BS>&& s, decltype(begin(std::declval<BS>())) i) { return split(std::move(s.sequence), i); }

template<typename Tag, typename ByteSequence>
std::enable_if_t<is_byte_sequence<std::decay_t<ByteSequence>>::value,tagged_byte_sequence<Tag, std::decay_t<ByteSequence>>> tag(ByteSequence&& b) {
  return {std::forward<ByteSequence>(b)};  
}

template<typename BS>
auto size(BS const& bs) -> std::enable_if_t<is_byte_sequence<BS>::value, std::size_t> {
  return std::distance(begin(bs), end(bs));
}

template<typename BS>
auto empty(BS const& bs) -> std::enable_if_t<is_byte_sequence<BS>::value, bool> {
  return size(bs) == 0;
}

}

#endif

