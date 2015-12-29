#ifndef __optional_hpp_04616336_07bd_4cee_9eb6_fc406d3685a6__
#define __optional_hpp_04616336_07bd_4cee_9eb6_fc406d3685a6__

#include "base.hpp"
#include "variant.hpp"

//quick and dirty implementation of c++14 std::optional
namespace utils {

struct nullopt_t {};

constexpr nullopt_t nullopt{};

template<typename T>
class optional {
  variant<nullopt_t, T> storage;
public:
  constexpr optional() noexcept {}
  constexpr optional(nullopt_t) noexcept {}

  optional(const optional& r) noexcept(std::is_nothrow_copy_constructible<T>::value) : storage(r.storage) {}
  optional(optional&& r) noexcept(std::is_nothrow_move_constructible<T>::value) : storage(std::move(r.storage)) {}

  constexpr optional(T const& t) noexcept(std::is_nothrow_copy_constructible<T>::value) : storage(t) {}
  constexpr optional(T&& t) noexcept(std::is_nothrow_move_constructible<T>::value) : storage(std::move(t)) {}

//  template<typename... Args>
//  explicit optional(std::in_place_t, Args&& args) : storage(std::integral_constant<1>{}, std::forward<Args>(args)...) {}

  optional& operator = (nullopt_t) noexcept {
    storage = nullopt;
    return *this;
  }

  optional& operator = (optional const& r) noexcept(std::is_nothrow_copy_assignable<T>::value && std::is_nothrow_copy_constructible<T>::value) {
    storage = r.storage;
    return *this;
  }

  optional& operator = (optional&& r) noexcept(std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value) {
    storage = std::move(r.storage);
    return *this;
  }

  template<typename U>
  auto operator = (U&& value) -> typename std::enable_if<std::is_constructible<T,U>::value && std::is_assignable<T,U>::value, optional&>::type {
    storage = T(std::forward<U>(value));
    return *this;
  }

  explicit operator bool () const noexcept { return storage.tag() == 1; }
 
  constexpr T const& value() const { return storage.template get<1>(); }
  T& value() { return storage.template get<1>(); }

  constexpr T const& operator*() const { return value(); }
  constexpr T const* operator->() const { return &value(); }
  
  T& operator*() { return value(); }
  T* operator->() { return &value(); }

  friend bool operator == (optional const& a, optional const& b) noexcept { return a.storage == b.storage; }
  friend bool operator < (optional const& a, optional const& b) noexcept { return a.storage < b.storage; }
  
  friend bool operator == (optional const& a, T const& b) noexcept { return a && a.value() == b; }
  friend bool operator == (T const& a, optional const& b) noexcept { return b == a; }

  friend bool operator < (optional const& a, T const& b) noexcept { return !a || a.value() < b; }
  friend bool operator < (T const& a, optional const& b) noexcept { return b && a < b.value(); }

  friend bool operator == (optional const& a, nullopt_t) noexcept { return !a; }
  friend bool operator == (nullopt_t, optional const& a) noexcept { return !a; }
  
  friend bool operator < (optional const& a, nullopt_t) noexcept { return false; }
  friend bool operator < (nullopt_t, optional const& a) noexcept { return a; }

  template<typename... Args>
  void emplace(Args&&... args) {
    storage = variant<nullopt_t, T>{std::integral_constant<int, 1>{}, std::forward<Args>(args)...};
  }

  template<typename U, typename... Args>
  void emplace(std::initializer_list<U> ilist, Args&&... args) {
    storage = variant<nullopt_t, T>{std::integral_constant<int, 1>{}, ilist, std::forward<Args>(args)...}; 
  }
};

}

#endif
