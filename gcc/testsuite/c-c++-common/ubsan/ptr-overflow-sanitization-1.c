/* { dg-require-effective-target lp64 } */
/* { dg-options "-O -fsanitize=pointer-overflow -fdump-tree-sanopt-details" } */

void foo(void)
{
  char *p;
  char *p2;
  char b[1];
  char c[1];

  p = b + 9223372036854775807LL; /* pointer overflow check is needed */
  p = b;
  p++;
  p2 = p + 1000;
  p2 = p + 999;

  p = b + 9223372036854775807LL;
  p2 = p + 1; /* pointer overflow check is needed */

  p = b;
  p--; /* pointer overflow check is needed */
  p2 = p + 1;
  p2 = p + 2;

  p = b - 9223372036854775807LL; /* pointer overflow check is needed */
  p2 = p + 9223372036854775805LL; /* b - 2 */
  p2 = p + 9223372036854775806LL; /* b - 1 */
  p2 = p + (9223372036854775807LL); /* b */
  p2++; /* b + 1 */

  p = c;
  p++; /* c + 1 */
  p = c - 9223372036854775807LL; /* pointer overflow check is needed */
  p2 = p + (9223372036854775807LL); /* c */
  p2++; /* c + 1 */
}

// { dg-final { scan-tree-dump-times "Leaving" 5 "sanopt"} }
