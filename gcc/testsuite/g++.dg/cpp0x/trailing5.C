// PR c++/38798, DR 770
// { dg-do compile { target c++11 } }

struct A {};
auto foo() -> struct A { static A a; return a; }

enum B {};
auto bar() -> enum B { static B b; return b; }

auto baz() -> struct C {} {}	// { dg-error "" }
