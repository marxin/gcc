/* { dg-do run } */
/* { dg-shouldfail "hwasan" } */
/* Don't really need this option since there are no vararray/alloca objects in
   the interesting function, however it never hurts to make doubly sure and
   make it explicit that we're checking the alternate approach to deallocation.
   */
/* { dg-additional-options "--param hwasan-instrument-allocas=0" } */

/* Handling large aligned variables.
   Large aligned variables take a different code-path through expand_stack_vars
   in cfgexpand.c.  This testcase is just to exercise that code-path.

   The alternate code-path produces a second base-pointer through some
   instructions emitted in the prologue.

   This eventually follows a different code path for untagging when not tagging
   allocas. The untagging needs to work at the top of the frame, and this
   should account for this different base when large aligned variables are
   around.  */
__attribute__ ((noinline))
void * Ident (void * argument)
{
  return argument;
}

int* __attribute__ ((noinline))
large_alignment_untagging (int num)
{
  int other_array[100];
  int big_array[100] __attribute__ ((aligned (32)));
  if (num % 1)
    return (int*)Ident(big_array);
  else
    return (int*)Ident(other_array);
}

#ifndef ARG
#define ARG 1
#endif

int global;

int __attribute__ ((noinline))
main ()
{
  int *array = large_alignment_untagging (ARG);
  /* Want to test that both ends of the function are untagged.
     That means both the start of the large aligned variable and the end of the
     other variable.  */
  global += array[ARG ? 99 : 0];
  return 0;
}

/* { dg-output "HWAddressSanitizer: tag-mismatch on address 0x\[0-9a-f\]*.*" } */
/* NOTE: This assumes the current tagging mechanism (one at a time from the
   base and large aligned variables being handled first).  */
/* { dg-output "READ of size 4 at 0x\[0-9a-f\]* tags: \[\[:xdigit:\]\]\[\[:xdigit:\]\]/\[\[:xdigit:\]\]\[\[:xdigit:\]\] \\(ptr/mem\\) in thread T0.*" } */
/* { dg-output "Address 0x\[0-9a-f\]* is located in stack of thread T0.*" } */
/* { dg-output "SUMMARY: HWAddressSanitizer: tag-mismatch \[^\n\]*.*" } */
