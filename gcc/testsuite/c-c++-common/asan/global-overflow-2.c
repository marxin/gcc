/* { dg-do run } */
/* { dg-shouldfail "asan" } */

const char c1[] = "abcdefg";

int main() {
  return *(&c1[0]-1);
}

/* { dg-output "READ of size 1 at 0x\[0-9a-f\]+ thread T0.*(\n|\r\n|\r)" } */
/* { dg-output "    #0 0x\[0-9a-f\]+ +(in _*main (\[^\n\r]*global-overflow-2.c:7|\[^\n\r]*:0|\[^\n\r]*\\+0x\[0-9a-z\]*)|\[(\])\[^\n\r]*(\n|\r\n|\r).*" } */
/* { dg-output "0x\[0-9a-f\]+ is located 1 bytes to the left of global variable" } */
/* { dg-output ".*c1\[^\n\r]* of size 8\[^\n\r]*(\n|\r\n|\r)" } */
