/* HSA traits.
   Copyright (C) 2015 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

////////////////////////////////////////////////////////////////////////////////

#ifndef HSA_TRAITS_H_
#define HSA_TRAITS_H_

struct hsa_kernel_runtime
{
  /* Pointer to a command queue associated with a kernel dispatch agent.  */
  void *queue;
  /* List of pointers where is a space passing of kernel arguments.  */
  void **kernarg_addresses;
  /* List of kernel objects.  */
  uint64_t *objects;
  /* List of sync signals, where we prepare a single for each called kernel.  */
  uint64_t *signals;
  /* List of sizes of private segments.  */
  uint32_t *private_segments_size;
  /* List of sizes of group segments.  */
  uint32_t *group_segments_size;
};

#endif // HSA_TRAITS_H_ 
