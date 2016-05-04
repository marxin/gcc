// { dg-do run }
// { dg-additional-options "-fsanitize=use-after-scope -fstack-reuse=none" }
// { dg-shouldfail "asan" }

int
main (void)
{
  char *ptr;
  {
    char my_char[9];
    ptr = &my_char[0];
  }

  *(ptr+9) = 'c';
}

// { dg-output "ERROR: AddressSanitizer: stack-use-after-scope on address.*(\n|\r\n|\r)" }
// { dg-output "WRITE of size 1 at.*" }
// { dg-output ".*'my_char' <== Memory access at offset \[0-9\]* overflows this variable.*" }
