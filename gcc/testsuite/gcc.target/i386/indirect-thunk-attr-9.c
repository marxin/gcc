/* { dg-do compile { target { lp64 } } } */
/* { dg-options "-O2 -mindirect-branch=keep -mfunction-return=keep -mcmodel=large" } */

__attribute__ ((indirect_branch("thunk")))
void
bar (void)
{ /* { dg-error "'-mindirect-branch=thunk' and '-mcmodel=large' are not compatible" } */
}
