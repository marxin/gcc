// { dg-options "-frepo" }
// { dg-require-host-local "" }
// { dg-warning "switch '-frepo' is no longer supported" "" { target *-*-* } 0 }

extern "C" inline void f() {}

int main () {
  f();
}

// { dg-final { cleanup-repo-files } }
