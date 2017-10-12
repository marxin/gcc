// PR c++/34054
// { dg-do compile { target c++11 } }

template<typename... T> T foo() { static T a; return a; } // { dg-error "not expanded|T" }
