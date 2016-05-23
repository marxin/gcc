// PR c++/70590
// { dg-do compile }
// { dg-options "-Wall -Os" }

class A;
template <long Length> class B {
    A mArr[Length];

public:
    A &operator[](long aIndex) {
        return mArr[aIndex];
    }
};
class A {
public:
    operator int *() {
        int *a = mRawPtr;
        return a;
    }
    int *mRawPtr;
};
extern B<0> b;
void fn1() {
    if (b[long(fn1)])
        new int;
}
