/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-icf"  } */

int
__attribute__((optimize(("split-loops"))))
foo(void) { return 0; }

int
__attribute__((optimize(("split-loops"))))
bar (void) { return 0; }


int
__attribute__ ((hot))
foo2(void) { return 0; }

int
__attribute__ ((hot))
bar2(void) { return 0; }

/* { dg-final { scan-ipa-dump "Equal symbols: 1" "icf"  } } */
/* { dg-final { scan-ipa-dump "Semantic equality hit:bar2->foo2" "icf"  } } */
