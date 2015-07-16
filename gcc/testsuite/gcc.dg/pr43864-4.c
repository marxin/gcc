/* { dg-do compile } */
/* { dg-options "-O2 -ftree-tail-merge -fdump-tree-tail-merge" } */

/* Different stmt order.  */

int f(int c, int b, int d)
{
  int r, r2, e;

  if (c)
    {
      r = b + d;
      r2 = d - b;
    }
  else
    {
      e = d + b;
      r2 = d - b;
      r = e;
    }

  return r - r2;
}

/* { dg-final { scan-tree-dump-times "if " 0 "tail-merge"} } */
/* { dg-final { scan-tree-dump-times "(?n)_.*\\+.*_" 1 "tail-merge"} } */
/* { dg-final { scan-tree-dump-times "(?n)_.*-.*_" 2 "tail-merge"} } */
/* { dg-final { scan-tree-dump-not "Invalid sum" "tail-merge"} } */
