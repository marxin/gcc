/* { dg-options "-O2 -march=x86-64 -mtune=generic" } */

#include <inttypes.h>

void a (void) {}
#pragma GCC push_options
#pragma GCC optimize "align-functions=1024"
void b (void) {}
void c (void) {}
#pragma GCC pop_options
void d (void) {}
void e (void) {}
#pragma GCC push_options
#pragma GCC optimize "align-functions=256"
void f (void) {}
#pragma GCC pop_options

#define CHECK_ADDRESS(fn, alignment) \
  do { \
    intptr_t addr = (intptr_t)&fn; \
    if (addr % alignment) \
      __builtin_abort (); \
  } \
  while (0);

int main(int argc, char **argv)
{
  CHECK_ADDRESS (a, 8);
  CHECK_ADDRESS (b, 1024);
  CHECK_ADDRESS (c, 1024);
  CHECK_ADDRESS (f, 256);

  return 0;
}

/* { dg-final { scan-assembler-not ".p2align 4,,255" } } */
/* { dg-final { scan-assembler-not ".p2align 4,,1023" } } */
/* { dg-final { scan-assembler-times ".p2align 8,,255" 1 } } */
/* { dg-final { scan-assembler-times ".p2align 10,,1023" 2 } } */
