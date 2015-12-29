#ifndef __expected_7abb722e_2790_4d55_8539_9557eff4749d_hpp_
#define __expected_7abb722e_2790_4d55_8539_9557eff4749d_hpp_

// partial implementation of N4015 proposal for C++ standard library

#include "base.hpp"
#include "variant.hpp"

namespace utils {

template<typename T, typename E>
class expected {
  variant<T, E> storage;
public:
  using value_type = T;
  using error_type = E;

  expected() noexcept {}
  expected(expected&&) = default;
  expected(expected const&) = default;

  expected(T&& t) noexcept(noexcept(T(std::move(t)))) : storage(std::move(t)) {}
  expected(T const& t) noexcept(noexcept(T(t))) : storage(t) {}

  expected(E&& e) noexcept(noexcept(E(std::move(e)))) : storage(std::move(e)) {}
  expected(E const& e) noexcept(noexcept(E(e))) : storage(e) {}

  expected& operator = (expected&&) = default;
  expected& operator = (expected const&) = default;

  expected& operator = (T&& t) noexcept(noexcept(T(std::move(t))) && noexcept(std::declval<T>() = std::move(t))) {
    storage = std::move(t);
    return *this;
  }
  expected& operator = (T const& t) noexcept(std::is_nothrow_copy_constructible<T>::value && std::is_nothrow_copy_assignable<T>::value) {
    storage = t;
    return *this;
  }

  expected& operator = (E&& e) noexcept(noexcept(E(std::move(e))) && noexcept(std::declval<E>() = std::move(e))) {
    storage = std::move(e);
    return *this;
  }
  expected& operator = (E const& e) noexcept(noexcept(E(e)) && noexcept(std::declval<E>() = e)) {
    storage = e;
    return *this;
  }

  bool has_value() const noexcept { return storage.tag() == 0; }
  T& value() { return storage.template get<0>(); }
  T const& value() const { return storage.template get<0>(); }

  bool has_error() const noexcept { return storage.tag() == 1; }
  E& error() { return storage.template get<1>(); }
  E const& error() const { return storage.template get<1>(); }
};

template<typename E>
class expected<void, E> {
  variant<std::nullptr_t, E> storage;
public:
  using value_type = void;
  using error_type = E;

  expected() noexcept {};

  expected(expected&&) = default;
  expected(expected const&) = default;

  expected(E&& e) noexcept(noexcept(E(std::move(e)))) : storage(std::move(e)) {}
  expected(E const& e) noexcept(noexcept(E(e))) : storage(e) {}

  expected& operator = (expected&&) = default;
  expected& operator = (expected const&) = default;

  expected& operator = (std::nullptr_t) noexcept {
    storage = nullptr;
    return *this;
  }

  expected& operator = (E&& e) noexcept(noexcept(E(std::move(e))) && noexcept(std::declval<E>() = std::move(e))) {
    storage = std::move(e);
    return *this;
  }

  expected& operator = (E const& e) noexcept(noexcept(E(e)) && noexcept(std::declval<E>() = e)) {
    storage = e;
    return *this;
  }

  bool has_value() noexcept { return storage.tag() == 0; }
  void value() const { storage.template get<0>(); }

