/* Routines required for instrumenting a program.  */
/* Compile this one with gcc.  */
/* Copyright (C) 1989-2017 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#include "libgcov.h"

#if defined(inhibit_libc)
/* If libc and its header files are not available, provide dummy functions.  */

#ifdef L_gcov_merge_add
void __gcov_merge_add (gcov_type *counters  __attribute__ ((unused)),
                       unsigned n_counters __attribute__ ((unused))) {}
#endif

#ifdef L_gcov_merge_single
void __gcov_merge_single (gcov_type *counters  __attribute__ ((unused)),
                          unsigned n_counters __attribute__ ((unused))) {}
#endif

#else

#ifdef L_gcov_merge_add
/* The profile merging function that just adds the counters.  It is given
   an array COUNTERS of N_COUNTERS old counters and it reads the same number
   of counters from the gcov file.  */
void
__gcov_merge_add (gcov_type *counters, unsigned n_counters)
{
  for (; n_counters; counters++, n_counters--)
    *counters += gcov_get_counter ();
}
#endif /* L_gcov_merge_add */

#ifdef L_gcov_merge_ior
/* The profile merging function that just adds the counters.  It is given
   an array COUNTERS of N_COUNTERS old counters and it reads the same number
   of counters from the gcov file.  */
void
__gcov_merge_ior (gcov_type *counters, unsigned n_counters)
{
  for (; n_counters; counters++, n_counters--)
    *counters |= gcov_get_counter_target ();
}
#endif

#ifdef L_gcov_merge_time_profile
/* Time profiles are merged so that minimum from all valid (greater than zero)
   is stored. There could be a fork that creates new counters. To have
   the profile stable, we chosen to pick the smallest function visit time.  */
void
__gcov_merge_time_profile (gcov_type *counters, unsigned n_counters)
{
  unsigned int i;
  gcov_type value;

  for (i = 0; i < n_counters; i++)
    {
      value = gcov_get_counter_target ();

      if (value && (!counters[i] || value < counters[i]))
        counters[i] = value;
    }
}
#endif /* L_gcov_merge_time_profile */

#ifdef L_gcov_merge_single
/* The profile merging function for choosing the most common value.
   It is given an array COUNTERS of N_COUNTERS old counters and it
   reads the same number of counters from the gcov file.  The counters
   are split into 3-tuples where the members of the tuple have
   meanings:

   -- the stored candidate on the most common value of the measured entity
   -- counter
   -- total number of evaluations of the value  */
void
__gcov_merge_single (gcov_type *counters, unsigned n_counters)
{
  unsigned i, n_measures;
  gcov_type value, counter, all;

  gcc_assert (!(n_counters % 3));
  n_measures = n_counters / 3;
  for (i = 0; i < n_measures; i++, counters += 3)
    {
      value = gcov_get_counter_target ();
      counter = gcov_get_counter ();
      all = gcov_get_counter ();

      if (counters[0] == value)
        counters[1] += counter;
      else if (counter > counters[1])
        {
          counters[0] = value;
          counters[1] = counter - counters[1];
        }
      else
        counters[1] -= counter;
      counters[2] += all;
    }
}
#endif /* L_gcov_merge_single */

#ifdef L_gcov_merge_topn
/* The profile merging top N mostly used values.
   This function is given array COUNTERS of N_COUNTERS old counters and it
   reads the same number of counters from the gcov file.  */

void
__gcov_merge_topn (gcov_type *counters, unsigned n_counters)
{
  size_t data_size = GCOV_TOPN_NCOUNTS * sizeof (gcov_type);
  gcov_type *tmp_array = (gcov_type *) alloca (2 * data_size);

  gcc_assert (!(n_counters % GCOV_TOPN_NCOUNTS));
  for (unsigned i = 0; i < n_counters; i += GCOV_TOPN_NCOUNTS)
    {
      gcov_type *value_array = &counters[i];

      memcpy (tmp_array, value_array, data_size);
      memset (tmp_array + data_size, 0, data_size);
      unsigned j = GCOV_TOPN_NCOUNTS;

      for (unsigned k = 0; k < GCOV_TOPN_NCOUNTS; k += 2)
        {
          int found = 0;
          gcov_type value = gcov_get_counter_target ();
          gcov_type count = gcov_get_counter ();
          for (unsigned m = 0; m < j; m += 2)
            {
              if (tmp_array[m] == value)
                {
                  found = 1;
                  tmp_array[m + 1] += count;
                  break;
                }
            }
          if (!found)
            {
              tmp_array[j] = value;
              tmp_array[j + 1] = count;
              j += 2;
            }
        }
      /* Now sort the temp array */
      gcov_sort_n_vals (tmp_array, j);

      /* Now copy back the top half of the temp array */
      memcpy (value_array, tmp_array, data_size);
    }
}

void
__gcov_merge_icall_topn (gcov_type *counters, unsigned n_counters)
{
  __gcov_merge_topn (counters, n_counters);
}

#endif /* L_gcov_merge_topn */
#endif /* inhibit_libc */
