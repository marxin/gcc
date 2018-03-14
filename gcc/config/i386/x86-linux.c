/* Implementation for linux-specific functions for i386 and x86-64 systems.
   Copyright (C) 2018 Free Software Foundation, Inc.
   Contributed by Martin Liska <mliska@suse.cz>.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#define IN_TARGET_CODE 1
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"

/* This hook determines whether a function from libc has a fast implementation
   FN is present at the runtime.  We override it for i386 and glibc C library
   as this combination provides fast implementation of mempcpy function.  */

enum libc_speed
ix86_linux_libc_func_speed (int fn)
{
  enum built_in_function f = (built_in_function)fn;

  if (!OPTION_GLIBC)
    return LIBC_UNKNOWN_SPEED;

  switch (f)
    {
      case BUILT_IN_MEMPCPY:
	return LIBC_FAST_SPEED;
      default:
	return LIBC_UNKNOWN_SPEED;
    }
}
