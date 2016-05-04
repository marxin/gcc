// { dg-do run }
// { dg-additional-options "-fsanitize-address-use-after-scope" }
// { dg-shouldfail "asan" }

#include <functional>

int main() {
  std::function<int()> function;
  {
    int v = 0;
    function = [&v]()
    {
      return v;
    };
  }
  return function();
}


// { dg-output "ERROR: AddressSanitizer: stack-use-after-scope on address.*(\n|\r\n|\r)" }
// { dg-output "READ of size 4 at.*" }
// { dg-output ".*'v' <== Memory access at offset \[0-9\]* is inside this variable.*" }
