/* Debug counter for debugging support
   Copyright (C) 2006-2019 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.

See dbgcnt.def for usage information.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "diagnostic-core.h"
#include "dumpfile.h"

#include "dbgcnt.h"

struct string2counter_map {
  const char *name;
  enum debug_counter counter;
};

#define DEBUG_COUNTER(a) { #a , a },

static struct string2counter_map map[debug_counter_number_of_counters] =
{
#include "dbgcnt.def"
};
#undef DEBUG_COUNTER

typedef std::pair<unsigned int, unsigned int> limit_tuple;

static auto_vec<limit_tuple> *limits[debug_counter_number_of_counters] = {NULL};

static unsigned int count[debug_counter_number_of_counters];

static void
print_limit_reach (const char *counter, int limit, bool upper_p)
{
  char buffer[128];
  sprintf (buffer, "***dbgcnt: %s limit %d reached for %s.***\n",
	   upper_p ? "upper" : "lower", limit, counter);
  fputs (buffer, stderr);
  if (dump_file)
    fputs (buffer, dump_file);
}

bool
dbg_cnt (enum debug_counter index)
{
  unsigned v = ++count[index];

  if (limits[index] == NULL)
    return true;
  else if (limits[index]->is_empty ())
    return false;

  /* Reverse intervals exactly once.  */
  if (v == 1)
    limits[index]->reverse ();

  unsigned last = limits[index]->length () - 1;
  unsigned int min = (*limits[index])[last].first;
  unsigned int max = (*limits[index])[last].second;

  if (v < min)
    return false;
  else if (v == min)
    {
      print_limit_reach (map[index].name, v, false);
      if (min == max)
	limits[index]->pop ();
      return true;
    }
  else if (v < max)
    return true;
  else if (v == max)
    {
      print_limit_reach (map[index].name, v, true);
      limits[index]->pop ();
      return true;
    }
  else
    return false;
}

static bool
dbg_cnt_set_limit_by_index (enum debug_counter index, const char *name,
			    unsigned int low, unsigned int high)
{
  if (limits[index] == NULL)
    limits[index] = new auto_vec<limit_tuple> ();

  if (!limits[index]->is_empty ())
    {
      auto_vec<limit_tuple> *intervals = limits[index];
      unsigned int last_max = (*intervals)[intervals->length () - 1].second;
      if (last_max >= low)
	{
	  error ("Interval minimum %d of %<-fdbg-cnt=%s%> is smaller or equal "
		 "to previous value %u\n",
		 low, name, last_max);
	  return false;
	}
    }

  limits[index]->safe_push (limit_tuple (low, high));
  return true;
}

static bool
dbg_cnt_set_limit_by_name (const char *name, unsigned int low,
			   unsigned int high)
{
  if (high < low)
    {
      error ("%<-fdbg-cnt=%s:%d-%d%> has smaller upper limit than the lower",
	     name, low, high);
      return false;
    }

  int i;
  for (i = debug_counter_number_of_counters - 1; i >= 0; i--)
    if (strcmp (map[i].name, name) == 0)
      break;

  if (i < 0)
    return false;

  return dbg_cnt_set_limit_by_index ((enum debug_counter) i, name, low, high);
}

/* Process a single "low:high" pair.
   Returns NULL if there's no valid pair is found.
   Otherwise returns a pointer to the end of the pair. */

static bool
dbg_cnt_process_single_pair (char *name, char *str)
{
  char *value1 = strtok (str, "-");
  char *value2 = strtok (NULL, "-");

  unsigned int high, low;

  if (value1 == NULL)
    return false;

  if (value2 == NULL)
    {
      low = 1;
      high = strtol (value1, NULL, 10);
    }
  else
    {
      low = strtol (value1, NULL, 10);
      high = strtol (value2, NULL, 10);
    }

  return dbg_cnt_set_limit_by_name (name, low, high);
}

void
dbg_cnt_process_opt (const char *arg)
{
  char *str = xstrdup (arg);
  unsigned int start = 0;

  auto_vec<char *> tokens;
  for (char *next = strtok (str, ","); next != NULL; next = strtok (NULL, ","))
    tokens.safe_push (next);

  unsigned i;
  for (i = 0; i < tokens.length (); i++)
    {
      auto_vec<char *> ranges;
      char *name = strtok (tokens[i], ":");
      for (char *part = strtok (NULL, ":"); part; part = strtok (NULL, ":"))
	ranges.safe_push (part);

      for (unsigned j = 0; j < ranges.length (); j++)
	{
	  if (!dbg_cnt_process_single_pair (name, ranges[j]))
	    break;
	}
      start += strlen (tokens[i]) + 1;
    }

   if (i != tokens.length ())
     {
       char *buffer = XALLOCAVEC (char, start + 2);
       sprintf (buffer, "%*c", start + 1, '^');
       error ("cannot find a valid counter:value pair:");
       error ("%<-fdbg-cnt=%s%>", arg);
       error ("           %s", buffer);
     }
}

/* Print name, limit and count of all counters.   */

void
dbg_cnt_list_all_counters (void)
{
  int i;
  printf ("  %-30s %s\n", "counter name", "closed intervals");
  printf ("-----------------------------------------------------------------\n");
  for (i = 0; i < debug_counter_number_of_counters; i++)
    {
      printf ("  %-30s ", map[i].name);
      if (limits[i] != NULL)
	{
	  for (unsigned j = 0; j < limits[i]->length (); j++)
	    {
	      printf ("[%u, %u]", (*limits[i])[j].first,
		      (*limits[i])[j].second);
	      if (j != limits[i]->length () - 1)
		printf (", ");
	    }
	  putchar ('\n');
	}
      else
	printf ("unset\n");
    }
  printf ("\n");
}
