#ifndef __VARIANT_HPP__acf076f9_622b_486d_9d2d_dc88441a9d45__
#define __VARIANT_HPP__acf076f9_622b_486d_9d2d_dc88441a9d45__

#include <stdexcept>
#include <type_traits>

#include "base.hpp"

namespace Variant {

template<typename... Args>
union Storage;

template<typename H, typename... T> 
union Storage<H, T...> {
  Storage() {}
  ~Storage() {}

  H head_;
  Storage<T...> tail_;
};

template<typename H>
union Storage<H> {
  H head_;
};

template<int I, typename... Args>
struct Type {
  static_assert(I > 0, "I should be greater than zero");
};

template<int I, typename H, typename... T>
struct Type<I, H, T...> {
  static_assert((I <= sizeof...(T)) && I >= 0, "I should be less than number of arguments and not negative");
  typedef typename Type<I-1, T...>::type type;
};

template<typename H, typename... T>
struct Type<0, H, T...> {
  typedef H type;
};

template<typename A, typename... Args>
struct Index {};

template<typename A, typename... Tail>
struct Index<A, A, Tail...> {
  static constexpr int value = 0; 
  typedef std::integral_constant<int, value> type;
};

template<typename A, typename H, typename... Tail> 
struct Index<A, H, Tail...> {
  static constexpr int value = Index<A, Tail...>::value != -1 ? (1 + Index<A, Tail...>::value) : -1;
  typedef std::integral_constant<int, value> type;
};

template<typename A>
struct Index<A> {
  static constexpr int value = -1;
  typedef std::integral_constant<int, value> type;
};

template<int I, typename H, typename... T>
typename Type<I,H,T...>::type& get(Storage<H,T...>& v, std::integral_constant<int, I> i) {
  return get(v.tail_, std::integral_constant<int, I-1>());
}

template<typename H, typename... T>
H& get(Storage<H,T...>& v, std::integral_constant<int, 0>) {
  return v.head_;
}

template<int I, typename... Args>
auto get(Storage<Args...>& v) -> decltype(get(v, std::integral_constant<int, I>())) {
  return get(v, std::integral_constant<int, I>());
}

template<int I, typename H, typename... T>
typename Type<I,H,T...>::type const& get(Storage<H,T...> const& v, std::integral_constant<int, I> i) {
  return get(v.tail_, std::integral_constant<int, I-1>());
}

template<typename H, typename... T>
H const& get(Storage<H,T...> const& v, std::integral_constant<int, 0>) {
  return v.head_;
}

template<int I, typename... Args>
auto get(Storage<Args...> const& v) -> decltype(get(v, std::integral_constant<int, I>())) {
  return get(v, std::integral_constant<int, I>());
}

template<int To, typename Op, typename... Args>
inline
void static_range(std::integral_constant<int, To>, std::integral_constant<int, To>, Op op, Args&&... args) {}

template<int From, int To, typename Op, typename... Args>
inline
void static_range(std::integral_constant<int, From> f, std::integral_constant<int, To> t, Op op, Args&&... args) {
  op(f, std::forward<Args>(args)...);
  static_range(std::integral_constant<int, From+1>(), t, op, std::forward<Args>(args)...);
}

template<int From, int To, typename Op, typename... Args>
inline
void static_range(Op op, Args&&... args) {
  static_range(std::integral_constant<int, From>(), std::integral_constant<int, To>(), op, std::forward<Args>(args)...);
}

namespace Details {
struct apply_to_static_index_call {
  template<int I, typename Op, typename... Args>
  void operator()(std::integral_constant<int, I> ic, int i, Op op, Args&&... args) {
    if(i == I) op(ic, std::forward<Args>(args)...);
  }
};
}

template<int From, int To, typename Op, typename... Args>
inline
void apply_to_static_index(int i, Op op, Args&&... args) {
  if(i < From || i >= To) throw std::range_error("index out of range");

  static_range<From,To>(Details::apply_to_static_index_call(), i, op, std::forward<Args>(args)...);
}

template<typename... Types>
class variant {
  int tag_ = 0;
  Storage<Types...> storage_;
  
  struct applicator {
    template<int I, typename V, typename Op, typename... Args>
    void operator()(std::integral_constant<int, I> ic, V& v, Op op, Args&&... args) {
      op(Variant::get<I>(v.storage_), std::forward<Args>(args)...);
    }
  };

  struct copy_construct {
    template<int I>
    void operator()(std::integral_constant<int, I> ic, variant& lhs, variant const& rhs) {
      new (&lhs.get<I>()) typename std::remove_reference<decltype(lhs.get<I>())>::type (rhs.get<I>());
    }
  };

  struct move_construct {
    template<int I>
    void operator()(std::integral_constant<int, I> ic, variant& lhs, variant& rhs) {
      new (&lhs.get<I>()) typename std::remove_reference<decltype(lhs.get<I>())>::type (std::move(rhs.get<I>()));
    }
  };

  struct assign {
    template<int I>
    void operator()(std::integral_constant<int, I> ic, variant& lhs, variant const& rhs) {
      if(lhs.tag() == rhs.tag())
        lhs.get<I>() = rhs.get<I>();
      else {
        typedef typename std::decay<decltype(rhs.get<I>())>::type target_type;
        (*this)(ic, lhs, rhs,  std::is_nothrow_copy_constructible<target_type>(), std::is_nothrow_move_constructible<target_type>());
      }
    }

