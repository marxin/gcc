/* PR preprocessor/78497 */
/* { dg-do compile } */
/* { dg-options "-Wimplicit-fallthrough --save-temps" } */

int main (int argc, char **argv)
{
  int a;
  switch (argc)
    {
    case 1:
      a = 1;
      break;
    case 2:
      a = 2;
      /* FALLTHROUGH */
    case 3:
      a = 3;
      break;
    }

  return a;
}
