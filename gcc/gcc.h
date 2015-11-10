/* Header file for modules that link with gcc.c
   Copyright (C) 1999-2015 Free Software Foundation, Inc.

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

#ifndef GCC_GCC_H
#define GCC_GCC_H

#include "version.h"
#include "diagnostic-core.h"

class string_pool
{
public:
  string_pool (): m_items (8) {}

  ~string_pool ()
  {
    for (hash_set <const char *>::iterator it = m_items.begin ();
	 it != m_items.end (); ++it)
      free (const_cast <char *> (*it));
  }

  inline char *allocate (size_t size)
  {
    char *ptr = XNEWVEC (char, size);
    add (ptr);
    return ptr;
  }

  inline void add (const char *ptr)
  {
    if (ptr == NULL)
      return;

    gcc_assert (!m_items.contains (ptr));

    m_items.add (ptr);
  }

  char *dup (const char *s)
  {
    size_t l = strlen (s);
    char *ptr = allocate (l + 1);
    memcpy (ptr, s, l + 1);
    return ptr;
  }

  char *cat (const char *str0, const char *str1, const char *str2 = NULL,
	     const char *str3 = NULL, const char *str4 = NULL,
	     const char *str5 = NULL)
  {
    char *ptr = concat (str0, str1, str2, str3, str4, str5, NULL);
    add (ptr);

    return ptr;
  }

private:
  hash_set <const char *> m_items;
};

/* The top-level "main" within the driver would be ~1000 lines long.
   This class breaks it up into smaller functions and contains some
   state shared by them.  */

class driver
{
 public:
  driver (bool can_finalize, bool debug);
  ~driver ();
  int main (int argc, char **argv);
  void release ();
  void finalize ();

 private:
  void set_progname (const char *argv0) const;
  void expand_at_files (int *argc, char ***argv) const;
  void decode_argv (int argc, const char **argv);
  void global_initializations ();
  void build_multilib_strings () const;
  void set_up_specs () const;
  void putenv_COLLECT_GCC (const char *argv0) const;
  void maybe_putenv_COLLECT_LTO_WRAPPER () const;
  void maybe_putenv_OFFLOAD_TARGETS () const;
  void handle_unrecognized_options () const;
  int maybe_print_and_exit () const;
  bool prepare_infiles ();
  void do_spec_on_infiles () const;
  void maybe_run_linker (const char *argv0) const;
  void final_actions () const;
  int get_exit_code () const;

 private:
  char *explicit_link_files;
  struct cl_decoded_option *decoded_options;
  unsigned int decoded_options_count;
};

/* The mapping of a spec function name to the C function that
   implements it.  */
struct spec_function
{
  const char *name;
  const char *(*func) (int, const char **);
};

/* These are exported by gcc.c.  */
extern int do_spec (const char *);
extern void record_temp_file (const char *, int, int);
extern void pfatal_with_name (const char *) ATTRIBUTE_NORETURN;
extern void set_input (const char *);

/* Spec files linked with gcc.c must provide definitions for these.  */

/* Called before processing to change/add/remove arguments.  */
extern void lang_specific_driver (struct cl_decoded_option **,
				  unsigned int *, int *);

/* Called before linking.  Returns 0 on success and -1 on failure.  */
extern int lang_specific_pre_link (void);

extern int n_infiles;

/* Number of extra output files that lang_specific_pre_link may generate.  */
extern int lang_specific_extra_outfiles;

/* A vector of corresponding output files is made up later.  */

extern const char **outfiles;

extern void
driver_get_configure_time_options (void (*cb)(const char *option,
					      void *user_data),
				   void *user_data);

#endif /* ! GCC_GCC_H */
