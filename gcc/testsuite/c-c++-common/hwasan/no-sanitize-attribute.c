/* { dg-do compile } */

__attribute__((no_sanitize_hwaddress)) int
f1 (int *p, int *q)
{
  *p = 42;
  return *q;
}

__attribute__((no_sanitize("hwaddress"))) int
f2 (int *p, int *q)
{
  *p = 42;
  return *q;
}

/* Only have one instance of __hwasan, it is __hwasan_init (the module
 * constructor) there is no instrumentation in the functions.  */
/* { dg-final { scan-assembler-times "__hwasan" 1 } } */
