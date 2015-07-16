/* { dg-do compile } */
/* { dg-options "-O2 -ftree-tail-merge -fdump-tree-tail-merge" } */

int bar (int);
void baz (int);

void
foo (int y)
{
  int a;
  if (y)
    baz (bar (7) + 6);
  else
    baz (bar (7) + 6);
}

/* { dg-final { scan-tree-dump-times "bar \\(" 1 "tail-merge"} } */
/* { dg-final { scan-tree-dump-times "baz \\(" 1 "tail-merge"} } */
