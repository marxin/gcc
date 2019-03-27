/* { dg-do compile } */
/* { dg-options "-fdbg-cnt=vect_loop:1:2,vect_slp:2,merged_ipa_icf:7:8:dbg-cnt-1" } */
/* { dg-additional-options "-mavx2" { target { i?86-*-* x86_64-*-* } } } */
/* { dg-prune-output "dbg_cnt 'vect_loop' set to 1-2" } */
/* { dg-prune-output "dbg_cnt 'vect_slp' set to 0-2" } */
/* { dg-prune-output "dbg_cnt 'merged_ipa_icf' set to 7-8" } */
