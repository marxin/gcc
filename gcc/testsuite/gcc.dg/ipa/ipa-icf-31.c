/* { dg-do compile } */
/* { dg-options "-fipa-icf -fdump-ipa-icf-details"  } */


static int f(int t, int *a) __attribute__((noinline));

static int g(int t, volatile int *a) __attribute__((noinline));
static int g(int t, volatile int *a)
{
  int i;
  int tt = 0;
  for(i=0;i<t;i++)
    tt += *a;
  return tt;
}
static int f(int t, int *a)
{
  int i;
  int tt = 0;
  for(i=0;i<t;i++)
    tt += *a;
  return tt;
}


int main()
{
  return 0;
}

/* { dg-final { scan-ipa-dump "Equal symbols: 0" "icf"  } } */
/* { dg-final { scan-ipa-dump "different volatility for assignment statement" "icf"  } } */
/* { dg-final { cleanup-ipa-dump "icf" } } */
