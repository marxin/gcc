/* { dg-do compile { target { ! x32 } } } */
/* { dg-options "-fcheck-pointer-bounds -mmpx -mabi=ms" } */

void a (_Complex) { a (3); }
