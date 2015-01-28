/* { dg-do compile } */
/* { dg-options "-O0 -fipa-icf -fdump-ipa-icf-details"  } */

#include <stdlib.h>
#include <assert.h>

int callback1(int a)
{
  return a * a;
}

int callback2(int a)
{
  return a * a;
}

static int test(int (*callback) (int))
{
  if (callback == callback1)
    return 1;

  return 0;
}

int foo()
{
  return test(&callback1);
}

int bar()
{
  return test(&callback2);
}

int main()
{
  assert (foo() != bar());

  return 0;
}

/* { dg-final { scan-ipa-dump "Equal symbols: 2" "icf"  } } */
/* { dg-final { scan-ipa-dump "A function from the congruence class has address taken." "icf"  } } */
/* { dg-final { cleanup-ipa-dump "icf" } } */
