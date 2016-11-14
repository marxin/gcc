// { dg-do run }
// { dg-shouldfail "asan" }
// { dg-additional-options "-O2" }

int
main (int argc, char **argv)
{
  int *ptr = 0;

  {
    int a;
    ptr = &a;
    *ptr = 12345;
  }

  return *ptr;
}

// { dg-output "ERROR: AddressSanitizer: stack-use-after-scope at pc.*(\n|\r\n|\r)" }
// { dg-output "ACCESS of size .* for variable 'a'" }
