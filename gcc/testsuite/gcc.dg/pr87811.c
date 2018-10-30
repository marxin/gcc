/* { dg-do compile } */
/* { dg-options "-O2 -fdump-tree-profile_estimate -frounding-math" } */

void bar (void);

void
foo (int i, double d)
{
  if (__builtin_expect_with_probability (i, 0, d))
    bar ();
}

/* { dg-final { scan-tree-dump-not "__builtin_expect_with_probability heuristics of edge" "profile_estimate"} } */
