/* { dg-options "-O2 -fdump-tree-optimized -fdump-ipa-profile-optimized --param=profile-topn-invalid-threshold=1" } */
unsigned int a[1000];
unsigned int b = 99;
unsigned int c = 10022;
unsigned int d = 10033;
unsigned int e = 100444;
unsigned int f = 1005555;
int
main ()
{
  int i;
  unsigned int n;
  for (i = 0; i < 1000; i++)
    {
	    a[i]=1000+i;
    }
  for (i = 0; i < 1000; i++)
    {
      if (i % 100 == 1)
	n = b;
      else if (i % 100 == 2)
	n = c;
      else if (i % 100 == 3)
	n = d;
      else if (i % 100 == 4)
	n = e;
      else
	n = f;
      a[i] /= n;
    }
  return 0;
}
/* autofdo does not do value profiling so far */
/* { dg-final-use-not-autofdo { scan-ipa-dump-not "Transformation done: div.mod by constant 1005555" "profile"} } */
