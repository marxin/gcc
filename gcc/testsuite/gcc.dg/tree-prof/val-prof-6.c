/* { dg-options "-O2 -fdump-ipa-profile-details" } */
char a[1000];
char b[1000];
int size=1000;
__attribute__ ((noinline)) void
t(int size)
{
  __builtin_memcpy(a,b,size);
}
int
main()
{
  int i;
  for (i=0; i < size; i++)
    t(i);
  return 0;
}
/* autofdo does not do value profiling so far */
/* { dg-final-use-not-autofdo { scan-tree-dump "Average value sum:499500" "profile"} } */
/* { dg-final-use-not-autofdo { scan-tree-dump "IOR value" "profile"} } */
