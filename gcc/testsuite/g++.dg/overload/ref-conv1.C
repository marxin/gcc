// PR c++/50442

template <typename T> struct MoveRef { operator T& () { return MoveRef(); } };
template <typename T> MoveRef <T> Move(T&a) { return a; }
struct Thing {};
Thing foo(const Thing* p) { return Thing(Move(*p)); }
