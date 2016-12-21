// { dg-do run }
// { dg-options "-fsanitize=return" }
// { dg-shouldfail "ubsan" }

struct S { S (); ~S (); };

S::S () {}
S::~S () {}

int
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
foo (int x)
{
  S a;
  {
    S b;
    if (x)
      return 1;
  }
}

int
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
main ()
{
  foo (0);
}

// { dg-output "execution reached the end of a value-returning function without returning a value" }
