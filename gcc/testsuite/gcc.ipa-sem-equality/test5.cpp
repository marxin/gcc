void f1(int x)
{
  try
  {    
    throw x;
  }
  catch(int e)
  {
  }
}

void f2(int x)
{
  try
  {    
    throw x;
  }
  catch(int e)
  {
  }
}

int main(int argc, char **argv)
{
  f1(argc);

  return 0;
}
