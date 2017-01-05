/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-icf"  } */

int
__attribute__((hot, no_reorder))
foo(void) { return 0; }

int
__attribute__((no_reorder, hot))
bar (void) { return 0; }

/* { dg-final { scan-ipa-dump "Equal symbols: 1" "icf"  } } */
/* { dg-final { scan-ipa-dump "Semantic equality hit:bar->foo" "icf"  } } */
