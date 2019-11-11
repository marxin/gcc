/* { dg-do run } */
/* TODO Ensure this test has enough optimisation to get RVO. */

#define assert(x) if (!(x)) __builtin_abort ()

struct big_struct {
    int left;
    int right;
    void *ptr;
    int big_array[100];
};

/*
   Tests for RVO (basically, checking -fsanitize=hwaddress has not broken RVO
   in any way).

   0) The value is accessible in both functions without a hwasan complaint.
   1) RVO does happen.
 */

struct big_struct __attribute__ ((noinline))
return_on_stack()
{
  struct big_struct x;
  x.left = 100;
  x.right = 20;
  x.big_array[10] = 30;
  x.ptr = &x;
  return x;
}

struct big_struct __attribute__ ((noinline))
unnamed_return_on_stack()
{
  return (struct big_struct){
      .left = 100,
      .right = 20,
      .ptr = __builtin_frame_address (0),
      .big_array = {0}
  };
}

int main()
{
  struct big_struct x;
  x = return_on_stack();
  /* Check that RVO happens by checking the address that the callee saw.  */
  assert (x.ptr == &x);
  struct big_struct y;
  y = unnamed_return_on_stack();
  /* Know only running this on AArch64, which means stack grows downwards,
     We're checking that the frame of the callee function is below the address
     of this variable, which means that the callee function used RVO.  */
  assert (y.ptr < (void *)&y);
  return 0;
}
