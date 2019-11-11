/* { dg-do run } */

/* This version should catch the invalid access.  */
#define CLEARED_ACCESS_CATCH
#include "try-catch-0.cpp"
#undef CLEARED_ACCESS_CATCH
