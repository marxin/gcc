/* Utility functions for finding files relative to GCC binaries.
   Copyright (C) 1992-2017 Free Software Foundation, Inc.

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
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "filenames.h"
#include "file-find.h"
#include "selftest.h"

static bool debug = false;

void
find_file_set_debug (bool debug_state)
{
  debug = debug_state;
}

char *
find_a_file (struct path_prefix *pprefix, const char *name, int mode)
{
  char *temp;
  struct prefix_list *pl;
  int len = pprefix->max_len + strlen (name) + 1;

  if (debug)
    fprintf (stderr, "Looking for '%s'\n", name);

#ifdef HOST_EXECUTABLE_SUFFIX
  len += strlen (HOST_EXECUTABLE_SUFFIX);
#endif

  temp = XNEWVEC (char, len);

  /* Determine the filename to execute (special case for absolute paths).  */

  if (IS_ABSOLUTE_PATH (name))
    {
      if (access (name, mode) == 0)
	{
	  strcpy (temp, name);

	  if (debug)
	    fprintf (stderr, "  - found: absolute path\n");

	  return temp;
	}

#ifdef HOST_EXECUTABLE_SUFFIX
	/* Some systems have a suffix for executable files.
	   So try appending that.  */
      strcpy (temp, name);
	strcat (temp, HOST_EXECUTABLE_SUFFIX);

	if (access (temp, mode) == 0)
	  return temp;
#endif

      if (debug)
	fprintf (stderr, "  - failed to locate using absolute path\n");
    }
  else
    for (pl = pprefix->plist; pl; pl = pl->next)
      {
	struct stat st;

	strcpy (temp, pl->prefix);
	strcat (temp, name);

	if (stat (temp, &st) >= 0
	    && ! S_ISDIR (st.st_mode)
	    && access (temp, mode) == 0)
	  return temp;

#ifdef HOST_EXECUTABLE_SUFFIX
	/* Some systems have a suffix for executable files.
	   So try appending that.  */
	strcat (temp, HOST_EXECUTABLE_SUFFIX);

	if (stat (temp, &st) >= 0
	    && ! S_ISDIR (st.st_mode)
	    && access (temp, mode) == 0)
	  return temp;
#endif
      }

  if (debug && pprefix->plist == NULL)
    fprintf (stderr, "  - failed: no entries in prefix list\n");

  free (temp);
  return 0;
}

/* Add an entry for PREFIX to prefix list PREFIX.
   Add at beginning if FIRST is true.  */

void
do_add_prefix (struct path_prefix *pprefix, const char *prefix, bool first)
{
  struct prefix_list *pl, **prev;
  int len;

  if (pprefix->plist && !first)
    {
      for (pl = pprefix->plist; pl->next; pl = pl->next)
	;
      prev = &pl->next;
    }
  else
    prev = &pprefix->plist;

  /* Keep track of the longest prefix.  */

  len = strlen (prefix);
  bool append_separator = !IS_DIR_SEPARATOR (prefix[len - 1]);
  if (append_separator)
    len++;

  if (len > pprefix->max_len)
    pprefix->max_len = len;

  pl = XNEW (struct prefix_list);
  char *dup = XCNEWVEC (char, len + 1);
  memcpy (dup, prefix, append_separator ? len - 1 : len);
  if (append_separator)
    {
      dup[len - 1] = DIR_SEPARATOR;
      dup[len] = '\0';
    }
  pl->prefix = dup;

  if (*prev)
    pl->next = *prev;
  else
    pl->next = (struct prefix_list *) 0;
  *prev = pl;
}

/* Add an entry for PREFIX at the end of prefix list PREFIX.  */

void
add_prefix (struct path_prefix *pprefix, const char *prefix)
{
  do_add_prefix (pprefix, prefix, false);
}

/* Add an entry for PREFIX at the begin of prefix list PREFIX.  */

