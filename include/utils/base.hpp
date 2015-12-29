#ifndef __UTILS_HPP_f5ff3add_c5fa_40c1_a713_4a8bd5169bd8__
#define __UTILS_HPP_f5ff3add_c5fa_40c1_a713_4a8bd5169bd8__

#include <unistd.h>
#include <fcntl.h>
#include <mutex>
#include <iostream>
#include <tuple>
#include <memory>
#include <algorithm>

namespace utils {

template<typename T>
using decayed_type = typename std::decay<T>::type;

struct identity {
  template<typename A>
  auto operator()(A&& a) -> typename std::decay<A>::type { return std::forward<A>(a); }
};

template<typename... Args>
struct parameter_pack_tail_type;

template<typename A>
struct parameter_pack_tail_type<A> {
  using type = A;
};

template<typename H, typename... T>
struct parameter_pack_tail_type<H, T...> {
  using type = typename parameter_pack_tail_type<T...>::type;
};

template<typename T>
auto parameter_pack_tail(T&& t) -> decltype(std::forward<T>(t)) {
  return std::forward<T>(t);
}

template<typename H, typename... T>
typename parameter_pack_tail_type<T...>::type&& parameter_pack_tail(H&&, T&&... tail) {
  return parameter_pack_tail(std::forward<T>(tail)...);
}

template<typename T, T... I>
struct integer_sequence {
  typedef T value_type;

  static constexpr size_t size() noexcept { return sizeof...(I); }
};
 
template<std::size_t ... I>
using index_sequence = integer_sequence<std::size_t, I...>;

template<typename T, std::size_t N, T ... S>
struct make_integer_sequence_impl : make_integer_sequence_impl<T, N-1, N-1, S...> {};

template<typename T, T ... S>
struct make_integer_sequence_impl<T, 0, S...> {
  using type = integer_sequence<T, S...>;
};

template<class T, T N>
using make_integer_sequence = typename  make_integer_sequence_impl<T, N>::type;

template<size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

template<class... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

template<int... S>
struct indices {};

template<int N, int ... S>
struct generate_indices : generate_indices<N-1, N-1, S...> {};

template<int ... S>
struct generate_indices<0, S...> {
  static indices<S...> value;
};

template<typename F, typename... Args, int... S>
inline auto call_with_tuple(F f, std::tuple<Args...> const& tuple, indices<S...>) -> decltype(f(std::get<S>(tuple)...)) {
  return f(std::get<S>(tuple)...);
} 
  
template<typename F, typename... Args, int... S>
inline auto call_with_tuple(F f, std::tuple<Args...>& tuple, indices<S...>) -> decltype(f(std::get<S>(tuple)...)) {
  return f(std::get<S>(tuple)...);
} 
  
template<typename F, typename... Args, int... S>
inline auto call_with_tuple(F f, std::tuple<Args...>&& tuple, indices<S...>) -> decltype(f(std::move(std::get<S>(tuple))...)) {
  return f(std::move(std::get<S>(tuple))...);
}

template<typename F, typename... Args, int... S>
inline auto call_with_tuple(F f, std::tuple<Args...> const& tuple) -> decltype(call_with_tuple(f, tuple, generate_indices<sizeof...(Args)>::value)) {
  return call_with_tuple(f, tuple, generate_indices<sizeof...(Args)>::value);
} 

template<typename F, typename... Args, int... S>
inline auto call_with_tuple(F f, std::tuple<Args...>& tuple) -> decltype(call_with_tuple(f, tuple, generate_indices<sizeof...(Args)>::value)) {
  return call_with_tuple(f, tuple, generate_indices<sizeof...(Args)>::value);
}   
  
template<typename F, typename... Args, int... S>
inline auto call_with_tuple(F f, std::tuple<Args...>&& tuple) -> decltype(call_with_tuple(f, std::move(tuple), generate_indices<sizeof...(Args)>::value)) {
  return call_with_tuple(f, std::move(tuple), generate_indices<sizeof...(Args)>::value);
} 

struct is_callable_helper {
  template<typename F, typename ... Args>
  static auto __test(int) -> decltype(std::declval<F>()(std::declval<Args>()...), std::true_type());
  
