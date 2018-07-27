/* { dg-do compile } */
/* { dg-options "-O2 -fdump-tree-profile_estimate" } */

#include <stdlib.h>
#include <string.h>

void *r;
void *r2;
void *r3;
void *r4;

void *m (size_t s, int c)
{
  r = malloc (s);
  if (__builtin_unpredictable (r == 0))
    memset (r, 0, s);
}

/* { dg-final { scan-tree-dump "__builtin_unpredictable heuristics of edge\[^:\]*: 50.00%" "profile_estimate"} } */
/* { dg-final { scan-tree-dump "combined heuristics: 50.00%" "profile_estimate"} } */
