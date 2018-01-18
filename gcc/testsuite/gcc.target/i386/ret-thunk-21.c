/* { dg-do compile { target { lp64 } } } */
/* { dg-options "-O2 -mfunction-return=keep -mindirect-branch=keep -mcmodel=large" } */

__attribute__ ((function_return("thunk-inline")))
void
bar (void)
{
}
