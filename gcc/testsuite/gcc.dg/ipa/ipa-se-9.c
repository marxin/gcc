/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

int funkce(int a, int b) __attribute__ ((pure));

__attribute__ ((noinline))
int ferda(int x, int y)
{
  if (x < y)
  {
    return x;
  }
  else
    return y;
}

__attribute__ ((noinline))
int funkce(int a, int b)
{
  if(a < b)
    return a;
  else
    return b;
}

int main(int argc, char **argv)
{
  return 0;
}

/* { dg-final { scan-ipa-dump-not "Semantic equality hit:" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 0" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