    template<int I, bool M>
    void operator()(std::integral_constant<int, I>, variant& lhs, variant const& rhs, std::integral_constant<bool, true>, std::integral_constant<bool, M>) {
      lhs.~variant();
      new (&lhs) variant(rhs);
    }

    template<int I>
    void operator()(std::integral_constant<int, I>, variant& lhs, variant const& rhs, std::integral_constant<bool, false>, std::integral_constant<bool, true>) {
      variant tmp(rhs);
      lhs = std::move(tmp);
    }
  };

  struct destructor {
    template<typename T>
    void operator()(T& t) {
      t.~T();
    }
  };

  struct compare {
    bool flag = false;
    template<int I>
    void operator()(std::integral_constant<int, I> ic, variant const& lhs, variant const& rhs) {
      if(lhs.tag() == rhs.tag()) flag = lhs.get<I>() == rhs.get<I>();
    } 
  };

  friend bool operator == (variant const& a, variant const& b) {
    if(a.tag() != b.tag()) return false;
    compare c;
    apply_to_static_index<0, sizeof...(Types)>(a.tag(), std::ref(c), a, b);
    return c.flag;  
  }
public:
  variant() 
    noexcept(noexcept(variant(std::integral_constant<int, 0>())))
    //noexcept(std::is_nothrow_default_constructible<typename Type<0, Types...>::type>::value) 
    : variant(std::integral_constant<int, 0>())
  {}

  template<int I, typename... Args>
  variant(std::integral_constant<int, I> ic, Args&&... args) noexcept(std::is_nothrow_constructible<typename Type<I, Types...>::type, Args...>::value) {
    tag_ = I;
    new (&get<I>()) typename std::remove_reference<decltype(get<I>())>::type (std::forward<Args>(args)...);
  }

  template<int I, typename U, typename... Args>
  variant(std::integral_constant<int, I> ic, std::initializer_list<U> ilist, Args&&... args)
    noexcept(std::is_nothrow_constructible<typename Type<I, Types...>::type, std::initializer_list<U>, Args...>::value) 
  {
    tag_ = I;
    new (&get<I>()) typename std::remove_reference<decltype(get<I>())>::type (ilist, std::forward<Args>(args)...);
  }

  variant(variant const& rhs) noexcept(utils::is_all_predicate<std::is_nothrow_copy_constructible, Types...>::value) {
    tag_ = rhs.tag_;
    apply_to_static_index<0, sizeof...(Types)>(rhs.tag_, copy_construct(), *this, rhs);
  }

  template<typename A, typename std::enable_if<Index<typename std::decay<A>::type, Types...>::value != -1>::type* = nullptr>
  variant(A&& a)
    : variant(std::integral_constant<int, Index<typename std::decay<A>::type, Types...>::value>(), std::forward<A>(a)) {} 

  variant(variant&& rhs) noexcept(utils::is_all_predicate<std::is_nothrow_move_constructible, Types...>::value) {
    tag_ = rhs.tag_;
    apply_to_static_index<0, sizeof...(Types)>(rhs.tag_, move_construct(), *this, rhs);
  }

  variant& operator=(variant&& rhs) noexcept(utils::is_all_predicate<std::is_nothrow_move_constructible, Types...>::value) {
    this->~variant();
    new (this) variant(std::move(rhs));
    return *this;
  }

  variant& operator=(variant const& rhs) 
    noexcept(utils::is_all_predicate<std::is_nothrow_copy_constructible, Types...>::value
      && utils::is_all_predicate<std::is_nothrow_copy_assignable, Types...>::value)
  {
    apply_to_static_index<0, sizeof...(Types)>(rhs.tag_, assign(), *this, rhs);
    return *this;
  }

  template<typename A, typename std::enable_if<Index<typename std::decay<A>::type, Types...>::value != -1>::type* = nullptr>
  variant& operator=(A&& a) noexcept(std::is_nothrow_move_constructible<typename std::decay<A>::type>::value) {
    variant t(typename Index<typename std::decay<A>::type, Types...>::type(), std::forward<A>(a));
    return (*this) = std::move(t);
  }

  ~variant() {
    apply(destructor());
  }

  int tag() const { return tag_; }

  template<int I>
  auto get() -> decltype(Variant::get(*reinterpret_cast<Storage<Types...>*>(0), std::integral_constant<int, I>())) {
    if(I != tag_) throw std::runtime_error("bad_get");
    return Variant::get(storage_, std::integral_constant<int, I>());
  }

  template<int I>
  auto get() const -> decltype(Variant::get(*reinterpret_cast<const Storage<Types...>*>(0), std::integral_constant<int, I>())) {
    if(I != tag_) throw std::runtime_error("bad_get");
    return Variant::get(storage_, std::integral_constant<int, I>());
  }

  template<typename Op, typename... Args>
  void apply(Op op, Args&& ... args) {
    apply_to_static_index<0, sizeof...(Types)>(tag_, applicator(), *this, op, std::forward<Args>(args)...);
  }

  template<typename Op, typename... Args>
  void apply(Op op, Args&& ... args) const {
    apply_to_static_index<0, sizeof...(Types)>(tag_, applicator(), *this, op, std::forward<Args>(args)...);
  }

};

}

namespace utils {
  using ::Variant::variant;
}


#endif
