/* { dg-do compile } */
/* { dg-additional-options "-fsanitize=address" } */
/* { dg-error ".*'-fsanitize=hwaddress' is incompatible with both '-fsanitize=address' and '-fsanitize=kernel-address'.*" "" { target *-*-* } 0 } */
