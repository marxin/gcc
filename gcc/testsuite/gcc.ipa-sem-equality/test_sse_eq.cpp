#include <xmmintrin.h>

void foo()
{
  float x = 1.2345f;
  __m128 v =_mm_load1_ps(&x);
}

void bar()
{
  float x = 1.2345f;
  __m128 v =_mm_load1_ps(&x);
}

int main()
{
  return 2;
}
