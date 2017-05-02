! { dg-do run { target { vect_simd_clones && lto } } }
! { dg-options "-fno-inline -flto -fno-use-linker-plugin --param lto-streamer-checking=1" }
! { dg-additional-sources declare-simd-3.f90 }
! { dg-additional-options "-msse2" { target sse2_runtime } }
! { dg-additional-options "-mavx" { target avx_runtime } }
! { dg-final { cleanup-modules "declare_simd_2_mod" } }

include 'declare-simd-2.f90'
