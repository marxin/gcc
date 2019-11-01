/* If-elseif-else to switch conversion pass
   Copyright (C) 2019 Free Software Foundation, Inc.

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "tree-pass.h"
#include "ssa.h"
#include "gimple-pretty-print.h"
#include "fold-const.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "tree-dfa.h"
#include "domwalk.h"
#include "tree-cfgcleanup.h"
#include "params.h"
#include "alias.h"
#include "tree-ssa-loop.h"
#include "diagnostic.h"
#include "cfghooks.h"
#include "tree-into-ssa.h"
#include "cfganal.h"

struct if_chain_entry
{
  gcond *cond;
  basic_block bb;
  edge true_edge;
  edge false_edge;
};

class if_dom_walker : public dom_walker
{
public:
  if_dom_walker (cdi_direction direction)
    : dom_walker (direction), all_candidates (), m_visited_bbs ()
  {}

  virtual edge before_dom_children (basic_block);

  auto_vec<vec<if_chain_entry> > all_candidates;

private:
  auto_bitmap m_visited_bbs;
};

static tree
build_case_label (tree value, basic_block dest)
{
  tree label = gimple_block_label (dest);
  return build_case_label (value, NULL, label);
}

static int
label_cmp (const void *a, const void *b)
{
  const_tree l1 = *(const const_tree *) a;
  const_tree l2 = *(const const_tree *) b;

  return tree_int_cst_compare (CASE_LOW (l1), CASE_LOW (l2));
}

static void
record_phi_arguments (hash_map<basic_block, vec<tree> > *phi_map, edge e)
{
  if (phi_map->get (e->dest) == NULL)
    {
      vec<tree> phi_arguments;
      phi_arguments.create (4);
      for (gphi_iterator gsi = gsi_start_phis (e->dest); !gsi_end_p (gsi);
	   gsi_next (&gsi))
	{
	  gphi *phi = gsi.phi ();
	  if (!virtual_operand_p (gimple_phi_result (phi)))
	    phi_arguments.safe_push (PHI_ARG_DEF_FROM_EDGE (phi, e));
	}

      phi_map->put (e->dest, phi_arguments);
    }
}

struct int_cst_hash : typed_noop_remove<tree>
{
  typedef tree value_type;
  typedef tree compare_type;

  static hashval_t hash (const tree &ref)
  {
    inchash::hash hstate (0);
    inchash::add_expr (ref, hstate);
    return hstate.end ();
  }

  static bool equal (const tree &ref1, const tree &ref2)
  {
    return tree_int_cst_equal (ref1, ref2);
  }

  static void mark_deleted (tree &ref) { ref = reinterpret_cast<tree> (1); }
  static void mark_empty (tree &ref) { ref = NULL; }

  static bool is_deleted (const tree &ref)
  {
    return ref == reinterpret_cast<tree> (1);
  }

  static bool is_empty (const tree &ref) { return ref == NULL; }
};

static void
convert_if_conditions_to_switch (vec<if_chain_entry> &conditions)
{
  auto_vec<tree> labels;
  if_chain_entry first_cond = conditions[0];

  edge default_edge = conditions[conditions.length () - 1].false_edge;
  basic_block default_bb = default_edge->dest;

  /* Recond all PHI nodes that will later be fixed.  */
  hash_map<basic_block, vec<tree> > phi_map;
  for (unsigned i = 0; i < conditions.length (); i++)
    record_phi_arguments (&phi_map, conditions[i].true_edge);
  record_phi_arguments (&phi_map,
			conditions[conditions.length () - 1].false_edge);

  for (unsigned i = 0; i < conditions.length (); i++)
    {
      if_chain_entry entry = conditions[i];

      basic_block case_bb = entry.true_edge->dest;
      labels.safe_push (
	build_case_label (gimple_cond_rhs (entry.cond), case_bb));
      default_bb = entry.false_edge->dest;

      if (i == 0)
	{
	  remove_edge (first_cond.true_edge);
	  remove_edge (first_cond.false_edge);
	}
      else
	delete_basic_block (entry.bb);

      make_edge (first_cond.bb, case_bb, 0);
    }

  labels.qsort (label_cmp);

  edge e = find_edge (first_cond.bb, default_bb);
  if (e == NULL)
    e = make_edge (first_cond.bb, default_bb, 0);
  gswitch *s
    = gimple_build_switch (gimple_cond_lhs (first_cond.cond),
			   build_case_label (NULL_TREE, default_bb), labels);

  gimple_stmt_iterator gsi = gsi_for_stmt (first_cond.cond);
  gsi_remove (&gsi, true);
  gsi_insert_before (&gsi, s, GSI_NEW_STMT);

  /* Fill up missing PHI node arguments.  */
  for (hash_map<basic_block, vec<tree> >::iterator it = phi_map.begin ();
       it != phi_map.end (); ++it)
    {
      edge e = find_edge (first_cond.bb, (*it).first);
      unsigned i = 0;
      for (gphi_iterator gsi = gsi_start_phis ((*it).first); !gsi_end_p (gsi);
	   gsi_next (&gsi))
	{
	  gphi *phi = gsi.phi ();
	  if (!virtual_operand_p (gimple_phi_result (phi)))
	    add_phi_arg (phi, (*it).second[i++], e, UNKNOWN_LOCATION);
	}
    }
}