void
add_prefix_begin (struct path_prefix *pprefix, const char *prefix)
{
  do_add_prefix (pprefix, prefix, true);
}

/* Take the value of the environment variable ENV, break it into a path, and
   add of the entries to PPREFIX.  */

void
prefix_from_env (const char *env, struct path_prefix *pprefix)
{
  const char *p;
  p = getenv (env);

  if (p)
    prefix_from_string (p, pprefix);
}

void
prefix_from_string (const char *p, struct path_prefix *pprefix)
{
  const char *startp, *endp;
  char *nstore = XNEWVEC (char, strlen (p) + 3);

  if (debug)
    fprintf (stderr, "Convert string '%s' into prefixes, separator = '%c'\n", p, PATH_SEPARATOR);

  startp = endp = p;
  while (1)
    {
      if (*endp == PATH_SEPARATOR || *endp == 0)
	{
	  strncpy (nstore, startp, endp-startp);
	  if (endp == startp)
	    {
	      strcpy (nstore, "./");
	    }
	  else if (! IS_DIR_SEPARATOR (endp[-1]))
	    {
	      nstore[endp-startp] = DIR_SEPARATOR;
	      nstore[endp-startp+1] = 0;
	    }
	  else
	    nstore[endp-startp] = 0;

	  if (debug)
	    fprintf (stderr, "  - add prefix: %s\n", nstore);

	  add_prefix (pprefix, nstore);
	  if (*endp == 0)
	    break;
	  endp = startp = endp + 1;
	}
      else
	endp++;
    }
  free (nstore);
}

#if CHECKING_P

namespace selftest {

/* Encode '#' and '_' to path and dir separators in order to test portability
   of the test-cases.  */

static char *
purge (const char *input)
{
  char *s = xstrdup (input);
  for (char *c = s; *c != '\0'; c++)
    switch (*c)
      {
      case '/':
      case ':':
	*c = 'a'; /* Poison default string values.  */
	break;
      case '_':
	*c = PATH_SEPARATOR;
	break;
      case '#':
	*c = DIR_SEPARATOR;
	break;
      default:
	break;
      }

  return s;
}

const char *env1 = purge ("#home#user#bin_#home#user#bin_#bin_#usr#bin");
const char *env2 = purge ("#root_#root_#root");

/* Verify creation of prefix.  */

static void
file_find_verify_prefix_creation (void)
{
  path_prefix prefix;
  memset (&prefix, 0, sizeof (prefix));
  prefix_from_string (env1, &prefix);

  ASSERT_EQ (15, prefix.max_len);

  /* All prefixes end with DIR_SEPARATOR.  */
  ASSERT_STREQ (purge ("#home#user#bin#"), prefix.plist->prefix);
  ASSERT_STREQ (purge ("#home#user#bin#"), prefix.plist->next->prefix);
  ASSERT_STREQ (purge ("#bin#"), prefix.plist->next->next->prefix);
  ASSERT_STREQ (purge ("#usr#bin#"), prefix.plist->next->next->next->prefix);
  ASSERT_EQ (NULL, prefix.plist->next->next->next->next);
}

/* Verify adding a prefix.  */

static void
file_find_verify_prefix_add (void)
{
  path_prefix prefix;
  memset (&prefix, 0, sizeof (prefix));
  prefix_from_string (env1, &prefix);

  add_prefix (&prefix, purge ("#root"));
  ASSERT_STREQ (purge ("#home#user#bin#"), prefix.plist->prefix);
  ASSERT_STREQ (purge ("#root#"),
		prefix.plist->next->next->next->next->prefix);

  add_prefix_begin (&prefix, purge ("#var"));
  ASSERT_STREQ (purge ("#var#"), prefix.plist->prefix);
}

/* Run all of the selftests within this file.  */

void file_find_c_tests ()
{
  file_find_verify_prefix_creation ();
  file_find_verify_prefix_add ();
}

} // namespace selftest
#endif /* CHECKING_P */
