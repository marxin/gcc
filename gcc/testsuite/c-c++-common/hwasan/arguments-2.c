/* { dg-do compile } */
/* { dg-additional-options "-fsanitize=kernel-address" } */
/* { dg-error ".*'-fsanitize=hwaddress' is incompatible with both '-fsanitize=address' and '-fsanitize=kernel-address'.*" "" { target *-*-* } 0 } */
