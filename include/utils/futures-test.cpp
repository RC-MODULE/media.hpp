#include "futures.hpp"
#include <string>
#include <cassert>
#include <vector>

using namespace utils;

void test1() {
  promise<int> p;
  auto f = p.get_future();
  
  assert(f.valid() == true);
  assert(f.ready() == false);
  
  p.set_value(12);
  
  assert(f.ready() == true);
  assert(f.get() == 12);
  assert(f.valid() == false);
}

void check_void_promise() {
  promise<void> p;
  auto f = p.get_future();

  assert(f.valid() == true);
  assert(f.ready() == false);

  p.set_value();

  assert(f.ready() == true);
  f.get();
  assert(f.valid() == false);
}

void check_reference_promise() {
  int v = 47;
  promise<int&> p;
  auto f = p.get_future();
  
  assert(f.valid() == true);
  assert(f.ready() == false);
  
  p.set_value(v);
  
  assert(f.ready() == true);
  assert(&f.get() == &v);
  assert(f.valid() == false);
  
  std::cout << "check_reference_promise: done" << std::endl;
}

void test2() {
  promise<int> p;
  p.set_value(34);
  assert(p.get_future().get() == 34);
}

void test3() {
  promise<int> p;
  int x = 0;
 
  auto f = p .get_future();
 
  assert(f.valid());
  
  f.then([&](future<int> f) {
    x = f.get();
  });

  assert(!f.valid());

  assert(x == 0);

  p.set_value(67);

  assert(x == 67);
}

void test4() {
  promise<int> p;
  int x = 0;

  p.set_value(51);
 
  auto f = p .get_future();
 
  assert(f.valid());
  
  f.then([&](future<int> f) {
    x = f.get();
  });

  assert(!f.valid());
  assert(x = 51);
}

void check_then_return_value() {
  promise<int> p;
  
  auto f = p.get_future();
  assert(f.valid());

  auto r = f.then([](future<int> a) {
    return a.get() + 17;
  });

  p.set_value(13);
  assert(r.ready());
  assert(r.valid());
  assert(r.get() == 30);
  assert(r.valid() == false);
}

void check_then_chaining() {
  promise<std::string> p;
  
  auto f = p.get_future()
  .then([](future<std::string> f) {
    return f.get().size();
  })
  .then([](future<std::size_t> f) {
    return f.get() +  34;  
  });

  std::string v = "123456";

  p.set_value(v);
  assert(f.get() == v.size() + 34);
}

void check_then_returning_future() {
  promise<void> p1;
  promise<std::string> p2;

  auto f = p1.get_future().then([&](future<void> f) {
    return p2.get_future();
  });

  assert(f.valid());
  assert(!f.ready());

  p1.set_value();

  assert(f.valid());
  assert(!f.ready());

  p2.set_value("123");

  assert(f.valid());
  assert(f.ready());
  assert(f.get() == "123");
}

void check_shared_basic() {
  promise<std::string> p;

  auto f = p.get_future();

  assert(f.valid());
  auto fs1 = f.share();
  assert(!f.valid());

  p.set_value("1234");
  assert(fs1.get() == "1234");
  assert(fs1.get() == "1234");
}

void check_shared_then() {
  promise<std::string> p;
  
  auto f = p.get_future().share();

  auto r1 = f.then([](shared_future<std::string> s) { return s.get().size() + 11; });
  auto r2 = f.then([](shared_future<std::string> s) { return s.get().size() + 17; });
  auto r3 = f.then([](auto s) { return s; });

  p.set_value("hello");
  assert(r1.get() == 5+11);
  assert(r2.get() == 5+17);

  std::cout << "check_shared_then: done" << std::endl;
}

struct throw_on_n_move {
  int n;

  explicit throw_on_n_move(int n) : n(n) {}

  throw_on_n_move(throw_on_n_move&& r) : n(r.n-1) {
    if(n == 0) throw std::logic_error("max move count is reached");
  }

  throw_on_n_move& operator= (throw_on_n_move&& r) {
    n = r.n-1;
    if(n == 0) throw std::logic_error("max move count is reached");
    return *this;
  }
};

template<typename T, typename C>
struct controlled_wrapper {
  T value;
  C control;

  template<typename... Args>
  auto operator()(Args&& ... args) -> decltype(value(std::forward<Args>(args)...)) {
    return value(std::forward<Args>(args)...);
  }
};

