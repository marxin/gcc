/* Test gcov block mode.  */

/* { dg-options "-fprofile-arcs -ftest-coverage" } */
/* { dg-do run { target native } } */

unsigned int
UuT (void)
{
  unsigned int true_var = 1;
  unsigned int false_var = 0;
  unsigned int ret = 0;

  if (true_var) /* count(1) */
    {
      if (false_var) /* count(1) */
	ret = 111; /* count(#####) */
    }
  else
    ret = 999; /* count(#####) */
  return ret;
}

int
main (int argc, char **argv)
{
  UuT ();
  return 0;
}

/* { dg-final { run-gcov { -a gcov-17.c } } } */
