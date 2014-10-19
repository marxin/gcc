/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-icf-details"  } */

static int f(int t) __attribute__((noinline));

static int g(int t) __attribute__((noinline));
static int g(int t)
{
    asm("addl %0, 1": "+r"(t));  
      return t;
}
static int f(int t)
{
    asm("addq %0, -1": "+r"(t));
      return t;
}


int h(int t)
{
    return f(t) + g(t);
}

/* { dg-final { scan-ipa-dump "ASM strings are different" "icf"  } } */
/* { dg-final { scan-ipa-dump "Equal symbols: 0" "icf"  } } */
/* { dg-final { cleanup-ipa-dump "icf" } } */