  bool has_error() noexcept { return storage.tag() == 1; }
  E& error() { return storage.template get<1>(); }
  E const& error() const { return storage.template get<1>(); }
};

template<typename E, typename T>
expected<decayed_type<T>,E> make_expected(T&& t) {
  return expected<decayed_type<T>,E>{std::forward<T>(t)};
}

template<typename E, typename T>
expected<T,E> make_expected(expected<T,E>&& te) {
  return std::move(te);
}

template<typename E, typename T>
expected<T,E> make_expected(expected<T,E> const& te) {
  return te;
}

template<typename E>
expected<void, E> make_expected() { return expected<void,E>(); }

template<typename T>
struct is_expected_type : std::false_type {};

template<typename T, typename E>
struct is_expected_type<expected<T,E>> : std::true_type {};

template<typename T, typename E, typename F>
auto operator >> (expected<T,E>&& te, F&& func) -> decltype(func(std::move(te))) {
  return func(std::move(te));
}

template<typename T, typename E, typename F>
auto operator >> (expected<T,E> const& te, F&& func) -> decltype(func(te)) {
  return func(te);
}

template<typename T, typename E, typename F>
auto call_with_value(expected<T,E>&& te, F&& f) -> decltype(f(std::move(te.value()))) { return f(std::move(te.value())); }

template<typename T, typename E, typename F>
auto call_with_value(expected<T,E> const& te, F&& f) -> decltype(f(te.value())) { return f(te.value()); }

template<typename E, typename F>
auto call_with_value(expected<void,E> const& te, F&& f) -> decltype(f()) { return f(); }

template<typename E, typename F, typename... Args>
auto call_with_value(expected<std::tuple<Args...>, E>&& te, F&& f) -> decltype(call_with_tuple(std::forward<F>(f), std::move(te.value()))) {
  return call_with_tuple(std::forward<F>(f), std::move(te.value()));
}

template<typename E, typename F, typename... Args>
auto call_with_value(expected<std::tuple<Args...>, E> const& te, F&& f) -> decltype(call_with_tuple(std::forward<F>(f), te.value())) {
  return call_with_tuple(std::forward<F>(f), te.value());
}

template<typename T, typename E, typename F>
auto construct_with_value(expected<T,E>&& te, F&& f) -> decltype(make_expected<E>(call_with_value(std::move(te), std::forward<F>(f)))) {
  return make_expected<E>(call_with_value(std::move(te), std::forward<F>(f)));
}

template<typename T, typename E, typename F>
auto construct_with_value(expected<T,E> const& te, F&& f) -> decltype(make_expected<E>(call_with_value(te, std::forward<F>(f)))) {
  return make_expected<E>(call_with_value(te, std::forward<F>(f)));
}

template<typename T, typename E, typename F>
auto construct_with_value(expected<T,E>&& te, F& f) -> typename std::enable_if<
  std::is_void<decltype(call_with_value(std::move(te), f))>::value,
  expected<void, E> >::type
{
  call_with_value(std::move(te), f);
  return {};
}

template<typename T, typename E, typename F>
auto construct_with_value(expected<T,E> const& te, F& f) -> typename std::enable_if<
  std::is_void<decltype(call_with_value(te, f))>::value,
  expected<void, E> >::type
{
  call_with_value(te, f);
  return {};
}

template<typename T, typename E, typename F>
auto construct_with_error(expected<T,E>&& te, F&& f) -> decltype(make_expected<E>(f(std::move(te.error())))) {
  return make_expected<E>(f(std::move(te.error())));
}

template<typename T, typename E, typename F>
auto construct_with_error(expected<T,E> const& te, F&& f) -> decltype(make_expected<E>(f(te.error()))) {
  return make_expected<E>(f(te.error()));
}

template<typename T, typename E, typename F>
auto construct_with_error(expected<T,E>&& te, F& f) -> typename std::enable_if<
  std::is_void<decltype(f(std::move(te)))>::value,
  expected<void, E> >::type 
{
  f(std::move(te.error()));
  return {};
}

template<typename T, typename E, typename F>
auto construct_with_error(expected<T,E> const& te, F& f) -> typename std::enable_if<
  std::is_void<decltype(f(te.error()))>::value,
  expected<void,E>>::type
{
  f(te.error());
  return {};
}


template<typename F>
struct if_valued_type {
  F func;

  template<typename T, typename E>
  auto operator()(expected<T, E>&& te) -> decltype(construct_with_value(std::move(te), func)) {
    if(te.has_value()) return construct_with_value(std::move(te), func);
    return decltype(construct_with_value(std::move(te), func)){std::move(te.error())};
  }

  template<typename T, typename E>
  auto operator()(expected<T, E> const& te) -> decltype(construct_with_value(te, func)) {
    if(te.has_value()) return construct_with_value(te, func);
    return decltype(construct_with_value(te, func)){te.error()};
  }
};

template<typename F>
struct if_errored_type {
  F func;

  template<typename T, typename E>
  auto operator()(expected<T,E>&& te) -> typename std::enable_if<std::is_same<T, decltype(func(std::move(te.error())))>::value, expected<T,E>>::type {
    if(te.has_error())
      return construct_with_error(std::move(te), func);
    return te;
  }

  template<typename T, typename E>
  auto operator()(expected<T,E> const& te) -> typename std::enable_if<std::is_same<T, decltype(func(te.error()))>::value, expected<T,E>>::type {
    if(te.has_error())
      return construct_with_error(std::move(te), func);
    return te;
  }
};

template<typename F>
if_valued_type<decayed_type<F>> if_valued(F&& func) { return {std::forward<F>(func)}; }

template<typename F>
if_errored_type<decayed_type<F>> if_errored(F&& func) { return {std::forward<F>(func)}; }

template<typename T, typename E, typename F>
auto operator >> (expected<T,E>&& te, F&& f) -> decltype(if_valued(std::forward<F>(f))(std::move(te))) {
  return if_valued(std::forward<F>(f))(std::move(te));
}

template<typename T, typename E, typename F>
auto operator >> (expected<T,E> const& te, F&& f) -> decltype(if_valued(std::forward<F>(f))(te)) {
  return if_valued(std::forward<F>(f))(te);
}

template<typename T, typename E, typename F>
auto operator >> (expected<T,E>&& te, F&& f) -> decltype(if_errored(std::forward<F>(f))(std::move(te))) {
  return if_errored(std::forward<F>(f))(std::move(te));
}

template<typename T, typename E, typename F>
auto operator >> (expected<T,E> const& te, F&& f) -> decltype(if_errored(std::forward<F>(f))(te)) {
  return if_errored(std::forward<F>(f))(te);
}

}

#endif

