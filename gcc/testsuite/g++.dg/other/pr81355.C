/* { dg-do compile { target x86_64-*-* } } */

__attribute__((target("default")))
int foo() {return 1;}

__attribute__((target("arch=core2", "")))
int foo() {return 2;}

__attribute__((target("sse4.2", "", "")))
int foo() {return 2;}

int main() {
    return foo();
}