template<typename T, typename C>
controlled_wrapper<T, C> make_controlled_wrapper(T t, C c) {
  return controlled_wrapper<T,C>{std::move(t), std::move(c)};
}

void check_throw_in_callback() {
  auto f = utils::make_ready_future(12);

  auto f2 = f.then(
    make_controlled_wrapper(
      [](future<int> f) {
        return utils::make_ready_future(f.get());
      },
      throw_on_n_move(3)
    )
  );

  assert(f2.get() == 12);

  std::cout << "check_throw_in_callback: done" << std::endl;
};


void check_queue() {
  utils::future_queue<int> q;
  std::vector<int> v;

  auto lambda = [&](future<int> f) { v.push_back(f.get()); };
  q.pop().then(lambda);
  q.pop().then(lambda);
  
  assert(v.size() == 0);
  q.push(0);
  assert(v.size() == 1);
  q.push(1);
  assert(v.size() == 2);

  q.push(2);
  q.push(3);
  q.push(4);

  assert(v.size() == 2);  

  q.pop().then(lambda);
  q.pop().then(lambda);
  q.pop().then(lambda);

  assert(v.size() == 5);

  std::cout << "check_basic_future_queue: done" << std::endl;
}

void check_void_future_chain() {
  utils::promise<void> p1;
  utils::promise<void> p2;

  bool flag = false;
  p1.get_future().share().then([&](utils::shared_future<void> f) { return p2.get_future(); }).then([&](utils::future<void>){
    flag = true;
  });

  assert(!flag);
  p2.set_value();
  assert(!flag);
  p1.set_value();

  assert(flag);

  std::cout << "check_void_future_chain: done" << std::endl;
}

  static int n = 0;
  struct A {
    A() { ++n; }
    A(A&&) { ++n; }
    A& operator = (A&&) = default;
    ~A() { --n; }
  };


void check_destructor() {
  promise<A> p;
  {
    auto f1 = p.get_future().share(); 
    f1.then([](auto f) {});

    p.set_value(A());
    p = std::move(promise<A>{});
  }

  assert(n == 0);

  std::cout << "check_destructor: done" << std::endl;
}

void check_when_all_tuple() {
  promise<void> p1;
  promise<int> p2;
  promise<int> p3;

  bool flag = false;
  when_all(p1.get_future(), p2.get_future().share(), p3.get_future()).then([&](auto r) {
    auto t = r.get();
    std::get<0>(t).get();
    assert(std::get<1>(t).get() == 57);
    flag = true;
  });

  assert(flag == false);
  p2.set_value(57);
  assert(flag == false);
  p1.set_value();
  assert(flag == false);
  p3.set_value(91);
  
  assert(flag == true);
  
  std::cout << "check_when_all_tuple: done" << std::endl;
}

void check_when_all_vector() {
  std::vector<promise<int>> p(4);
  std::vector<future<int>> f(p.size());
  std::transform(p.begin(), p.end(), f.begin(), [](auto& p) { return p.get_future(); });

  bool flag = false;

  when_all(f.begin(), f.end()).then([&](future<std::vector<future<int>>> f) {
    auto v = f.get();
    assert(v.size() == p.size());
    for(int i = 0; i != v.size(); ++i)
      assert(v[i].get() == 53 * i);
    flag = true;
  });

  for(int i = 0; i != p.size(); ++i) {
    assert(!flag);
    p[i].set_value(53*i);
  }

  assert(flag);

  std::cout << "check_when_all_vector: done" << std::endl;
}

void check_return_invalid_future_from_then() {
  promise<void> p;
  auto f = p.get_future().then([](auto) { return future<void>{}; });

  p.set_value();
  try {
    f.get();
    assert(0);
  }
  catch(future_error const& e) {
    assert(e.code() == make_error_code(future_errc::no_state));
  }

  std::cout << "check_return_invalid_future_from_then" << std::endl;
}

int main(int argc, char* argv[]) {
  test1();
  check_void_promise();
  check_reference_promise();
  test2();
  test3();
  test4();
  check_then_return_value();
  check_then_chaining();
  check_then_returning_future();
  check_shared_basic();
  check_shared_then();

  check_queue();

  check_throw_in_callback();
  check_void_future_chain();
  check_destructor();

  check_when_all_tuple();
  check_when_all_vector();

  check_return_invalid_future_from_then();
}

