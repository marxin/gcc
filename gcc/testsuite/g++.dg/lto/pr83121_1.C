struct Environment { // { dg-lto-message "8: a type with different size is defined in another translation unit" }
  struct AsyncHooks { // { dg-lto-message "10: a different type is defined in another translation unit" }
    int providers_[1]; // { dg-lto-message "21: a field of same name but different type is defined in another translation unit" }
  };
  AsyncHooks async_hooks_;
};
void fn1() { Environment a; }
int main ()
{
}
