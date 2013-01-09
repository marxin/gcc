class SuperObject
{
  public:
    static int RandomNumber()
    {
      return 123;
    }
};

template <class myType>
myType GetMax(myType a, myType b)
{
  return a > b ? a : b;
}

int main(int argc, char **argv)
{
  long x = GetMax<long>(2, 4);
  int y = GetMax<int>(2, 4);

  return 0;
}
