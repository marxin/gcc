/* PR ipa/92799 */
/* { dg-do compile } */
/* { dg-require-weak "" } */

static void __attribute__ ( ( weakref ( "bar" ) ) ) foo ( void ) { }  /* { dg-warning "attribute ignored because function is defined" } */
extern void foo ( void );