edge
if_dom_walker::before_dom_children (basic_block bb)
{
  tree seen_lhs = NULL_TREE;
  vec<if_chain_entry> conditions;
  conditions.create (8);

  hash_set<int_cst_hash> seen_constants;

  while (true)
    {
      if (bitmap_bit_p (m_visited_bbs, bb->index))
	break;
      bitmap_set_bit (m_visited_bbs, bb->index);

      gimple_stmt_iterator gsi = gsi_last_nondebug_bb (bb);
      if (gsi_end_p (gsi))
	break;

      if (!conditions.is_empty () && EDGE_COUNT (bb->preds) != 1)
	break;

      gcond *cond = dyn_cast<gcond *> (gsi_stmt (gsi));
      if (cond == NULL)
	break;

      if (gimple_cond_code (cond) != EQ_EXPR)
	break;

      tree lhs = gimple_cond_lhs (cond);
      if (TREE_CODE (lhs) != SSA_NAME || !INTEGRAL_TYPE_P (TREE_TYPE (lhs)))
	break;

      tree rhs = gimple_cond_rhs (cond);
      if (TREE_CODE (rhs) != INTEGER_CST)
	break;

      if (conditions.is_empty ())
	seen_lhs = lhs;
      else if (seen_lhs != lhs)
	break;

      /* If it's not the first condition, then we need a BB without
	 any statements.  */
      if (!conditions.is_empty ())
	{
	  gsi_prev_nondebug (&gsi);
	  if (!gsi_end_p (gsi))
	    break;
	}

      /* Follow if-elseif-elseif chain.  */
      edge true_edge, false_edge;
      extract_true_false_edges_from_block (bb, &true_edge, &false_edge);

      if (seen_constants.contains (rhs))
	return NULL;
      else
	seen_constants.add (rhs);

      if_chain_entry entry = {cond, bb, true_edge, false_edge};
      conditions.safe_push (entry);

      bb = false_edge->dest;
    }

  if (conditions.length () >= 3)
    {
      if (dump_file)
	{
	  expanded_location loc
	    = expand_location (gimple_location (conditions[0].cond));
	  fprintf (dump_file, "Condition chain (at %s:%d) with %d conditions "
		   "transformed into a switch statement.\n",
		   loc.file, loc.line, conditions.length ());
	}

      all_candidates.safe_push (conditions);
    }

  return NULL;
}

namespace {

const pass_data pass_data_if_to_switch =
{
  GIMPLE_PASS, /* type */
  "iftoswitch", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_TREE_IF_TO_SWITCH, /* tv_id */
  ( PROP_cfg | PROP_ssa ), /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_cleanup_cfg | TODO_update_ssa /* todo_flags_finish */
};

class pass_if_to_switch : public gimple_opt_pass
{
public:
  pass_if_to_switch (gcc::context *ctxt)
    : gimple_opt_pass (pass_data_if_to_switch, ctxt)
  {}

  /* opt_pass methods: */
  virtual bool gate (function *) { return flag_tree_if_to_switch != 0; }
  virtual unsigned int execute (function *);

}; // class pass_if_to_switch

unsigned int
pass_if_to_switch::execute (function *fun)
{
  /* We might consider making this a property of each pass so that it
     can be [re]computed on an as-needed basis.  Particularly since
     this pass could be seen as an extension of DCE which needs post
     dominators.  */
  calculate_dominance_info (CDI_DOMINATORS);

  /* Dead store elimination is fundamentally a walk of the post-dominator
     tree and a backwards walk of statements within each block.  */
  if_dom_walker walker (CDI_DOMINATORS);
  walker.walk (fun->cfg->x_entry_block_ptr);

  for (unsigned i = 0; i < walker.all_candidates.length (); i++)
    convert_if_conditions_to_switch (walker.all_candidates[i]);

  /* For now, just wipe the dominator information.  */
  free_dominance_info (CDI_DOMINATORS);

  mark_virtual_operands_for_renaming (cfun);

  return 0;
}

} // anon namespace

gimple_opt_pass *
make_pass_if_to_switch (gcc::context *ctxt)
{
  return new pass_if_to_switch (ctxt);
}
