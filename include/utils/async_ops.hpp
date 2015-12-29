#ifndef __async_ops_bfd26d91_36b7_4a08_a1fb_c9861d8d723c_hpp__
#define __async_ops_bfd26d91_36b7_4a08_a1fb_c9861d8d723c_hpp__

#include "base.hpp" 
#include "expected.hpp"

namespace utils {
inline namespace async_ops {

// Library of combinators of asio style async operation (op(callback(std::error_code const& ec, result_type result)));
//
// // so, i want to be able to write something like this:
// async_read_some(device, buffers)
// << [&](std::size_t bytes) {
//      return async_write_some(device, std::const_buffers(buffers, bytes));
//    }
// += callback;

// async operation concept:
// struct async_op {
//   template<typename Func>
//   auto operator()(Func&& func) -> std::enable_if<is_callbable<Func(std::error_code, async_result_type<async_op>)>::value>::type;
// };
// 

template<typename A>
struct is_async_op : std::false_type {};

template<typename A>
struct async_result;

template<typename A>
using async_result_type = typename async_result<A>::type;

// trivial async op - async value container
template<typename V>
struct async_value {
  V value;

  template<typename F>
  auto operator += (F&& f) -> typename std::enable_if<is_callable<F(V)>::value>::type {
    f(std::move(value));
  }
};

template<typename V>
struct is_async_op<async_value<V>> : std::true_type {};

template<typename V>
struct async_result<async_value<V>> {
  using type = V;
};

template<typename V>
async_value<decayed_type<V>> make_async_value(V&& v) { return {std::forward<V>(v)}; }

//
// support for async_ops returning async_op
//
template<typename A>
struct unwrapped_async_op {
  A a;

  using result_type = async_result_type<async_result_type<A>>;

  template<typename F>
  auto operator += (F&& f) -> typename std::enable_if<is_callable<F(result_type)>::value>::type {
    a += [=](async_result_type<A> r) mutable {
      std::move(r) += f;
    };
  }
};

template<typename A>
struct is_async_op<unwrapped_async_op<A>> : std::true_type {};

template<typename A>
struct async_result<unwrapped_async_op<A>> {
  using type = typename unwrapped_async_op<A>::result_type;
};

template<typename A>
auto unwrap(A a) -> typename std::enable_if<
    is_async_op<A>::value && is_async_op<async_result_type<A>>::value,
    unwrapped_async_op<decayed_type<A>>
  >::type
{
  return {std::move(a)};
}

template<typename A>
auto unwrap(A&& a) -> typename std::enable_if<
    is_async_op<A>::value && !is_async_op<async_result_type<A>>::value,
    decayed_type<A>
  >::type
{
  return std::forward<A>(a);
}

//
// support for async_op chaining
//

template<typename A, typename F>
struct combined_async_op {
  A a;
  F func;

  using result_type = decltype(func(std::declval<async_result_type<A>>()));

  template<typename F2>
  auto operator += (F2&& func2) -> typename std::enable_if<is_callable<F2(result_type)>::value>::type {
    auto mf = move_on_copy(func);
    a += [=](async_result_type<A> r) mutable {
      func2(unwrap(mf)(std::move(r)));
    };
  }
};

template<typename A, typename F>
struct is_async_op<combined_async_op<A,F>> : std::true_type {};

template<typename A, typename F>
struct async_result<combined_async_op<A,F>> {
  using type = typename combined_async_op<A,F>::result_type;
};

template<typename A, typename F>
struct combined_and_unwrapped_type {
  using type = decltype(unwrap(std::declval<combined_async_op<A,F>>()));
};

template<typename A, typename F>
auto operator >> (A a, F func) -> typename std::enable_if<
    is_async_op<A>::value && is_callable<F(async_result_type<A>)>::value,
    combined_and_unwrapped_type<A,F>
  >::type::type
{
  return unwrap(combined_async_op<A,F>{std::move(a), std::move(func)});
}

// async_op interface for expected<async_op, E>

template<typename A, typename E>
struct is_async_op<expected<A, E>> : is_async_op<A> {};

template<typename A, typename E>
struct async_result<expected<A, E>> {
  static_assert(is_async_op<A>::value, "expected::value_type must be an async_op");

  using type = decltype(make_expected<E>(std::declval<async_result_type<A>>()));
};

template<typename A, typename E, typename F>
auto operator += (expected<A, E> a, F func) -> typename std::enable_if<
    is_async_op<expected<A, E>>::value
    && is_callable<F(async_result_type<expected<A,E>>)>::value
  >::type
{
  if(a.has_value()) {
    a.value() += [=] (async_result_type<A> r) mutable {
      func(make_expected<E>(std::move(r)));
    };
  }
  else
    func(async_result_type<expected<A,E>>(std::move(a.error())));
}

template<typename A, typename F>
auto operator >> (A a, F func) -> typename std::enable_if<
    is_async_op<A>::value
    && is_expected_type<async_result_type<A>>::value
    && is_callable<if_valued_type<F>(async_result_type<A>)>::value,
    combined_and_unwrapped_type<A, decltype(if_valued(std::move(func)))>
  >::type::type
{
  return std::move(a) >> if_valued(std::move(func));
}

template<typename A, typename F>
auto operator >> (A a, F func) -> typename std::enable_if<
    is_async_op<A>::value
    && is_expected_type<async_result_type<A>>::value
    && is_callable<F(typename async_result_type<A>::error_type)>::value,
    combined_and_unwrapped_type<A, decltype(if_errored(std::move(func)))>
  >::type::type
{
  return std::move(a) >> if_errored(std::move(func));
}

//
// tempalte<typename F>
// ... expected_to_asio(F f) 
//
// converts callback accepting expected<std::error_code, T> value i.e `callback(expected<std::error_code, T> v)' to
// asio style callback accepting std::error_code and return value - `callback(std::error_code const& ec, T r)'
//

template<typename F>
struct expected_to_asio_wrapper {
  F func;

  template<typename T>
  void operator()(std::error_code const ec, T&& t) {
    expected<decayed_type<T>, std::error_code> v(ec);
    if(!ec) v = std::forward<T>(t);
    
    func(std::move(v));
  }

  void operator()(std::error_code const& ec) {
    expected<void, std::error_code> v(ec);
    if(!ec) v = expected<void, std::error_code>();
    func(std::move(v));
  }
};

template<typename F>
expected_to_asio_wrapper<decayed_type<F>> expected_to_asio(F&& func) {
  return {std::forward<F>(func)};
}

// += operator overload for asio style callbacks

template<typename A, typename F>
auto operator += (A op, F func) -> typename std::enable_if<
    is_async_op<A>::value
    && is_expected_type<async_result_type<A>>::value
    && std::is_same<std::error_code, typename async_result_type<A>::error_type>::value
    && std::is_default_constructible<typename async_result_type<A>::value_type>::value
    && is_callable<F(typename async_result_type<A>::error_type, typename async_result_type<A>::value_type)>::value
  >::type
{
  op += [=](async_result_type<A> r) mutable {
    if(r.has_value())
      func(std::error_code(), std::move(r.value()));
    else
      func(r.error(), typename async_result_type<A>::value_type{});
  };
}

template<typename A, typename F>
auto operator += (A op, F func) -> typename std::enable_if<
    is_async_op<A>::value
    && is_expected_type<async_result_type<A>>::value
    && std::is_same<std::error_code, typename async_result_type<A>::error_type>::value
    && std::is_void<typename async_result_type<A>::value_type>::value
    && is_callable<F(typename async_result_type<A>::error_type)>::value
    && !is_callable<F(async_result_type<A>)>::value 
  >::type
{
  op += [=](async_result_type<A> r) mutable {
    if(r.has_value())
      func(std::error_code());
    else
      func(r.error());
  };
}

// adapter to async ops returning tuples 
template<typename A, typename F>
auto operator += (A op, F func) -> decltype(utils::call_with_tuple(std::move(func), std::declval<async_result_type<A>>()), std::declval<void>()) {
  std::move(op) += [=](async_result_type<A> r) mutable { 
    utils::call_with_tuple(std::move(func), std::move(r));
  };
}

//
// adapter for asio devices supporting async_read_some method
//
// template<typename Device, typename Buffers>
// async_op async_read_some(Device& device, Buffers buffer);
//

template<typename Device, typename Buffers>
struct async_read_some_op {
  Device& device;
  Buffers buffers;

  using result_type = expected<std::size_t, std::error_code>;

  template<typename Func>
  auto operator()(Func&& func) -> typename std::enable_if<is_callable<Func(result_type)>::value>::type {
    device.async_read_some(buffers, expected_to_asio(func));
  }

  template<typename Func>
  friend auto operator += (async_read_some_op& op, Func func) -> typename std::enable_if<is_callable<Func(result_type)>::value>::type {
    op.device.async_read_some(op.buffers, expected_to_asio(std::move(func)));
  }
};

template<typename Device, typename Buffers>
struct is_async_op<async_read_some_op<Device, Buffers>> : std::true_type {};

template<typename Device, typename Buffers>
struct async_result<async_read_some_op<Device, Buffers>> {
  using type = typename async_read_some_op<Device, Buffers>::result_type;
};

template<typename Device, typename Buffers>
async_read_some_op<Device, Buffers> async_read_some(Device& d, Buffers buffers) {
  return {d, std::move(buffers)};
}

//
// adapter for asio devices supporting async_write_some method
//
// template<typename Device, typename Buffers>
// async_op async_write_some(Device& device, Buffers buffer);
//
template<typename Device, typename Buffers>
struct async_write_some_op {
  Device& device; 
  Buffers buffers;
    
  using result_type = expected<std::size_t, std::error_code>;

  template<typename Func>
  auto operator()(Func&& func) -> typename std::enable_if<is_callable<Func(result_type)>::value>::type {
    device.async_wite_some(buffers, expected_to_asio(func));
  }
    
  template<typename Func>
  friend auto operator += (async_write_some_op& op, Func func) -> typename std::enable_if<is_callable<Func(result_type)>::value>::type {
    op.device.async_write_some(op.buffers, expected_to_asio(std::move(func)));
  }
};
    
template<typename Device, typename Buffers>
struct is_async_op<async_write_some_op<Device, Buffers>> : std::true_type {};

template<typename Device, typename Buffers>
struct async_result<async_write_some_op<Device, Buffers>> {
  using type = typename async_write_some_op<Device, Buffers>::result_type;
};

template<typename Device, typename Buffers>
async_write_some_op<Device, Buffers> async_write_some(Device& d, Buffers buffers) {
  return {d, std::move(buffers)};
}

//
// polimorphic_async_op
//
template<typename T>
struct polymorphic_async_op {
  std::function<void (std::function<void (T)>)> func;

  polymorphic_async_op() = default;
  polymorphic_async_op(polymorphic_async_op&&) = default;
  polymorphic_async_op(polymorphic_async_op const&) = default;

  template<typename A>
  static
  auto make_func(A&& a) -> typename std::enable_if<
      is_async_op<decayed_type<A>>::value 
      && std::is_same<T, async_result_type<decayed_type<A>>>::value
      && !std::is_same<polymorphic_async_op<T>, decayed_type<A>>::value,
      std::function<void (std::function<void (T)>)>
    >::type
  {
    auto mf = move_on_copy(std::forward<A>(a));
    return [=](std::function<void (T)> func) mutable { unwrap(mf) += std::move(func); };
  }

  template<typename A>
  polymorphic_async_op(A a) : func(make_func(std::forward<A>(a))) {}
  
  polymorphic_async_op& operator = (polymorphic_async_op&&) = default;
  polymorphic_async_op& operator = (polymorphic_async_op const&) = default;

  template<typename A>
  auto operator = (A&& a) -> decltype(make_func(std::forward<A>(a)), *this) {
    func = make_func(std::forward<A>(a));
    return *this;
  }
 
  template<typename F>
  friend auto operator += (polymorphic_async_op<T> op, F func) -> typename std::enable_if<is_callable<F(T)>::value>::type {
    op.func(std::move(func));
  }
};

template<typename T>
struct is_async_op<polymorphic_async_op<T>> : std::true_type {};

template<typename T>
struct async_result<polymorphic_async_op<T>> {
  using type = T;
};

//
// async_future

template<typename T>
struct async_variable {
  variant<std::nullptr_t, std::function<void (T t)>, T> state;

  async_variable() noexcept {}
  async_variable(async_variable&&) = default;
  async_variable& operator=(async_variable&&) = default;

  async_variable& operator = (T t) {
    if(state.tag() == 1)
      (state.template get<1>())(std::move(t));

    state = std::move(t); 
  
    return *this;
  }

  template<typename F>
  auto operator += (F func) -> typename std::enable_if<is_callable<F(T)>::value>::type
  {
    if(state.tag() == 2)
      func(std::move(state.template get<2>()));
    else
      state = std::function<void (T)>(std::move(func));
  }
};

template<typename T>
struct is_async_op<async_variable<T>> : std::true_type {};

template<typename T>
struct async_result<async_variable<T>> {
  using type = T;
};

template<typename T, typename F>
auto operator += (std::shared_ptr<async_variable<T>> const& v, F func) -> typename std::enable_if<is_callable<F(T)>::value>::type {
  *v += std::move(func);
}

template<typename T>
struct is_async_op<std::shared_ptr<async_variable<T>>> : std::true_type {};

template<typename T>
struct async_result<std::shared_ptr<async_variable<T>>> : async_result<async_variable<T>> {};

template<typename T, typename OP> 
struct ensure_op {
  variant<T, OP> op; 

  template<typename F>
  auto operator += (F func) -> typename std::enable_if<is_callable<F(T)>::value>::type {
    if(op.tag() == 0)
      func(std::move(op.template get<0>()));
    else
      op.template get<1>() += std::move(func);
  }
};

template<typename T,typename A>
struct is_async_op<ensure_op<T,A>> : std::true_type {};

template<typename T, typename A>
struct async_result<ensure_op<T,A>>  { using type = T; };

template<typename T, typename A>
auto ensure(T t, A aop) -> typename std::enable_if<
    std::is_convertible<decltype(!!t), bool>::value
    && is_async_op<A>::value
    && std::is_same<T, typename async_result<A>::type>::value,
    ensure_op<T,A>
  >::type
{
  if(!!t)
    return ensure_op<T,A>{{std::move(t)}};
  else
    return ensure_op<T,A>{{std::move(aop)}};
}

template<typename T, typename A>
auto ensure(T t, A a) -> typename std::enable_if<
    std::is_convertible<decltype(!!t), bool>::value
    && is_async_op<A>::value
    && std::is_same<typename std::decay<decltype(*t)>::type, typename async_result<A>::type>::value,
    ensure_op<typename std::decay<decltype(*t)>::type, A>
  >::type
{
  using return_type = ensure_op<typename std::decay<decltype(*t)>::type, A>;

  if(!!t)
    return return_type{{std::move(*t)}};
  else
    return return_type{{std::move(a)}}; 
}

} // namespace async_ops
} // namespace utils

#endif

