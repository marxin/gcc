/* { dg-do run } */
/* { dg-options "-fno-common" } */
/* { dg-shouldfail "asan" } */

volatile int var;

int main() {
  return *(&var-1);
}

/* { dg-output "READ of size 4 at 0x\[0-9a-f\]+ thread T0.*(\n|\r\n|\r)" } */
/* { dg-output "    #0 0x\[0-9a-f\]+ +(in _*main (\[^\n\r]*global-overflow-3.c:8|\[^\n\r]*:0|\[^\n\r]*\\+0x\[0-9a-z\]*)|\[(\])\[^\n\r]*(\n|\r\n|\r).*" } */
/* { dg-output "0x\[0-9a-f\]+ is located 4 bytes to the left of global variable" } */
/* { dg-output ".*var\[^\n\r]* of size 4\[^\n\r]*(\n|\r\n|\r)" } */
