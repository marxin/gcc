/* { dg-do compile } */

void *ptr;

void foo(void)
{
 __builtin_malloc (1); /* { dg-warning "ignoring return value of '.*' declared with attribute 'warn_unused_result'" } */
 __builtin_calloc (10, 20); /* { dg-warning "ignoring return value of '.*' declared with attribute 'warn_unused_result'" } */
 __builtin_alloca (10); /* { dg-warning "ignoring return value of '.*' declared with attribute 'warn_unused_result'" } */
 __builtin_realloc (ptr, 100); /* { dg-warning "ignoring return value of '.*' declared with attribute 'warn_unused_result'" } */
 __builtin_aligned_alloc (10, 16); /* { dg-warning "ignoring return value of '.*' declared with attribute 'warn_unused_result'" } */
 __builtin_strdup ("pes"); /* { dg-warning "ignoring return value of '.*' declared with attribute 'warn_unused_result'" } */
 __builtin_strndup ("pes", 10); /* { dg-warning "ignoring return value of '.*' declared with attribute 'warn_unused_result'" } */
}