  template<typename...>
  static auto __test(...) -> std::false_type;
};

template<typename F, typename... Args>
struct is_callable;

template<typename F, typename... Args>
struct is_callable<F(Args...)> : decltype(is_callable_helper::__test<F, Args...>(0)) {};

template<typename F>
struct is_callable<F(void)> : decltype(is_callable_helper::__test<F>(0)) {};


template<template<typename> class Predicate, typename... Types>
struct is_all_predicate;

template<template<typename> class Predicate>
struct is_all_predicate<Predicate> : std::true_type {};

template<template<typename> class Predicate, typename H, typename... Tail>
struct is_all_predicate<Predicate, H, Tail...> : std::integral_constant<bool, Predicate<H>::value && is_all_predicate<Predicate, Tail...>::value> {};


template<typename I>
struct range {
  I first;
  I last;

  using iterator = I;
  using const_iterator = I;

  range() = default;

  range(I first, I last) : first(first), last(last) {}
 
  template<typename I2>
  range(range<I2> const& r2) : first(r2.first), last(r2.last) {}

  template<typename I2>
  range& operator=(range<I2> const& r2) {
    first = r2.first;
    last = r2.last;
    return *this;
  }

  I begin() const { return first; }
  I end() const { return last; }

  friend 
  inline I begin(range<I> const& r) { return r.begin(); }
  
  friend
  inline I end(range<I> const& r) { return r.end(); }

  std::size_t size() const { return end() - begin(); }
  bool empty() const { return end() == begin(); }

  auto operator[](std::size_t i) const -> decltype(*first) { return begin()[i]; }
  auto operator[](std::size_t i) -> decltype(*first) { return begin()[i]; }
};

template<typename I>
constexpr range<I> make_range(I first, I last) { return {first, last}; }

template<typename C>
constexpr auto make_range(C&& c) -> decltype(make_range(c.begin(), c.end())) { return make_range(c.begin(), c.end()); }

template<typename I>
range<I> make_range(I first, std::size_t n) {
  auto t = first;
  std::advance(t, n);
  return make_range(first, t);
}
 
// file descriptor wrapper conforming to NullablePointer concept
struct file_descriptor {
  int fd = -1;

  file_descriptor() {}
  file_descriptor(std::nullptr_t) {}
  file_descriptor(int fd) : fd(fd) {}
  
  explicit operator int() const { return fd; }

  friend bool operator == (file_descriptor a, file_descriptor b) { return a.fd == b.fd; }
  friend bool operator != (file_descriptor a, file_descriptor b) { return !(a == b); }
};

struct file_descriptor_deleter {
  constexpr file_descriptor_deleter() noexcept = default;

  using pointer = file_descriptor;

  void operator()(file_descriptor fd) const {
    ::close(int(fd));
  }
};

using unique_file_descriptor = std::unique_ptr<int, file_descriptor_deleter>;

struct less {
  template<typename T>
  bool operator()(T const& a, T const& b) const { return a < b; }
};

template<typename KeyExtractor, typename Compare>
struct compare_by_key {
  KeyExtractor key_extractor;
  Compare comp;

  template<typename Key, typename Value>
  auto operator()(Key const& key, Value const& value) const -> decltype(comp(key,key_extractor(value))) { return comp(key,key_extractor(value)); }
  
  template<typename Key, typename Value>
  auto operator()(Value const& value, Key const& key) const -> decltype(comp(key_extractor(value),key)) { return comp(key_extractor(value),key); }

