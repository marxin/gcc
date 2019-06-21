/* Callgraph based analysis of static variables.
   Copyright (C) 2015-2019 Free Software Foundation, Inc.
   Contributed by Martin Liska <mliska@suse.cz>

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

/* Interprocedural HSA pass is responsible for creation of HSA clones.
   For all these HSA clones, we emit HSAIL instructions and pass processing
   is terminated.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "is-a.h"
#include "hash-set.h"
#include "vec.h"
#include "tree.h"
#include "tree-pass.h"
#include "function.h"
#include "basic-block.h"
#include "gimple.h"
#include "dumpfile.h"
#include "gimple-pretty-print.h"
#include "tree-streamer.h"
#include "stringpool.h"
#include "cgraph.h"
#include "print-tree.h"
#include "symbol-summary.h"
#include "hsa-common.h"

namespace {

static void
ipa_verification_write_summary (void)
{
  struct output_block *ob;

  ob = create_output_block (LTO_section_ipa_verification);
  ob->symbol = NULL;
  streamer_write_uhwi (ob, canonical_verification_hash.elements ());

  for (hash_map<tree, tree>::iterator it = canonical_verification_hash.begin ();
       it != canonical_verification_hash.end (); ++it)
    {
      stream_write_tree (ob, (*it).first, false);
      stream_write_tree (ob, (*it).second, false);
      gcc_assert ((*it).second != NULL_TREE);
    }

  streamer_write_char_stream (ob->main_stream, 0);
  produce_asm (ob, NULL);
  destroy_output_block (ob);
}

static hash_map<tree, tree> wpa_canonical_map;

/* Read section in file FILE_DATA of length LEN with data DATA.  */

static void
ipa_verification_read_section (struct lto_file_decl_data *file_data, const char *data,
			       size_t len)
{
  const struct lto_function_header *header
    = (const struct lto_function_header *) data;
  const int cfg_offset = sizeof (struct lto_function_header);
  const int main_offset = cfg_offset + header->cfg_size;
  const int string_offset = main_offset + header->main_size;
  struct data_in *data_in;
  unsigned int i;
  unsigned int count;

  lto_input_block ib_main ((const char *) data + main_offset,
			   header->main_size, file_data->mode_table);

  data_in
    = lto_data_in_create (file_data, (const char *) data + string_offset,
			  header->string_size, vNULL);
  count = streamer_read_uhwi (&ib_main);

  for (i = 0; i < count; i++)
    {
      tree main_variant = stream_read_tree (&ib_main, data_in);
      tree canonical_type = stream_read_tree (&ib_main, data_in);

      tree *value = wpa_canonical_map.get (main_variant);
      if (value != NULL)
	gcc_assert (*value == canonical_type);
      else
	wpa_canonical_map.put (main_variant, canonical_type);
    }
  lto_free_section_data (file_data, LTO_section_ipa_verification, NULL, data,
			 len);
  lto_data_in_delete (data_in);
}

/* Load streamed IPA verification summary.  */

static void
ipa_verification_read_summary (void)
{
  struct lto_file_decl_data **file_data_vec = lto_get_file_decl_data ();
  struct lto_file_decl_data *file_data;
  unsigned int j = 0;

  if (hsa_summaries == NULL)
    hsa_summaries = new hsa_summary_t (symtab);

  while ((file_data = file_data_vec[j++]))
    {
      fprintf (stderr, "reading: %s\n", file_data->file_name);
      size_t len;
      const char *data = lto_get_section_data (file_data, LTO_section_ipa_verification,
					       NULL, &len);

      if (data)
	ipa_verification_read_section (file_data, data, len);
    }
}

static unsigned int
check_types (void)
{
  for (hash_map<tree, tree>::iterator it = wpa_canonical_map.begin ();
       it != wpa_canonical_map.end (); ++it)
    {
      gcc_assert ((*it).second != NULL_TREE);
      tree main_variant = (*it).first;
      gcc_assert (TYPE_MAIN_VARIANT (main_variant) == main_variant);

      tree canonical_type = (*it).second;
      //gcc_assert (TYPE_CANONICAL (main_variant) != NULL_TREE);
      if (TYPE_CANONICAL (main_variant) != NULL_TREE)
	gcc_assert (TYPE_CANONICAL (main_variant) == canonical_type);
    }

  return 0;
}

const pass_data pass_data_ipa_verification =
{
  IPA_PASS, /* type */
  "verification", /* name */
  OPTGROUP_OMP, /* optinfo_flags */
  TV_IPA_HSA, /* tv_id */
  0, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_dump_symtab, /* todo_flags_finish */
};

class pass_ipa_verification : public ipa_opt_pass_d
{
public:
  pass_ipa_verification (gcc::context *ctxt)
    : ipa_opt_pass_d (pass_data_ipa_verification, ctxt,
		      NULL, /* generate_summary */
		      ipa_verification_write_summary, /* write_summary */
		      ipa_verification_read_summary, /* read_summary */
		      NULL, /* write_optimization_summary */
		      NULL, /* read_optimization_summary */
		      NULL, /* stmt_fixup */
		      0, /* function_transform_todo_flags_start */
		      NULL, /* function_transform */
		      NULL) /* variable_transform */
    {}

  /* opt_pass methods: */
  virtual bool gate (function *);

  virtual unsigned int execute (function *) { return check_types (); }

}; // class pass_ipa_reference

bool
pass_ipa_verification::gate (function *)
{
  return true;
}

} // anon namespace

ipa_opt_pass_d *
make_pass_ipa_verification (gcc::context *ctxt)
{
  return new pass_ipa_verification (ctxt);
}
