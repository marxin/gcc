/* { dg-options "-O2 -fdump-tree-switchconv" } */

int check(int param)
{
  switch (param) 
    {
    case -2:
      return 1;
    default:
      return 0;
    }
}

/* { dg-final { scan-tree-dump "Expanding GIMPLE switch as condition" "switchconv" } } */
