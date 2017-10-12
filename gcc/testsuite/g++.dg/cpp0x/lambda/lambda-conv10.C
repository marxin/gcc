// PR c++/69889
// { dg-do compile { target c++11 } }

template <typename F> struct Tag {
  static void fp() { f()(0); }
  static F f() { static F a; return a; }
};

struct Dispatch {
  template <typename F> Dispatch(F&&) : f(Tag<F>::fp) {}
  void (*f)();
};

struct Empty { Empty(Empty&&); };

struct Value {
  Value();
  template <typename U> Value(U);
  void call(Dispatch);
  Empty e;
};

struct EmptyValue {
  EmptyValue(EmptyValue&&);
  EmptyValue();
};

struct User {
  User() {
    Value().call([](Value) { return EmptyValue(); });
  }
};

User user;
