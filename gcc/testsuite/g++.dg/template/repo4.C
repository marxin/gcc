// PR c++/17775
// { dg-options "-frepo" }
// { dg-final { cleanup-repo-files } }
// { dg-require-host-local "" }
// { dg-skip-if "dkms are not final links" { vxworks_kernel } }
// { dg-warning "switch '-frepo' is no longer supported" "" { target *-*-* } 0 }

namespace { 
  struct Foo {}; 
} 
 
template <typename Tp> 
void foo(Tp) {} 
 
int 
main() 
{ 
  foo(Foo()); 
} 
