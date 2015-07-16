/* { dg-do compile } */
/* { dg-options "-O2 -ftree-tail-merge -fdump-tree-tail-merge" } */

int
f (int c, int b, int d)
{
  int r, e;

  if (c)
    r = b + d;
  else
    {
      e = b + d;
      r = e;
    }

  return r;
}

/* { dg-final { scan-tree-dump-times "if " 0 "tail-merge"} } */
/* { dg-final { scan-tree-dump-times "(?n)_.*\\+.*_" 1 "tail-merge"} } */
/* { dg-final { scan-tree-dump-not "Invalid sum" "tail-merge"} } */
