// { dg-do run }
// { dg-additional-options "-fsanitize=use-after-scope -fstack-reuse=none" }

int main(int argc, char **argv)
{
  int a = 123;

  if (argc == 0)
  {
    int *ptr;
    label:
      {
	ptr = &a;
        *ptr = 1;
	return 0;
      }
  }
  else
    goto label;

  return 0;
}
