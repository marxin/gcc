/* Test recovery mode.  */
/* { dg-do run } */
/* { dg-options "-fsanitize-recover=hwaddress" } */
/* { dg-set-target-env-var HWASAN_OPTIONS "halt_on_error=false" } */
/* { dg-shouldfail "hwasan" } */

#include <string.h>

volatile int ten = 16;

int main() {
  char x[10];
  __builtin_memset(x, 0, ten + 1);
  asm volatile ("" : : : "memory");
  volatile int res = x[ten];
  x[ten] = res + 3;
  res = x[ten];
  return 0;
}

/* { dg-output "WRITE of size 17 at 0x\[0-9a-f\]+.*" } */
/* { dg-output "READ of size 1 at 0x\[0-9a-f\]+.*" } */
/* { dg-output "WRITE of size 1 at 0x\[0-9a-f\]+.*" } */
/* { dg-output "READ of size 1 at 0x\[0-9a-f\]+.*" } */

