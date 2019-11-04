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

struct case_range
{
  case_range ():
    m_min (NULL_TREE), m_max (NULL_TREE)
  {}

  case_range (tree min, tree max = NULL_TREE)
    : m_min (min), m_max (max)
  {
    if (max == NULL_TREE)
      m_max = min;
  }

  tree m_min;
  tree m_max;
};

struct if_chain_entry
{
  if_chain_entry (basic_block bb, edge true_edge, edge false_edge)
    : m_case_values (), m_bb (bb),
      m_true_edge (true_edge), m_false_edge (false_edge)
  {
    m_case_values.create (2);
  }

  void add_case_value (case_range range)
  {
    m_case_values.safe_push (range);
  }

  vec<case_range> m_case_values;
  basic_block m_bb;
  edge m_true_edge;
  edge m_false_edge;
};

struct if_chain
{
  /* Default constructor.  */
  if_chain():
    m_first_condition (NULL), m_index (NULL_TREE), m_entries ()
  {
    m_entries.create (2);
  }

  bool set_and_check_index (tree index);

  bool check_non_overlapping_cases ();

  gcond *m_first_condition;
  tree m_index;
  vec<if_chain_entry> m_entries; 
};

bool
if_chain::set_and_check_index (tree index)
{
  if (TREE_CODE (index) != SSA_NAME || !INTEGRAL_TYPE_P (TREE_TYPE (index)))
    return false;

  if (m_index == NULL)
    m_index = index;

  return index == m_index;
}

static int
range_cmp (const void *a, const void *b)
{
  const case_range *cr1 = *(const case_range * const *) a;
  const case_range *cr2 = *(const case_range * const *) b;

  return tree_int_cst_compare (cr1->m_min, cr2->m_min);
}

bool
if_chain::check_non_overlapping_cases ()
{
  auto_vec<case_range *> all_ranges;
  for (unsigned i = 0; i < m_entries.length (); i++)
    for (unsigned j =0; j < m_entries[i].m_case_values.length (); j++)
      all_ranges.safe_push (&m_entries[i].m_case_values[j]);

  all_ranges.qsort (range_cmp);

  for (unsigned i = 0; i < all_ranges.length () - 2; i++)
    {
      case_range *left = all_ranges[i];
      case_range *right = all_ranges[i + 1];
      if (tree_int_cst_le (left->m_min, right->m_min)
	  && tree_int_cst_le (right->m_min, left->m_max))
	return false;
    }

  return true;
}

class if_dom_walker : public dom_walker
{
public:
  if_dom_walker (cdi_direction direction)
    : dom_walker (direction), all_candidates (), m_visited_bbs ()
  {}

  virtual edge before_dom_children (basic_block);

  auto_vec<if_chain> all_candidates;

private:
  auto_bitmap m_visited_bbs;
};

