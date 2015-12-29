#ifndef __light_futures_hpp__
#define __light_futures_hpp__

#include <functional>
#include <vector>
#include <iterator>

#include "base.hpp"
#include "variant.hpp"


namespace utils {

inline namespace futures {

enum class future_errc {
  broken_promise,
  future_already_retrieved,
  promise_already_satisfied,
  no_state,
  would_wait
};

inline
const std::error_category& future_category() noexcept {
  static struct impl : std::error_category {
    const char* name() const noexcept { return "utils::futures"; }

    std::string message(int e) const {
      switch(future_errc(e)) {
      case future_errc::broken_promise:
        return "broken_promise";
      case future_errc::future_already_retrieved:
        return "future already retrieved";
      case future_errc::promise_already_satisfied:
        return "promise already satisfied";
      case future_errc::would_wait:
        return "future would wait";
      default:
        return "unknown future error";
      }
    }
  } cat;

  return cat;
}

inline
std::error_code make_error_code(future_errc e) noexcept { return std::error_code((int)e, future_category()); }

std::error_condition make_error_condition(future_errc e) noexcept;

class future_error : public std::logic_error {
  std::error_code ec;
public:
  future_error(std::error_code ec) : logic_error(ec.message()), ec(ec) {}

  std::error_code const& code() const noexcept { return ec; }
};

template<typename R>
struct value_wrapper {
  R r;
  
  value_wrapper(R r) : r(std::move(r)) {}

  R get() { return std::move(r); }

  R const& get_shared() const { return r; }

  template<typename C, typename... Args>
  static value_wrapper<R> wrap(C&& c, Args&&... args) { return {c(std::forward<Args>(args)...)}; }
};

template<typename R>
struct value_wrapper<R&> {
  R& r;
  
  value_wrapper(R& r) : r(r) {}

  R& get()  { return r; }
  R& get_shared() const { return r; }

  template<typename C, typename... Args>
  static value_wrapper<R&> wrap(C&& c, Args&&... args) { return {c(std::forward<Args>(args)...)}; }
};

template<>
struct value_wrapper<void> {
  void get() {}
  void get_shared() const {};

  using get_result_type = void;

  template<typename C, typename... Args>
  static value_wrapper<void> wrap(C&& c, Args&&... args) { 
    c(std::forward<Args>(args)...); 
    return {};
  }
};

template<typename R>
struct future_shared_state {
  struct empty {};

  variant<empty, value_wrapper<R>, std::exception_ptr> storage;

  std::function<void () noexcept> notification;

  bool ready() const { return storage.tag() != 0; }

  value_wrapper<R>& internal_get() {
    switch(storage.tag()) {
    case 0: throw future_error(make_error_code(future_errc::would_wait));
    case 2: std::rethrow_exception(storage.template get<2>());
    default:
      return storage.template get<1>();
    };
  }

  auto get() -> decltype(this->internal_get().get()) { return internal_get().get(); }
  auto get_shared() -> decltype(this->internal_get().get_shared()) { return internal_get().get_shared(); }

  template<typename C>
  void add_notification(C&& c) {
    if(storage.tag() == 0) {
      if(!notification) {
        notification = move_on_copy(std::move(c));
      }
      else {
        notification = [prev = std::move(notification), c = move_on_copy(std::move(c))]() mutable noexcept { 
          prev();
          c();
        };
      }
    }
    else
      c();
  }

  void set_value(value_wrapper<R>&& r) {
    if(ready()) throw future_error(make_error_code(future_errc::promise_already_satisfied));

    storage = std::move(r);

    if(!!notification) {
      auto n = std::move(notification);
      n();
    }
  }

  void set_exception(std::exception_ptr p) { 
    if(ready()) throw future_error(make_error_code(future_errc::promise_already_satisfied));

    storage = std::move(p);

    if(!!notification) notification();
  }
};

template<typename R>
class future_shared_state_handle {
protected:
  std::shared_ptr<future_shared_state<R>> state;
public:
  future_shared_state_handle() = default;
  future_shared_state_handle(std::shared_ptr<future_shared_state<R>> p) : state(p) {}

  std::shared_ptr<future_shared_state<R>> get_state() const {
    if(!state) throw future_error(make_error_code(future_errc::no_state));
    return state;
  }

  std::shared_ptr<future_shared_state<R>> move_state() {
    if(!state) throw future_error(make_error_code(future_errc::no_state));
    return std::move(state);
  }
};

template<typename R>
class future;

template<typename R>
class shared_future;

template<typename R>
class promise_base;

template<typename R>
class promise;

template<typename R>
future<R> unwrap(future<R> f);

template<typename R>
future<R> unwrap(future<future<R>> f);

template<typename R>
future<R> unwrap(shared_future<R> f);

template<typename R>
future<R> unwrap(future<shared_future<R>> f);

template<typename R>
class shared_future : private future_shared_state_handle<R> {
public:
  shared_future() = default;
  shared_future(shared_future const&) = default;
  shared_future(future<R>&& p);
  
  auto get() const -> decltype(this->get_state()->get_shared()) { return this->get_state()->get_shared(); }

  bool valid() const noexcept { return !!this->state; }
  bool ready() const noexcept { return valid() && this->state->ready(); }

  template<typename C>
  auto then(C&& c) -> decltype(unwrap(std::declval<future<decltype(c(*this))>>()));
};

template<typename R>
class future : future_shared_state_handle<R> {
  friend class shared_future<R>;
  friend class promise_base<R>;
 
  future(std::shared_ptr<future_shared_state<R>> p) : future_shared_state_handle<R>(std::move(p)) {}
public:
  future() noexcept = default;
  future(future const&) = delete;
  future(future&&) noexcept = default;
  future& operator=(future&&) = default;

  bool valid() const noexcept { return !!this->state; }
  bool ready() const noexcept { return valid() && this->state->ready(); }

  //auto get() -> decltype(this->get_state()->get()) {
  auto get() -> R {
    return this->move_state()->get();
  }

  template<typename C>
  auto then(C&& c) -> decltype(unwrap(std::declval<future<decltype(c(std::move(*this)))>>()));
  shared_future<R> share() { return shared_future<R>(std::move(*this)); }
};

template<typename R>
class promise_base : future_shared_state_handle<R> {
  bool future_retrieved = false;

  template<typename R1> friend class future;
  friend class shared_future<R>;
  
  template<typename R2>
  friend future<R2> unwrap(future<future<R2>>);
  friend future<void> unwrap(future<future<void>>);

  template<typename R2>
  friend future<R2> unwrap(future<shared_future<R2>>);
  friend future<void> unwrap(future<shared_future<void>>);
 
protected:
  promise_base() : future_shared_state_handle<R>(std::make_shared<future_shared_state<R>>()) {}
  ~promise_base() {
    if(this->state && !this->state->ready())
      this->state->set_exception(std::make_exception_ptr(future_error(make_error_code(future_errc::broken_promise))));
  }

  promise_base(promise_base const&) = delete;
  promise_base(promise_base&&) = default;
  promise_base& operator=(promise_base&&) = default;

  void set_value_internal(value_wrapper<R>&& r) {
    auto s = this->get_state();
    s->set_value(std::move(r));
  }
public:
  void set_exception(std::exception_ptr p) {
    auto s = this->get_state();
    s->set_exception(std::move(p));
  }

  future<R> get_future() {
    auto s = this->get_state();
    if(future_retrieved) throw future_error(make_error_code(future_errc::future_already_retrieved));
    future_retrieved = true;
    return future<R>(std::move(s));
  }

  void swap(promise<R>&) noexcept;
};

template<typename R>
class promise : public promise_base<R> {
public:
  promise() = default;
  promise(promise&&) = default;
  promise& operator=(promise&& r) = default;

  void set_value(R&& r) {
    promise_base<R>::set_value_internal(value_wrapper<R>(std::move(r)));
  }

  void set_value(R const& r) {
    promise_base<R>::set_value_internal(value_wrapper<R>(std::move(r)));
  }
};

template<typename R>
class promise<R&> : public promise_base<R&> {
public:
  void set_value(R& r) {
    promise_base<R&>::set_value_internal(value_wrapper<R&>(r));
  }
};

template<>
class promise<void> : public promise_base<void> {
public:
  void set_value() {
    promise_base<void>::set_value_internal(value_wrapper<void>());
  }
};

template <typename R>
void promise_base<R>::swap(promise<R>& other) noexcept {
  std::swap(this->future_retrieved, other.future_retrieved);
  std::swap(this->state, other.state);
}

template<typename R>
future<R> unwrap(future<R> f) { return std::move(f); }

template<typename R>
future<R> unwrap(future<future<R>> f) {
  promise<R> p;
  auto r = p.get_future();
  auto s = p.move_state();

  f.then([=](future<future<R>> r) { 
    try {
      r.get().then([=](future<R> r3) {
        try {
          s->set_value(value_wrapper<R>(r3.get()));
        }
        catch(...) {
          s->set_exception(std::current_exception());
        }
      });
    }
    catch(...) {
      s->set_exception(std::current_exception());
    }
  });

  return r;
}

inline future<void> unwrap(future<future<void>> f) {
  promise<void> p;
  auto r = p.get_future();
  auto s = p.move_state();

  f.then([=](future<future<void>> r) {
    try {
      r.get().then([=](future<void> r) {
        try {
          r.get();
          s->set_value(value_wrapper<void>());
        } 
        catch(...) {
          s->set_exception(std::current_exception());
        }
      });
    }
    catch(...) {
      s->set_exception(std::current_exception());
    }
  });
  return r;
}

template<typename R>
future<R> unwrap(future<shared_future<R>> f) {
  promise<R> p;
  auto r = p.get_future();
  auto s = p.move_state();

  f.then([=](auto r) { 
    try {
      r.get().then([=](auto r3) {
        try {
          s->set_value(value_wrapper<R>(r3.get()));
        }
        catch(...) {
          s->set_exception(std::current_exception());
        }
      });
    }
    catch(...) {
      s->set_exception(std::current_exception());
    }
  });

  return r;
}

template<typename R>
template<typename C>
auto future<R>::then(C&& c) -> decltype(unwrap(std::declval<future<decltype(c(std::move(*this)))>>())) {
  using result_type = decltype(c(std::move(*this)));

  auto s = this->get_state();

  promise<result_type> p;
  auto f = p.get_future();

  s->add_notification([p = std::move(p), c = std::forward<C>(c), f = std::move(*this)]() mutable {
    try {
      p.set_value_internal(value_wrapper<result_type>::wrap(c,std::move(f)));
    }
    catch(...) {
      p.set_exception(std::current_exception());
    }
  });
  return unwrap(std::move(f));
}

template<typename R>
shared_future<R>::shared_future(future<R>&& r) : future_shared_state_handle<R>(std::move(r)) {}
 
template<typename R>
template<typename C>
auto shared_future<R>::then(C&& c) -> decltype(unwrap(std::declval<future<decltype(c(*this))>>())) {
  // dirty hack
  return future<R>(this->get_state()).then([c = std::forward<C>(c)](future<R> f) mutable { return c(f.share()); });
}

template<typename R>
future<R> make_ready_future(R r) {
  promise<R> p;
  p.set_value(std::move(r));
  return p.get_future();
}

inline future<void> make_ready_future() {
  promise<void> p;
  p.set_value();
  return p.get_future();
}

template<typename R>
future<R> make_exceptional_future(std::exception_ptr e) {
  promise<R> p;
  p.set_exception(e);
  return p.get_future();
}

template<typename T>
class future_queue {
  struct item {
    future<item> next;
    T value;
  };
  
  promise<item> tail;
  future<item> head = tail.get_future();
public:
  future<T> pop() {
    auto p = move_on_copy(promise<T>());
    auto r = unwrap(p).get_future();

    head = head.then([=](future<item> h) mutable {
      auto i = h.get();
      unwrap(p).set_value(std::move(i.value));
      return std::move(i.next);
    });

    return r;
  }
  
  void push(T t) {
    promise<item> p;
    auto f = p.get_future();
    p.swap(tail);
    p.set_value(item{std::move(f), std::move(t)});
  }
};

template<typename I>
future<std::vector<typename std::iterator_traits<I>::value_type>> when_all(I first, I last) {
  using future_type = typename std::iterator_traits<I>::value_type;
  std::vector<future_type> v(std::distance(first, last));
  std::move(first, last, v.begin());

  struct impl {
    auto operator()(std::vector<future_type> v, size_t n) {
      if(n == v.size()) return make_ready_future(std::move(v));
      auto f = std::move(v[n]);
      return f.then([v = std::move(v), n] (auto f) mutable { 
        v[n] = std::move(f);
        return impl{}(std::move(v), n + 1); 
      }); 
    }   
  };  

  return impl{}(std::move(v), 0); 
}

inline 
future<std::tuple<>> when_all() { return make_ready_future(std::tuple<>()); }

template<typename Future>
future<std::tuple<std::decay_t<Future>>> when_all(Future&& future) {
  return future.then([](auto f) { return std::make_tuple(std::move(f)); }); 
}

template<typename FuturesH, typename... FuturesT>
future<std::tuple<std::decay_t<FuturesH>, std::decay_t<FuturesT>...>> when_all(FuturesH&& h, FuturesT&&... t) {
  return h.then([f2 = when_all(std::forward<FuturesT>(t)...)](auto f1) mutable {
    return f2.then([f1 = std::move(f1)](auto f2) mutable { return std::tuple_cat(std::make_tuple(std::move(f1)), f2.get());});
  }); 
}

} // inline namespace futures
} // utils

#endif
