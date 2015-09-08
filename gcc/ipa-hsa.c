/* Callgraph based analysis of static variables.
   Copyright (C) 2015 Free Software Foundation, Inc.
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
#include "defaults.h"
#include "hard-reg-set.h"
#include "hash-set.h"
#include "vec.h"
#include "symtab.h"
#include "vec.h"
#include "input.h"
#include "alias.h"
#include "double-int.h"
#include "inchash.h"
#include "tree.h"
#include "tree-pass.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "gimple-expr.h"
#include "dominance.h"
#include "cfg.h"
#include "cfganal.h"
#include "function.h"
#include "predict.h"
#include "basic-block.h"
#include "fold-const.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "machmode.h"
#include "output.h"
#include "function.h"
#include "bitmap.h"
#include "dumpfile.h"
#include "gimple-pretty-print.h"
#include "tree-streamer.h"
#include "diagnostic-core.h"
#include "alloc-pool.h"
#include "tree-ssa-operands.h"
#include "gimple-ssa.h"
#include "tree-phinodes.h"
#include "stringpool.h"
#include "tree-ssanames.h"
#include "rtl.h"
#include "expr.h"
#include "tree-dfa.h"
#include "ssa-iterators.h"
#include "ipa-ref.h"
#include "lto-streamer.h"
#include "cgraph.h"
#include "stor-layout.h"
#include "gimplify-me.h"
#include "print-tree.h"
#include "cfghooks.h"
#include "symbol-summary.h"
#include "hsa.h"

namespace {

static unsigned int
process_hsa_functions (void)
{
  struct cgraph_node *node;

  if (hsa_summaries == NULL)
    hsa_summaries = new hsa_summary_t (symtab);

  FOR_EACH_DEFINED_FUNCTION (node)
    {
      hsa_function_summary *s = hsa_summaries->get (node);

      /* A linked function is skipped.  */
      if (s->binded_function != NULL)
	continue;

      cgraph_node *clone = NULL;
      if (s->kind != HSA_NONE)
	{
	  clone = node->create_virtual_clone (vec <cgraph_edge *> (),
					      NULL, NULL, "hsa");

	  clone->force_output = true;
	  hsa_summaries->link_functions (clone, node, s->kind);

	  if (dump_file)
	    fprintf (dump_file, "HSA creates a new clone: %s, type: %s\n",
		     clone->name (),
		     s->kind == HSA_KERNEL ? "kernel" : "function");
	}
      else if (hsa_callable_function_p (node->decl))
	{
	  clone = node->create_virtual_clone (vec <cgraph_edge *> (),
					      NULL, NULL, "hsa");

	  hsa_summaries->link_functions (clone, node, HSA_FUNCTION);

	  if (dump_file)
	    fprintf (dump_file, "HSA creates a new function clone: %s\n",
		     clone->name ());
	}
    }

  /* Redirect all edges that are between HSA clones.  */
  FOR_EACH_DEFINED_FUNCTION (node)
    {
      cgraph_edge *e = node->callees;

      while (e)
	{
	  hsa_function_summary *src = hsa_summaries->get (node);
	  if (src->kind != HSA_NONE && src->gpu_implementation_p)
	    {
	      hsa_function_summary *dst = hsa_summaries->get (e->callee);
	      if (dst->kind != HSA_NONE && !dst->gpu_implementation_p)
		{
		  e->redirect_callee (dst->binded_function);
		  if (dump_file)
		    fprintf (dump_file,
			     "Redirecting edge to HSA function: %s->%s\n",
			     xstrdup_for_dump (e->caller->name ()),
			     xstrdup_for_dump (e->callee->name ()));
		}
	    }

	  e = e->next_callee;
	}
    }

  return 0;
}

static void
ipa_hsa_write_summary (void)
{
  struct bitpack_d bp;
  struct cgraph_node *node;
  struct output_block *ob;
  unsigned int count = 0;
  lto_symtab_encoder_iterator lsei;
  lto_symtab_encoder_t encoder;

  if (!hsa_summaries)
    return;

  ob = create_output_block (LTO_section_ipa_hsa);
  encoder = ob->decl_state->symtab_node_encoder;
  ob->symbol = NULL;
  for (lsei = lsei_start_function_in_partition (encoder); !lsei_end_p (lsei);
       lsei_next_function_in_partition (&lsei))
    {
      node = lsei_cgraph_node (lsei);
      hsa_function_summary *s = hsa_summaries->get (node);

      if (s->kind != HSA_NONE)
	count++;
    }

  streamer_write_uhwi (ob, count);

  /* Process all of the functions.  */
  for (lsei = lsei_start_function_in_partition (encoder); !lsei_end_p (lsei);
       lsei_next_function_in_partition (&lsei))
    {
      node = lsei_cgraph_node (lsei);
      hsa_function_summary *s = hsa_summaries->get (node);

      if (s->kind != HSA_NONE)
	{
	  encoder = ob->decl_state->symtab_node_encoder;
	  int node_ref = lto_symtab_encoder_encode (encoder, node);
	  streamer_write_uhwi (ob, node_ref);

	  bp = bitpack_create (ob->main_stream);
	  bp_pack_value (&bp, s->kind, 2);
	  bp_pack_value (&bp, s->gpu_implementation_p, 1);
	  bp_pack_value (&bp, s->binded_function != NULL, 1);
	  streamer_write_bitpack (&bp);
	  if (s->binded_function)
	    stream_write_tree (ob, s->binded_function->decl, true);
	}
    }

  streamer_write_char_stream (ob->main_stream, 0);
  produce_asm (ob, NULL);
  destroy_output_block (ob);
}