  template<typename Value>
  auto operator()(Value const& a, Value const& b) const -> bool { return comp(key_extractor(a),key_extractor(b)); }
};

template<typename KeyExtractor, typename Compare>
inline
constexpr compare_by_key<KeyExtractor, Compare> make_compare_by_key(KeyExtractor key_extractor, Compare comp = less()) {
  return {std::move(key_extractor), std::move(comp)};
}

template<std::size_t Start, std::size_t Width=1>
struct bit_field {
  static constexpr std::size_t start = Start;
  static constexpr std::size_t width = Width;
};

// a bit of c++11 madness, i.e bit_field literals
template<char... c> struct bit_field_literal_type;

template<char c1, char c2, char c3, char c4>
struct bit_field_literal_type<c1, c2, '.', c3, c4> {
  using type = bit_field<(c1-'0')*10+(c2-'0'), (c3-'0')*10+(c4-'0')>;
  static constexpr type value = type{};
};

template<char c1, char c2>
struct bit_field_literal_type<c1, '.', c2> : bit_field_literal_type<'0',c1,'.','0',c2> {};

template<char c1, char c2, char c3>
struct bit_field_literal_type<c1, '.', c2, c3> : bit_field_literal_type<'0',c1,'.',c2,c3> {};

template<char c1, char c2, char c3>
struct bit_field_literal_type<c1, c2, '.', c3> : bit_field_literal_type<c1,c2,'.','0',c3> {};

template<char c1>
struct bit_field_literal_type<c1> : bit_field_literal_type<'0',c1,'.','0','1'> {};

template<char c1, char c2>
struct bit_field_literal_type<c1,c2> : bit_field_literal_type<c1,c2,'.','0','1'> {};

template<char... c>
constexpr auto operator "" _bf () -> typename bit_field_literal_type<c...>::type { return bit_field_literal_type<c...>::value; }

template<std::size_t I, std::size_t W>
constexpr std::uint32_t get(bit_field<I, W> f, std::uint32_t v) {
  return (v >> (32-(I+W))) & ((1 << W) - 1);
}

constexpr std::uint32_t get(bit_field<0,32> f, std::uint32_t v) { return v; }

template<std::size_t I, std::size_t W>
constexpr std::uint32_t set(std::uint32_t r, bit_field<I, W> f, std::uint32_t v) {
  return (r & (((1 << W)-1) << I)) | ((v & ((1 << W)-1)) << I);
}

template<std::size_t I, std::size_t W>
constexpr std::uint32_t set(bit_field<I, W> f, std::uint32_t v) {
  return set(0, f, v);
}

template<typename Int>
struct network_byte_order { Int v; };

using net32_t = network_byte_order<std::uint32_t>;

inline constexpr std::uint32_t to_host_order(net32_t v) {
  return (v.v & 0xFF) << 24 | (((v.v >> 8) & 0xFF) << 16) | (((v.v >> 16) & 0xFF) << 8) | (v.v >> 24);
}

template<std::size_t I, std::size_t W>
constexpr std::uint32_t get(bit_field<I, W> f, net32_t v) {
  return get(f, to_host_order(v));
}

template<typename T>
struct move_on_copy_wrapper {
  mutable T value;

  explicit move_on_copy_wrapper(T&& t): value(std::move(t)) {}

  move_on_copy_wrapper(move_on_copy_wrapper const& other): value(std::move(other.value)) {}

  move_on_copy_wrapper& operator=(move_on_copy_wrapper const& other) {
    value = std::move(other.value);
  }

  template<typename... Args>
  auto operator()(Args&&... args) -> decltype(value(std::forward<Args>(args)...)) {
    return value(std::forward<Args>(args)...);
  }

  friend T& unwrap(move_on_copy_wrapper& m) { return m.value; }
  friend T const& unwrap(move_on_copy_wrapper const& m) { return m.value; }
};

template<typename T>
inline move_on_copy_wrapper<decayed_type<T>> move_on_copy(T&& t) {
  return move_on_copy_wrapper<decayed_type<T>>(std::move(t));
}

template<typename B, typename A>
A* container_of(B* b, B A::* p) {
  return reinterpret_cast<A*>(((char*)b) - ((char*)&(reinterpret_cast<A*>(0)->*p) - (char*)(0)));
}

template<typename B, typename A>
A const* container_of(B const* b, B A::* p) {
  return reinterpret_cast<A const*>(((char*)b) - ((char*)&((A*)(0)->*p) - (char*)(0)));
}

// rearranges sequence [first, last) to alternatingly satisfy pred. 
// i.e pred(first) == true, pred(first+1) == false, pred(first+2) == true etc
template<typename I, typename Pred>
I stable_alternate(I first, I last, Pred pred) {
  bool bit = true;
  auto i = first;
  while(first != last) {
    i = bit ? std::find_if(i, last, pred) : std::find_if_not(first, last, pred);
    if(i == last) return first;
    
    if(i != first) {
      auto t = i++;
      std::rotate(first, t, i);
      std::advance(first, 2);
    }
    else {
      ++first;
      ++i;  
      bit = !bit;
    }
  }

  return first;
}

// helper for starting lambda base recursions
template<typename Func>
void recursion(Func func) {
  func(
    [=]() {
      recursion(func);
    }
  );
}


}

#endif
