/* { dg-do compile } */
/* { dg-options "-O2 -fdump-tree-iftoswitch" } */

int global;
int foo ();

int main(int argc, char **argv)
{
  if (argc == 1)
    foo ();
  else if (argc == 2)
    {
      global += 1;
    }
  else if (argc == 3)
    {
      foo ();
      foo ();
    }
  else if (argc == 4)
    {
      foo ();
    }
  else if (argc == 5)
    {
      global = 2;
    }
  else
    global -= 123;

  global -= 12;
  return 0;
}

/* { dg-final { scan-tree-dump "Condition chain \\(at .*if-to-switch-1.c:9\\) with 5 conditions transformed into a switch statement." "iftoswitch" } } */
