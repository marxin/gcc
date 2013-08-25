/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

class A
{
  public:
    __attribute__ ((noinline)) 
    virtual int Foo2()
    {
      return v;
    }

    float f;
    int v;
};

class B
{
  public:
    __attribute__ ((noinline))
    int Bar2()
    {
      return v;
    }

    float f, aaa;
    int v;
};

int main()
{
  A a;
  B b;

  a.Foo2();
  b.Bar2();

  return 12345;
}

/* { dg-final { scan-ipa-dump-not "Semantic equality hit:" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 0" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
