/* PR gcov-profile/84758 */
/* { dg-options "-fprofile-arcs -ftest-coverage" } */
/* { dg-do run { target native } } */

int a = 0;

void foo()
{
  a = 1;			      /* count(1) */
}

void bar()
{
  a++;				      /* count(1) */
}

int main()
{
  foo (); goto baz; lab: bar ();      /* count(2) */

  baz:				      /* count(2) */
    if (a == 1)			      /* count(2) */
      goto lab;			      /* count(1) */

  return 0;
}

// { dg-final { run-gcov pr84758-2.C } }