static void
ipa_hsa_read_section (struct lto_file_decl_data *file_data, const char *data,
		       size_t len)
{
  const struct lto_function_header *header =
    (const struct lto_function_header *) data;
  const int cfg_offset = sizeof (struct lto_function_header);
  const int main_offset = cfg_offset + header->cfg_size;
  const int string_offset = main_offset + header->main_size;
  struct data_in *data_in;
  unsigned int i;
  unsigned int count;

  lto_input_block ib_main ((const char *) data + main_offset,
			   header->main_size, file_data->mode_table);

  data_in =
    lto_data_in_create (file_data, (const char *) data + string_offset,
			header->string_size, vNULL);
  count = streamer_read_uhwi (&ib_main);

  for (i = 0; i < count; i++)
    {
      unsigned int index;
      struct cgraph_node *node;
      lto_symtab_encoder_t encoder;

      index = streamer_read_uhwi (&ib_main);
      encoder = file_data->symtab_node_encoder;
      node = dyn_cast<cgraph_node *> (lto_symtab_encoder_deref (encoder,
								index));
      gcc_assert (node->definition);
      hsa_function_summary *s = hsa_summaries->get (node);

      struct bitpack_d bp = streamer_read_bitpack (&ib_main);
      s->kind = (hsa_function_kind) bp_unpack_value (&bp, 2);
      s->gpu_implementation_p = bp_unpack_value (&bp, 1);
      bool has_tree = bp_unpack_value (&bp, 1);

      if (has_tree)
	{
	  tree decl = stream_read_tree (&ib_main, data_in);
	  s->binded_function = cgraph_node::get_create (decl);
	}
    }
  lto_free_section_data (file_data, LTO_section_ipa_hsa, NULL, data,
			 len);
  lto_data_in_delete (data_in);
}

static void
ipa_hsa_read_summary (void)
{
  struct lto_file_decl_data **file_data_vec = lto_get_file_decl_data ();
  struct lto_file_decl_data *file_data;
  unsigned int j = 0;

  if (hsa_summaries == NULL)
    hsa_summaries = new hsa_summary_t (symtab);

  while ((file_data = file_data_vec[j++]))
    {
      size_t len;
      const char *data = lto_get_section_data (file_data, LTO_section_ipa_hsa,
					       NULL, &len);

      if (data)
	ipa_hsa_read_section (file_data, data, len);
    }
}

const pass_data pass_data_ipa_hsa =
{
  IPA_PASS, /* type */
  "hsa", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_IPA_HSA, /* tv_id */
  0, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_dump_symtab, /* todo_flags_finish */
};

class pass_ipa_hsa : public ipa_opt_pass_d
{
public:
  pass_ipa_hsa (gcc::context *ctxt)
    : ipa_opt_pass_d (pass_data_ipa_hsa, ctxt,
		      NULL, /* generate_summary */
		      ipa_hsa_write_summary, /* write_summary */
		      ipa_hsa_read_summary, /* read_summary */
		      ipa_hsa_write_summary, /* write_optimization_summary */
		      ipa_hsa_read_summary, /* read_optimization_summary */
		      NULL, /* stmt_fixup */
		      0, /* function_transform_todo_flags_start */
		      NULL, /* function_transform */
		      NULL) /* variable_transform */
    {}

  /* opt_pass methods: */
  virtual bool gate (function *);

  virtual unsigned int execute (function *) { return process_hsa_functions (); }

}; // class pass_ipa_reference

bool
pass_ipa_hsa::gate (function *)
{
  return hsa_gen_requested_p () || in_lto_p;
}

} // anon namespace

ipa_opt_pass_d *
make_pass_ipa_hsa (gcc::context *ctxt)
{
  return new pass_ipa_hsa (ctxt);
}
