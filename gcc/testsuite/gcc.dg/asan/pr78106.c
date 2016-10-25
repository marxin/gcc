/* PR sanitizer/78106 */
/* { dg-do run } */
/* { dg-options "-fsanitize=address" } */
/* { dg-shouldfail "asan" } */

int *variable;

void __attribute__((used)) release()
{
  __builtin_free (variable);
}

int main2(int argc)
{
  *variable = 2;

  if (argc <= 5)
    asm volatile ("call release");

  *variable = 2;
  __builtin_abort ();

  return 0;
}

int main(int argc, char **argv)
{
  variable = __builtin_malloc (sizeof(int));
  return main2(argc);
}

/* { dg-output "ERROR: AddressSanitizer:? heap-use-after-free on address.*(\n|\r\n|\r)" } */
/* { dg-output "WRITE of size 4 at.*" } */
/* { dg-output "    #0 0x\[0-9a-f\]+ +in _*main2 .*" } */
