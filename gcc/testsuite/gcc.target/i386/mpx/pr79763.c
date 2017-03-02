/* { dg-do compile { target { ! x32 } } } */
/* { dg-options "-fcheck-pointer-bounds -mmpx -mabi=ms" } */

int q_sk_num(void *a);
typedef int (*fptr)(double);
void a() { ((fptr)q_sk_num)(0); } /* { dg-warning "function called through a non-compatible type" } */
