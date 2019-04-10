/* { dg-do compile} */
/* { dg-require-ifunc "" }  */

#define FN(TARGET) \
void __attribute__ ((target(TARGET))) foo () {}

FN("avx512f")
FN("avx512vl")
FN("avx512bw")
FN("avx512dq")
FN("avx512cd")
FN("avx512er")
FN("avx512pf")
FN("avx512vbmi")
FN("avx512ifma")
FN("avx5124vnniw")
FN("avx5124fmaps")
FN("avx512vpopcntdq")
FN("avx512vbmi2")
FN("gfni")
FN("vpclmulqdq")
FN("avx512vnni")
FN("avx512bitalg")
FN("default")

int main()
{
  foo ();
  return 0;
}