static tree
build_case_label (tree min, tree max, basic_block dest)
{
  tree label = gimple_block_label (dest);
  return build_case_label (min, min == max ? NULL_TREE : max, label);
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

static void
convert_if_conditions_to_switch (if_chain &chain)
{
  auto_vec<tree> labels;
  if_chain_entry first_cond = chain.m_entries[0];

  edge default_edge = chain.m_entries[chain.m_entries.length () - 1].m_false_edge;
  basic_block default_bb = default_edge->dest;

  /* Recond all PHI nodes that will later be fixed.  */
  hash_map<basic_block, vec<tree> > phi_map;
  for (unsigned i = 0; i < chain.m_entries.length (); i++)
    record_phi_arguments (&phi_map, chain.m_entries[i].m_true_edge);
  record_phi_arguments (&phi_map,
			chain.m_entries[chain.m_entries.length () - 1].m_false_edge);

  for (unsigned i = 0; i < chain.m_entries.length (); i++)
    {
      if_chain_entry entry = chain.m_entries[i];

      basic_block case_bb = entry.m_true_edge->dest;

      for (unsigned j = 0; j < entry.m_case_values.length (); j++)
	labels.safe_push (build_case_label (entry.m_case_values[j].m_min,
					    entry.m_case_values[j].m_max,
					    case_bb));
      default_bb = entry.m_false_edge->dest;

      if (i == 0)
	{
	  remove_edge (first_cond.m_true_edge);
	  remove_edge (first_cond.m_false_edge);
	}
      else
	delete_basic_block (entry.m_bb);

      make_edge (first_cond.m_bb, case_bb, 0);
    }

  labels.qsort (label_cmp);

  edge e = find_edge (first_cond.m_bb, default_bb);
  if (e == NULL)
    e = make_edge (first_cond.m_bb, default_bb, 0);
  gswitch *s
    = gimple_build_switch (chain.m_index,
			   build_case_label (NULL_TREE, NULL_TREE, default_bb),
			   labels);

  gimple_stmt_iterator gsi = gsi_for_stmt (chain.m_first_condition);
  gsi_remove (&gsi, true);
  gsi_insert_before (&gsi, s, GSI_NEW_STMT);

  /* Fill up missing PHI node arguments.  */
  for (hash_map<basic_block, vec<tree> >::iterator it = phi_map.begin ();
       it != phi_map.end (); ++it)
    {
      edge e = find_edge (first_cond.m_bb, (*it).first);
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

bool
extract_case_from_assignment (gassign *assign, tree *lhs, case_range *range,
			      unsigned *visited_stmt_count)
{
  tree_code code = gimple_assign_rhs_code (assign);
  if (code == EQ_EXPR)
    {
      /* Handle situation 2a:
	 _1 = aChar_8(D) == 1;  */
      *lhs = gimple_assign_rhs1 (assign);
      range->m_min = gimple_assign_rhs2 (assign);
      range->m_max = range->m_min;

      if (TREE_CODE (gimple_assign_rhs2 (assign)) != INTEGER_CST)
	return false;

      *visited_stmt_count += 1;
      return true;
    }
  else if (code == LE_EXPR)
    {
      /* Handle situation 2b:
	 aChar.1_1 = (unsigned int) aChar_10(D);
	 _2 = aChar.1_1 + 4294967287;
	 _3 = _2 <= 1;  */
      tree ssa = gimple_assign_rhs1 (assign);
      tree range_size = gimple_assign_rhs2 (assign);
      if (TREE_CODE (ssa) != SSA_NAME
	  || TREE_CODE (range_size) != INTEGER_CST)
	return false;

      gassign *subtraction = dyn_cast<gassign *> (SSA_NAME_DEF_STMT (ssa));
      if (subtraction == NULL
	  || gimple_assign_rhs_code (subtraction) != PLUS_EXPR)
	return false;

      tree casted = gimple_assign_rhs1 (subtraction);
      tree min = gimple_assign_rhs2 (subtraction);
      if (TREE_CODE (casted) != SSA_NAME
	  || TREE_CODE (min) != INTEGER_CST)
	return false;

      // TODO: with unsigned type the cast will not be needed
      gassign *to_unsigned = dyn_cast<gassign *> (SSA_NAME_DEF_STMT (casted));
      if (to_unsigned == NULL
	  || !gimple_assign_unary_nop_p (to_unsigned)
	  || !TYPE_UNSIGNED (TREE_TYPE (casted)))
	return false;

      *lhs = gimple_assign_rhs1 (to_unsigned);
      tree type = TREE_TYPE (*lhs);
      tree range_min = fold_convert (type, const_unop (NEGATE_EXPR, type, min));

      range->m_min = range_min;
      range->m_max = const_binop (PLUS_EXPR, TREE_TYPE (*lhs),
				  range_min, fold_convert (type, range_size));
      *visited_stmt_count += 3;
      return true;
    }
  else
    return false;
}

edge
if_dom_walker::before_dom_children (basic_block bb)
{
  if_chain chain;
  unsigned case_values = 0;

  while (true)
    {
      bool first = chain.m_entries.is_empty ();
      if (bitmap_bit_p (m_visited_bbs, bb->index))
	break;
      bitmap_set_bit (m_visited_bbs, bb->index);

      gimple_stmt_iterator gsi = gsi_last_nondebug_bb (bb);
      if (gsi_end_p (gsi))
	break;

      if (!chain.m_entries.is_empty () && EDGE_COUNT (bb->preds) != 1)
	break;

      gcond *cond = dyn_cast<gcond *> (gsi_stmt (gsi));
      if (cond == NULL)
	break;

      if (first)
	chain.m_first_condition = cond;

      edge true_edge, false_edge;
      extract_true_false_edges_from_block (bb, &true_edge, &false_edge);

      if_chain_entry entry (bb, true_edge, false_edge);

      /* Current we support following patterns (situations):

	 1) if condition with equal operation:

	    <bb 2> :
	    if (argc_8(D) == 1)
	      goto <bb 3>; [INV]
	    else
	      goto <bb 4>; [INV]

	 2a) if condition with two equal operations:

	    <bb 2> :
	    _1 = aChar_8(D) == 1;
	    _2 = aChar_8(D) == 10;
	    _3 = _1 | _2;
	    if (_3 != 0)
	      goto <bb 5>; [INV]
	    else
	      goto <bb 3>; [INV]

	2b) if condition with one or two range checks

	    <bb 2> :
	    aChar.1_1 = (unsigned int) aChar_10(D);
	    _2 = aChar.1_1 + 4294967287;
	    _3 = _2 <= 1;
	    _4 = aChar_10(D) == 12;
	    _5 = _3 | _4;
	    if (_5 != 0)
	      goto <bb 5>; [INV]
	    else
	      goto <bb 3>; [INV]
		*/

      tree lhs = gimple_cond_lhs (cond);
      tree rhs = gimple_cond_rhs (cond);
      tree_code code = gimple_cond_code (cond);
      unsigned visited_stmt_count = 0;

      /* Situation 1.  */
      if (code == EQ_EXPR)
	{
	  if (!chain.set_and_check_index (lhs))
	    break;
	  if (TREE_CODE (TREE_TYPE (rhs)) != INTEGER_CST)
	    break;
	  entry.add_case_value (case_range (rhs));
	  visited_stmt_count = 1;
	  ++case_values;
	}
      /* Situation 2a and 2b.  */
      else if (code == NE_EXPR
	       && integer_zerop (rhs)
	       && TREE_CODE (lhs) == SSA_NAME
	       && TREE_CODE (TREE_TYPE (lhs)) == BOOLEAN_TYPE)
	{
	  gassign *def = dyn_cast<gassign *> (SSA_NAME_DEF_STMT (lhs));
	  if (def == NULL
	      || gimple_assign_rhs_code (def) != BIT_IOR_EXPR
	      || gimple_bb (def) != bb)
	    break;

	  tree rhs1 = gimple_assign_rhs1 (def);
	  tree rhs2 = gimple_assign_rhs2 (def);
	  if (TREE_CODE (rhs1) != SSA_NAME || TREE_CODE (rhs2) != SSA_NAME)
	    break;

	  gassign *def1 = dyn_cast<gassign *> (SSA_NAME_DEF_STMT (rhs1));
	  gassign *def2 = dyn_cast<gassign *> (SSA_NAME_DEF_STMT (rhs2));
	  if (def1 == NULL
	      || def2 == NULL
	      || def1 == def2
	      || gimple_bb (def1) != bb
	      || gimple_bb (def2) != bb)
	    break;

	  case_range range1;
	  if (!extract_case_from_assignment (def1, &lhs, &range1,
					     &visited_stmt_count))
	    break;
	  rhs = gimple_assign_rhs2 (def1);
	  if (!chain.set_and_check_index (lhs))
	    break;
	  entry.add_case_value (range1);

	  case_range range2;
	  if (!extract_case_from_assignment (def2, &lhs, &range2,
					     &visited_stmt_count))
	    break;
	  rhs = gimple_assign_rhs2 (def2);
	  if (!chain.set_and_check_index (lhs))
	    break;
	  entry.add_case_value (range2);
	  case_values += 2;
	  visited_stmt_count += 2;
	}
      else
	break;

      /* If it's not the first condition, then we need a BB without
	 any statements.  */
      if (!first)
	{
	  unsigned stmt_count = 0;
	  for (gimple_stmt_iterator gsi = gsi_start_nondebug_bb (bb);
	       !gsi_end_p (gsi); gsi_next_nondebug (&gsi))
	    ++stmt_count;

	  if (stmt_count - visited_stmt_count != 0)
	    break;
	}

      chain.m_entries.safe_push (entry);

      /* Follow if-elseif-elseif chain.  */
      bb = false_edge->dest;
    }

  if (case_values >= 3
      && chain.check_non_overlapping_cases ())
    {
      if (dump_file)
	{
	  expanded_location loc
	    = expand_location (gimple_location (chain.m_first_condition));
	  fprintf (dump_file, "Condition chain (at %s:%d) with %d conditions "
		   "(%d BBs) transformed into a switch statement.\n",
		   loc.file, loc.line, case_values, chain.m_entries.length ());
	}

      all_candidates.safe_push (chain);
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
