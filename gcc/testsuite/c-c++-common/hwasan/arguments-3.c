/* { dg-do compile } */
/* { dg-additional-options "-fsanitize=thread" } */
/* { dg-error ".*'-fsanitize=hwaddress' is incompatible with '-fsanitize=thread'.*" "" { target *-*-* } 0 } */
