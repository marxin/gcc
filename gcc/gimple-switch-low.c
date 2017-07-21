/* Lower GIMPLE_SWITCH expressions to something more efficient than
   a jump table.
   Copyright (C) 2006-2017 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

GCC is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not, write to the Free
Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.  */

/* This file handles the lowering of GIMPLE_SWITCH to an indexed
   load, or a series of bit-test-and-branch expressions.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "insn-codes.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "tree-pass.h"
#include "ssa.h"
#include "optabs-tree.h"
#include "cgraph.h"
#include "gimple-pretty-print.h"
#include "params.h"
#include "fold-const.h"
#include "varasm.h"
#include "stor-layout.h"
#include "cfganal.h"
#include "gimplify.h"
#include "gimple-iterator.h"
#include "gimplify-me.h"
#include "tree-cfg.h"
#include "cfgloop.h"
#include "target.h"
#include "alloc-pool.h"
#include "tree-into-ssa.h"

struct case_node
{
  case_node		*left;	/* Left son in binary tree.  */
  case_node		*right;	/* Right son in binary tree;
				   also node chain.  */
  case_node		*parent; /* Parent of node in binary tree.  */
  tree			low;	/* Lowest index value for this label.  */
  tree			high;	/* Highest index value for this label.  */
  basic_block		case_bb; /* Label to jump to when node matches.  */
  tree			case_label; /* Label to jump to when node matches.  */
  profile_probability   prob; /* Probability of taking this case.  */
  profile_probability   subtree_prob;  /* Probability of reaching subtree
					  rooted at this node.  */
};

typedef case_node *case_node_ptr;

static basic_block emit_case_nodes (basic_block, tree, case_node_ptr,
				    basic_block, tree, profile_probability,
				    tree);
static bool node_has_low_bound (case_node_ptr, tree);
static bool node_has_high_bound (case_node_ptr, tree);
static bool node_is_bounded (case_node_ptr, tree);

/* Return the smallest number of different values for which it is best to use a
   jump-table instead of a tree of conditional branches.  */

static unsigned int
case_values_threshold (void)
{
  unsigned int threshold = PARAM_VALUE (PARAM_CASE_VALUES_THRESHOLD);

  if (threshold == 0)
    threshold = targetm.case_values_threshold ();

  return threshold;
}

/* Reset the aux field of all outgoing edges of basic block BB.  */

static inline void
reset_out_edges_aux (basic_block bb)
{
  edge e;
  edge_iterator ei;
  FOR_EACH_EDGE (e, ei, bb->succs)
    e->aux = (void *) 0;
}

/* Compute the number of case labels that correspond to each outgoing edge of
   STMT.  Record this information in the aux field of the edge.  */

static inline void
compute_cases_per_edge (gswitch *stmt)
{
  basic_block bb = gimple_bb (stmt);
  reset_out_edges_aux (bb);
  int ncases = gimple_switch_num_labels (stmt);
  for (int i = ncases - 1; i >= 1; --i)
    {
      tree elt = gimple_switch_label (stmt, i);
      tree lab = CASE_LABEL (elt);
      basic_block case_bb = label_to_block_fn (cfun, lab);
      edge case_edge = find_edge (bb, case_bb);
      case_edge->aux = (void *) ((intptr_t) (case_edge->aux) + 1);
    }
}

/* Do the insertion of a case label into case_list.  The labels are
   fed to us in descending order from the sorted vector of case labels used
   in the tree part of the middle end.  So the list we construct is
   sorted in ascending order.

   LABEL is the case label to be inserted.  LOW and HIGH are the bounds
   against which the index is compared to jump to LABEL and PROB is the
   estimated probability LABEL is reached from the switch statement.  */

static case_node *
add_case_node (case_node *head, tree low, tree high, basic_block case_bb,
	       tree case_label, profile_probability prob,
	       object_allocator<case_node> &case_node_pool)
{
  case_node *r;

  gcc_checking_assert (low);
  gcc_checking_assert (high && (TREE_TYPE (low) == TREE_TYPE (high)));

  /* Add this label to the chain.  */
  r = case_node_pool.allocate ();
  r->low = low;
  r->high = high;
  r->case_bb = case_bb;
  r->case_label = case_label;
  r->parent = r->left = NULL;
  r->prob = prob;
  r->subtree_prob = prob;
  r->right = head;
  return r;
}

/* Dump ROOT, a list or tree of case nodes, to file.  */

static void
dump_case_nodes (FILE *f, case_node *root, int indent_step, int indent_level)
{
  if (root == 0)
    return;
  indent_level++;

  dump_case_nodes (f, root->left, indent_step, indent_level);

  fputs (";; ", f);
  fprintf (f, "%*s", indent_step * indent_level, "");
  print_dec (root->low, f, TYPE_SIGN (TREE_TYPE (root->low)));
  if (!tree_int_cst_equal (root->low, root->high))
    {
      fprintf (f, " ... ");
      print_dec (root->high, f, TYPE_SIGN (TREE_TYPE (root->high)));
    }
  fputs ("\n", f);

  dump_case_nodes (f, root->right, indent_step, indent_level);
}

/* Take an ordered list of case nodes
   and transform them into a near optimal binary tree,
   on the assumption that any target code selection value is as
   likely as any other.

   The transformation is performed by splitting the ordered
   list into two equal sections plus a pivot.  The parts are
   then attached to the pivot as left and right branches.  Each
   branch is then transformed recursively.  */

static void
balance_case_nodes (case_node_ptr *head, case_node_ptr parent)
{
  case_node_ptr np;

  np = *head;
  if (np)
    {
      int i = 0;
      int ranges = 0;
      case_node_ptr *npp;
      case_node_ptr left;

      /* Count the number of entries on branch.  Also count the ranges.  */

      while (np)
	{
	  if (!tree_int_cst_equal (np->low, np->high))
	    ranges++;

	  i++;
	  np = np->right;
	}

      if (i > 2)
	{
	  /* Split this list if it is long enough for that to help.  */
	  npp = head;
	  left = *npp;

	  /* If there are just three nodes, split at the middle one.  */
	  if (i == 3)
	    npp = &(*npp)->right;
	  else
	    {
	      /* Find the place in the list that bisects the list's total cost,
		 where ranges count as 2.
		 Here I gets half the total cost.  */
	      i = (i + ranges + 1) / 2;
	      while (1)
		{
		  /* Skip nodes while their cost does not reach that amount.  */
		  if (!tree_int_cst_equal ((*npp)->low, (*npp)->high))
		    i--;
		  i--;
		  if (i <= 0)
		    break;
		  npp = &(*npp)->right;
		}
	    }
	  *head = np = *npp;
	  *npp = 0;
	  np->parent = parent;
	  np->left = left;

	  /* Optimize each of the two split parts.  */
	  balance_case_nodes (&np->left, np);
	  balance_case_nodes (&np->right, np);
	  np->subtree_prob = np->prob;
	  np->subtree_prob += np->left->subtree_prob;
	  np->subtree_prob += np->right->subtree_prob;
	}
      else
	{
	  /* Else leave this branch as one level,
	     but fill in `parent' fields.  */
	  np = *head;
	  np->parent = parent;
	  np->subtree_prob = np->prob;
	  for (; np->right; np = np->right)
	    {
	      np->right->parent = np;
	      (*head)->subtree_prob += np->right->subtree_prob;
	    }
	}
    }
}

/* Return true if a switch should be expanded as a decision tree.
   RANGE is the difference between highest and lowest case.
   UNIQ is number of unique case node targets, not counting the default case.
   COUNT is the number of comparisons needed, not counting the default case.  */

static bool
expand_switch_as_decision_tree_p (tree range,
				  unsigned int uniq ATTRIBUTE_UNUSED,
				  unsigned int count)
{
  int max_ratio;

  /* If neither casesi or tablejump is available, or flag_jump_tables
     over-ruled us, we really have no choice.  */
  if (!targetm.have_casesi () && !targetm.have_tablejump ())
    return true;
  if (!flag_jump_tables)
    return true;
#ifndef ASM_OUTPUT_ADDR_DIFF_ELT
  if (flag_pic)
    return true;
#endif

  /* If the switch is relatively small such that the cost of one
     indirect jump on the target are higher than the cost of a
     decision tree, go with the decision tree.

     If range of values is much bigger than number of values,
     or if it is too large to represent in a HOST_WIDE_INT,
     make a sequence of conditional branches instead of a dispatch.

     The definition of "much bigger" depends on whether we are
     optimizing for size or for speed.  If the former, the maximum
     ratio range/count = 3, because this was found to be the optimal
     ratio for size on i686-pc-linux-gnu, see PR11823.  The ratio
     10 is much older, and was probably selected after an extensive
     benchmarking investigation on numerous platforms.  Or maybe it
     just made sense to someone at some point in the history of GCC,
     who knows...  */
  max_ratio = optimize_insn_for_size_p () ? 3 : 10;
  if (count < case_values_threshold () || !tree_fits_uhwi_p (range)
      || compare_tree_int (range, max_ratio * count) > 0)
    return true;

  return false;
}

/* Add an unconditional jump to CASE_BB that happens in basic block BB.  */

static void
emit_jump (basic_block bb, basic_block case_bb)
{
  edge e = single_succ_edge (bb);
  redirect_edge_succ (e, case_bb);
}

/* Generate a decision tree, switching on INDEX_EXPR and jumping to
   one of the labels in CASE_LIST or to the DEFAULT_LABEL.
   DEFAULT_PROB is the estimated probability that it jumps to
   DEFAULT_LABEL.

   We generate a binary decision tree to select the appropriate target
   code.  */

static void
emit_case_decision_tree (gswitch *s, tree index_expr, tree index_type,
			 case_node_ptr case_list, basic_block default_bb,
			 tree default_label, profile_probability default_prob)
{
  balance_case_nodes (&case_list, NULL);

  if (dump_file)
    dump_function_to_file (current_function_decl, dump_file, dump_flags);
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      int indent_step = ceil_log2 (TYPE_PRECISION (index_type)) + 2;
      fprintf (dump_file, ";; Expanding GIMPLE switch as decision tree:\n");
      dump_case_nodes (dump_file, case_list, indent_step, 0);
    }

  basic_block bb = gimple_bb (s);
  gimple_stmt_iterator gsi = gsi_last_bb (bb);
  edge e;
  if (gsi_end_p (gsi))
    e = split_block_after_labels (bb);
  else
    {
      gsi_prev (&gsi);
      e = split_block (bb, gsi_stmt (gsi));
    }
  bb = split_edge (e);

  bb = emit_case_nodes (bb, index_expr, case_list, default_bb, default_label,
			default_prob, index_type);

  if (bb)
    emit_jump (bb, default_bb);

  /* Remove all edges and do just an edge that will reach default_bb.  */
  gsi = gsi_last_bb (gimple_bb (s));
  gsi_remove (&gsi, true);
}

static void
add_phi_operand_mapping (const vec<basic_block> bbs, basic_block switch_bb,
			 hash_map <tree, tree> *map)
{
  /* Record all PHI nodes that have to be fixed after conversion.  */
  for (unsigned i = 0; i < bbs.length (); i++)
    {
      basic_block bb = bbs[i];

      gphi_iterator gsi;
      for (gsi = gsi_start_phis (bb); !gsi_end_p (gsi); gsi_next (&gsi))
	{
	  gphi *phi = gsi.phi ();

	  for (unsigned i = 0; i < gimple_phi_num_args (phi); i++)
	    {
	      basic_block phi_src_bb = gimple_phi_arg_edge (phi, i)->src;
	      if (phi_src_bb == switch_bb)
		{
		  tree def = gimple_phi_arg_def (phi, i);
		  tree result = gimple_phi_result (phi);
		  map->put (result, def);
		  break;
		}
	    }
	}
    }
}
static void
fix_phi_operands_after_transform (auto_vec<basic_block> &case_bbs,
				  hash_map<tree, tree> *phis_to_fix)
{
  for (unsigned i = 0; i < case_bbs.length (); i++)
    {
      basic_block bb = case_bbs[i];
      gphi_iterator gsi;
      for (gsi = gsi_start_phis (bb); !gsi_end_p (gsi); gsi_next (&gsi))
	{
	  gphi *phi = gsi.phi ();

	  tree *replacement = phis_to_fix->get (gimple_phi_result (phi));
	  if (replacement)
	    {
	      for (unsigned j = 0; j < gimple_phi_num_args (phi); j++)
		{
		  tree def = gimple_phi_arg_def (phi, j);
		  if (def == NULL_TREE)
		    *gimple_phi_arg_def_ptr (phi, j) = *replacement;
		}
	    }
	}
    }
}

/* Attempt to expand gimple switch STMT to a decision tree.  */

static bool
try_switch_expansion (gswitch *stmt)
{
  tree minval = NULL_TREE, maxval = NULL_TREE, range = NULL_TREE;
  basic_block default_bb;
  unsigned int count, uniq;
  int i;
  int ncases = gimple_switch_num_labels (stmt);
  tree index_expr = gimple_switch_index (stmt);
  tree index_type = TREE_TYPE (index_expr);
  tree elt;
  basic_block bb = gimple_bb (stmt);

  hash_map<tree, tree> phis_to_fix;
  auto_vec<basic_block> case_bbs;

  /* A list of case labels; it is first built as a list and it may then
     be rearranged into a nearly balanced binary tree.  */
  case_node *case_list = 0;

  /* A pool for case nodes.  */
  object_allocator<case_node> case_node_pool ("struct case_node pool");

  /* cleanup_tree_cfg removes all SWITCH_EXPR with their index
     expressions being INTEGER_CST.  */
  gcc_assert (TREE_CODE (index_expr) != INTEGER_CST);

  /* Optimization of switch statements with only one label has already
     occurred, so we should never see them at this point.  */
  gcc_assert (ncases > 1);

  /* Find the default case target label.  */
  tree default_label = CASE_LABEL (gimple_switch_default_label (stmt));
  default_bb = label_to_block_fn (cfun, default_label);
  edge default_edge = EDGE_SUCC (bb, 0);
  profile_probability default_prob = default_edge->probability;
  case_bbs.safe_push (default_bb);

  /* Get upper and lower bounds of case values.  */
  elt = gimple_switch_label (stmt, 1);
  minval = fold_convert (index_type, CASE_LOW (elt));
  elt = gimple_switch_label (stmt, ncases - 1);
  if (CASE_HIGH (elt))
    maxval = fold_convert (index_type, CASE_HIGH (elt));
  else
    maxval = fold_convert (index_type, CASE_LOW (elt));

  /* Compute span of values.  */
  range = fold_build2 (MINUS_EXPR, index_type, maxval, minval);

  /* Listify the labels queue and gather some numbers to decide
     how to expand this switch.  */
  uniq = 0;
  count = 0;
  hash_set<tree> seen_labels;
  compute_cases_per_edge (stmt);

  for (i = ncases - 1; i >= 1; --i)
    {
      elt = gimple_switch_label (stmt, i);
      tree low = CASE_LOW (elt);
      gcc_assert (low);
      tree high = CASE_HIGH (elt);
      gcc_assert (!high || tree_int_cst_lt (low, high));
      tree lab = CASE_LABEL (elt);

      /* Count the elements.
	 A range counts double, since it requires two compares.  */
      count++;
      if (high)
	count++;

      /* If we have not seen this label yet, then increase the
	 number of unique case node targets seen.  */
      if (!seen_labels.add (lab))
	uniq++;

      /* The bounds on the case range, LOW and HIGH, have to be converted
	 to case's index type TYPE.  Note that the original type of the
	 case index in the source code is usually "lost" during
	 gimplification due to type promotion, but the case labels retain the
	 original type.  Make sure to drop overflow flags.  */
      low = fold_convert (index_type, low);
      if (TREE_OVERFLOW (low))
	low = wide_int_to_tree (index_type, low);

      /* The canonical from of a case label in GIMPLE is that a simple case
	 has an empty CASE_HIGH.  For the casesi and tablejump expanders,
	 the back ends want simple cases to have high == low.  */
      if (!high)
	high = low;
      high = fold_convert (index_type, high);
      if (TREE_OVERFLOW (high))
	high = wide_int_to_tree (index_type, high);

      basic_block case_bb = label_to_block_fn (cfun, lab);
      edge case_edge = find_edge (bb, case_bb);
      case_list = add_case_node (
	case_list, low, high, case_bb, lab,
	case_edge->probability.apply_scale (1, (intptr_t) (case_edge->aux)),
	case_node_pool);

      case_bbs.safe_push (case_bb);
    }
  reset_out_edges_aux (bb);
  add_phi_operand_mapping (case_bbs, bb, &phis_to_fix);

  /* cleanup_tree_cfg removes all SWITCH_EXPR with a single
     destination, such as one with a default case only.
     It also removes cases that are out of range for the switch
     type, so we should never get a zero here.  */
  gcc_assert (count > 0);

  /* Decide how to expand this switch.
     The two options at this point are a dispatch table (casesi or
     tablejump) or a decision tree.  */

  if (expand_switch_as_decision_tree_p (range, uniq, count))
    {
      emit_case_decision_tree (stmt, index_expr, index_type, case_list,
			       default_bb, default_label, default_prob);
      fix_phi_operands_after_transform (case_bbs, &phis_to_fix);
      return true;
    }

  return false;
}

/* The main function of the pass scans statements for switches and invokes
   process_switch on them.  */

namespace {

const pass_data pass_data_lower_switch =
{
  GIMPLE_PASS, /* type */
  "switchlower", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_TREE_SWITCH_LOWERING, /* tv_id */
  ( PROP_cfg | PROP_ssa ), /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_update_ssa | TODO_cleanup_cfg, /* todo_flags_finish */
};

class pass_lower_switch : public gimple_opt_pass
{
public:
  pass_lower_switch (gcc::context *ctxt)
    : gimple_opt_pass (pass_data_lower_switch, ctxt)
  {}

  /* opt_pass methods: */
  virtual bool gate (function *) { return true; }
  virtual unsigned int execute (function *);

}; // class pass_lower_switch

unsigned int
pass_lower_switch::execute (function *fun)
{
  basic_block bb;
  bool expanded = false;

  FOR_EACH_BB_FN (bb, fun)
    {
      gimple *stmt = last_stmt (bb);
      if (stmt && gimple_code (stmt) == GIMPLE_SWITCH)
	{
	  if (dump_file)
	    {
	      expanded_location loc = expand_location (gimple_location (stmt));

	      fprintf (dump_file, "beginning to process the following "
				  "SWITCH statement (%s:%d) : ------- \n",
		       loc.file, loc.line);
	      print_gimple_stmt (dump_file, stmt, 0, TDF_SLIM);
	      putc ('\n', dump_file);
	    }

	  expanded |= try_switch_expansion (as_a<gswitch *> (stmt));
	}
    }

  if (expanded)
    {
      free_dominance_info (CDI_DOMINATORS);
      free_dominance_info (CDI_POST_DOMINATORS);
    }

  return 0;
}

} // anon namespace

gimple_opt_pass *
make_pass_lower_switch (gcc::context *ctxt)
{
  return new pass_lower_switch (ctxt);
}

/* Generate code to jump to LABEL if OP0 and OP1 are equal in mode MODE.
   PROB is the probability of jumping to LABEL.  */
static basic_block
do_jump_if_equal (basic_block bb, tree op0, tree op1, basic_block label_bb,
		  profile_probability prob)
{
  gcond *cond = gimple_build_cond (EQ_EXPR, op0, op1, NULL_TREE, NULL_TREE);
  gimple_stmt_iterator gsi = gsi_last_bb (bb);
  gsi_insert_before (&gsi, cond, GSI_SAME_STMT);

  gcc_assert (single_succ_p (bb));

  /* Make a new basic block where false branch will take place.  */
  edge false_edge = split_block (bb, cond);
  false_edge->flags = EDGE_FALSE_VALUE;
  false_edge->probability = prob.invert ();

  edge true_edge = make_edge (bb, label_bb, EDGE_TRUE_VALUE);
  true_edge->probability = prob;

  return false_edge->dest;
}

/* Generate code to compare X with Y so that the condition codes are
   set and to jump to LABEL if the condition is true.  If X is a
   constant and Y is not a constant, then the comparison is swapped to
   ensure that the comparison RTL has the canonical form.

   UNSIGNEDP nonzero says that X and Y are unsigned; this matters if they
   need to be widened.  UNSIGNEDP is also used to select the proper
   branch condition code.

   If X and Y have mode BLKmode, then SIZE specifies the size of both X and Y.

   MODE is the mode of the inputs (in case they are const_int).

   COMPARISON is the rtl operator to compare with (EQ, NE, GT, etc.).
   It will be potentially converted into an unsigned variant based on
   UNSIGNEDP to select a proper jump instruction.

   PROB is the probability of jumping to LABEL.  */

static basic_block
emit_cmp_and_jump_insns (basic_block bb, tree op0, tree op1,
			 tree_code comparison, basic_block label_bb,
			 profile_probability prob)
{
  gcond *cond = gimple_build_cond (comparison, op0, op1, NULL_TREE, NULL_TREE);
  gimple_stmt_iterator gsi = gsi_last_bb (bb);
  gsi_insert_after (&gsi, cond, GSI_NEW_STMT);

  gcc_assert (single_succ_p (bb));

  /* Make a new basic block where false branch will take place.  */
  edge false_edge = split_block (bb, cond);
  false_edge->flags = EDGE_FALSE_VALUE;
  false_edge->probability = prob.invert ();

  edge true_edge = make_edge (bb, label_bb, EDGE_TRUE_VALUE);
  true_edge->probability = prob;

  return false_edge->dest;
}

/* Computes the conditional probability of jumping to a target if the branch
   instruction is executed.
   TARGET_PROB is the estimated probability of jumping to a target relative
   to some basic block BB.
   BASE_PROB is the probability of reaching the branch instruction relative
   to the same basic block BB.  */

static inline profile_probability
conditional_probability (profile_probability target_prob,
			 profile_probability base_prob)
{
  return target_prob / base_prob;
}

/* Emit step-by-step code to select a case for the value of INDEX.
   The thus generated decision tree follows the form of the
   case-node binary tree NODE, whose nodes represent test conditions.
   INDEX_TYPE is the type of the index of the switch.

   Care is taken to prune redundant tests from the decision tree
   by detecting any boundary conditions already checked by
   emitted rtx.  (See node_has_high_bound, node_has_low_bound
   and node_is_bounded, above.)

   Where the test conditions can be shown to be redundant we emit
   an unconditional jump to the target code.  As a further
   optimization, the subordinates of a tree node are examined to
   check for bounded nodes.  In this case conditional and/or
   unconditional jumps as a result of the boundary check for the
   current node are arranged to target the subordinates associated
   code for out of bound conditions on the current node.

   We can assume that when control reaches the code generated here,
   the index value has already been compared with the parents
   of this node, and determined to be on the same side of each parent
   as this node is.  Thus, if this node tests for the value 51,
   and a parent tested for 52, we don't need to consider
   the possibility of a value greater than 51.  If another parent
   tests for the value 50, then this node need not test anything.  */

static basic_block
emit_case_nodes (basic_block bb, tree index, case_node_ptr node,
		 basic_block default_bb, tree default_label,
		 profile_probability default_prob, tree index_type)
{
  /* If INDEX has an unsigned type, we must make unsigned branches.  */
  profile_probability probability;
  profile_probability prob = node->prob, subtree_prob = node->subtree_prob;

  /* See if our parents have already tested everything for us.
     If they have, emit an unconditional jump for this node.  */
  if (node_is_bounded (node, index_type))
    {
      emit_jump (bb, node->case_bb);
      return NULL;
    }

  else if (tree_int_cst_equal (node->low, node->high))
    {
      probability = conditional_probability (prob, subtree_prob + default_prob);
      /* Node is single valued.  First see if the index expression matches
	 this node and then check our children, if any.  */
      bb = do_jump_if_equal (bb, index, node->low, node->case_bb, probability);
      /* Since this case is taken at this point, reduce its weight from
	 subtree_weight.  */
      subtree_prob -= prob;
      if (node->right != 0 && node->left != 0)
	{
	  /* This node has children on both sides.
	     Dispatch to one side or the other
	     by comparing the index value with this node's value.
	     If one subtree is bounded, check that one first,
	     so we can avoid real branches in the tree.  */

	  if (node_is_bounded (node->right, index_type))
	    {
	      probability
		= conditional_probability (node->right->prob,
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, index, node->high, GT_EXPR,
					    node->right->case_bb, probability);
	      bb = emit_case_nodes (bb, index, node->left, default_bb,
				    default_label, default_prob, index_type);
	    }

	  else if (node_is_bounded (node->left, index_type))
	    {
	      probability
		= conditional_probability (node->left->prob,
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, index, node->high, LT_EXPR,
					    node->left->case_bb, probability);
	      bb = emit_case_nodes (bb, index, node->right, default_bb,
				    default_label, default_prob, index_type);
	    }

	  /* If both children are single-valued cases with no
	     children, finish up all the work.  This way, we can save
	     one ordered comparison.  */
	  else if (tree_int_cst_equal (node->right->low, node->right->high)
		   && node->right->left == 0 && node->right->right == 0
		   && tree_int_cst_equal (node->left->low, node->left->high)
		   && node->left->left == 0 && node->left->right == 0)
	    {
	      /* Neither node is bounded.  First distinguish the two sides;
		 then emit the code for one side at a time.  */

	      /* See if the value matches what the right hand side
		 wants.  */
	      probability
		= conditional_probability (node->right->prob,
					   subtree_prob + default_prob);
	      bb = do_jump_if_equal (bb, index, node->right->low,
				     node->right->case_bb, probability);

	      /* See if the value matches what the left hand side
		 wants.  */
	      probability
		= conditional_probability (node->left->prob,
					   subtree_prob + default_prob);
	      bb = do_jump_if_equal (bb, index, node->left->low,
				     node->left->case_bb, probability);
	    }

	  else
	    {
	      /* Neither node is bounded.  First distinguish the two sides;
		 then emit the code for one side at a time.  */

	      basic_block test_bb = split_edge (single_succ_edge (bb));
	      redirect_edge_succ (single_pred_edge (test_bb),
				  single_succ_edge (bb)->dest);

	      /* The default label could be reached either through the right
		 subtree or the left subtree.  Divide the probability
		 equally.  */
	      probability
		= conditional_probability (node->right->subtree_prob
					     + default_prob.apply_scale (1, 2),
					   subtree_prob + default_prob);
	      /* See if the value is on the right.  */
	      bb = emit_cmp_and_jump_insns (bb, index, node->high, GT_EXPR,
					    test_bb, probability);
	      default_prob = default_prob.apply_scale (1, 2);

	      /* Value must be on the left.
		 Handle the left-hand subtree.  */
	      bb = emit_case_nodes (bb, index, node->left, default_bb,
				    default_label, default_prob, index_type);
	      /* If left-hand subtree does nothing,
		 go to default.  */

	      if (bb && default_bb)
		emit_jump (bb, default_bb);

	      /* Code branches here for the right-hand subtree.  */
	      bb = emit_case_nodes (test_bb, index, node->right, default_bb,
				    default_label, default_prob, index_type);
	    }
	}
      else if (node->right != 0 && node->left == 0)
	{
	  /* Here we have a right child but no left so we issue a conditional
	     branch to default and process the right child.

	     Omit the conditional branch to default if the right child
	     does not have any children and is single valued; it would
	     cost too much space to save so little time.  */

	  if (node->right->right || node->right->left
	      || !tree_int_cst_equal (node->right->low, node->right->high))
	    {
	      if (!node_has_low_bound (node, index_type))
		{
		  probability
		    = conditional_probability (default_prob.apply_scale (1, 2),
					       subtree_prob + default_prob);
		  bb = emit_cmp_and_jump_insns (bb, index, node->high, LT_EXPR,
						default_bb, probability);
		  default_prob = default_prob.apply_scale (1, 2);
		}

	      bb = emit_case_nodes (bb, index, node->right, default_bb,
				    default_label, default_prob, index_type);
	    }
	  else
	    {
	      probability
		= conditional_probability (node->right->subtree_prob,
					   subtree_prob + default_prob);
	      /* We cannot process node->right normally
		 since we haven't ruled out the numbers less than
		 this node's value.  So handle node->right explicitly.  */
	      bb = do_jump_if_equal (bb, index, node->right->low,
				     node->right->case_bb, probability);
	    }
	}

      else if (node->right == 0 && node->left != 0)
	{
	  /* Just one subtree, on the left.  */
	  if (node->left->left || node->left->right
	      || !tree_int_cst_equal (node->left->low, node->left->high))
	    {
	      if (!node_has_high_bound (node, index_type))
		{
		  probability
		    = conditional_probability (default_prob.apply_scale (1, 2),
					       subtree_prob + default_prob);
		  bb = emit_cmp_and_jump_insns (bb, index, node->high, GT_EXPR,
						default_bb, probability);
		  default_prob = default_prob.apply_scale (1, 2);
		}

	      bb = emit_case_nodes (bb, index, node->left, default_bb,
				    default_label, default_prob, index_type);
	    }
	  else
	    {
	      probability
		= conditional_probability (node->left->subtree_prob,
					   subtree_prob + default_prob);
	      /* We cannot process node->left normally
		 since we haven't ruled out the numbers less than
		 this node's value.  So handle node->left explicitly.  */
	      do_jump_if_equal (bb, index, node->left->low, node->left->case_bb,
				probability);
	    }
	}
    }
  else
    {
      /* Node is a range.  These cases are very similar to those for a single
	 value, except that we do not start by testing whether this node
	 is the one to branch to.  */

      if (node->right != 0 && node->left != 0)
	{
	  /* Node has subtrees on both sides.
	     If the right-hand subtree is bounded,
	     test for it first, since we can go straight there.
	     Otherwise, we need to make a branch in the control structure,
	     then handle the two subtrees.  */
	  basic_block test_bb = NULL;

	  if (node_is_bounded (node->right, index_type))
	    {
	      /* Right hand node is fully bounded so we can eliminate any
		 testing and branch directly to the target code.  */
	      probability
		= conditional_probability (node->right->subtree_prob,
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, index, node->high, GT_EXPR,
					    node->right->case_bb, probability);
	    }
	  else
	    {
	      /* Right hand node requires testing.
		 Branch to a label where we will handle it later.  */

	      test_bb = split_edge (single_succ_edge (bb));
	      redirect_edge_succ (single_pred_edge (test_bb),
				  single_succ_edge (bb)->dest);

	      probability
		= conditional_probability (node->right->subtree_prob
					     + default_prob.apply_scale (1, 2),
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, index, node->high, GT_EXPR,
					    test_bb, probability);
	      default_prob = default_prob.apply_scale (1, 2);
	    }

	  /* Value belongs to this node or to the left-hand subtree.  */

	  probability
	    = conditional_probability (prob, subtree_prob + default_prob);
	  bb = emit_cmp_and_jump_insns (bb, index, node->low, GE_EXPR,
					node->case_bb, probability);

	  /* Handle the left-hand subtree.  */
	  bb = emit_case_nodes (bb, index, node->left, default_bb,
				default_label, default_prob, index_type);

	  /* If right node had to be handled later, do that now.  */
	  if (test_bb)
	    {
	      /* If the left-hand subtree fell through,
		 don't let it fall into the right-hand subtree.  */
	      if (bb && default_bb)
		emit_jump (bb, default_bb);

	      bb = emit_case_nodes (test_bb, index, node->right, default_bb,
				    default_label, default_prob, index_type);
	    }
	}

      else if (node->right != 0 && node->left == 0)
	{
	  /* Deal with values to the left of this node,
	     if they are possible.  */
	  if (!node_has_low_bound (node, index_type))
	    {
	      probability
		= conditional_probability (default_prob.apply_scale (1, 2),
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, index, node->low, LT_EXPR,
					    default_bb, probability);
	      default_prob = default_prob.apply_scale (1, 2);
	    }

	  /* Value belongs to this node or to the right-hand subtree.  */

	  probability
	    = conditional_probability (prob, subtree_prob + default_prob);
	  bb = emit_cmp_and_jump_insns (bb, index, node->high, LE_EXPR,
					node->case_bb, probability);

	  bb = emit_case_nodes (bb, index, node->right, default_bb,
				default_label, default_prob, index_type);
	}

      else if (node->right == 0 && node->left != 0)
	{
	  /* Deal with values to the right of this node,
	     if they are possible.  */
	  if (!node_has_high_bound (node, index_type))
	    {
	      probability
		= conditional_probability (default_prob.apply_scale (1, 2),
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, index, node->high, GT_EXPR,
					    default_bb, probability);
	      default_prob = default_prob.apply_scale (1, 2);
	    }

	  /* Value belongs to this node or to the left-hand subtree.  */

	  probability
	    = conditional_probability (prob, subtree_prob + default_prob);
	  bb = emit_cmp_and_jump_insns (bb, index, node->low, GE_EXPR,
					node->case_bb, probability);

	  bb = emit_case_nodes (bb, index, node->left, default_bb,
				default_label, default_prob, index_type);
	}

      else
	{
	  /* Node has no children so we check low and high bounds to remove
	     redundant tests.  Only one of the bounds can exist,
	     since otherwise this node is bounded--a case tested already.  */
	  bool high_bound = node_has_high_bound (node, index_type);
	  bool low_bound = node_has_low_bound (node, index_type);

	  if (!high_bound && low_bound)
	    {
	      probability
		= conditional_probability (default_prob,
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, index, node->high, GT_EXPR,
					    default_bb, probability);
	    }

	  else if (!low_bound && high_bound)
	    {
	      probability
		= conditional_probability (default_prob,
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, index, node->low, LT_EXPR,
					    default_bb, probability);
	    }
	  else if (!low_bound && !high_bound)
	    {
	      tree type = TREE_TYPE (index);
	      tree utype = unsigned_type_for (type);

	      tree lhs = make_ssa_name (type);
	      gassign *sub1
		= gimple_build_assign (lhs, MINUS_EXPR, index, node->low);

	      tree converted = make_ssa_name (utype);
	      gassign *a = gimple_build_assign (converted, NOP_EXPR, lhs);

	      tree rhs = fold_build2 (MINUS_EXPR, utype,
				      fold_convert (type, node->high),
				      fold_convert (type, node->low));
	      gimple_stmt_iterator gsi = gsi_last_bb (bb);
	      gsi_insert_before (&gsi, sub1, GSI_SAME_STMT);
	      gsi_insert_before (&gsi, a, GSI_SAME_STMT);

	      probability
		= conditional_probability (default_prob,
					   subtree_prob + default_prob);
	      bb = emit_cmp_and_jump_insns (bb, converted, rhs, GT_EXPR,
					    default_bb, probability);
	    }

	  emit_jump (bb, node->case_bb);
	  return NULL;
	}
    }

  return bb;
}

/* Search the parent sections of the case node tree
   to see if a test for the lower bound of NODE would be redundant.
   INDEX_TYPE is the type of the index expression.

   The instructions to generate the case decision tree are
   output in the same order as nodes are processed so it is
   known that if a parent node checks the range of the current
   node minus one that the current node is bounded at its lower
   span.  Thus the test would be redundant.  */

static bool
node_has_low_bound (case_node_ptr node, tree index_type)
{
  tree low_minus_one;
  case_node_ptr pnode;

  /* If the lower bound of this node is the lowest value in the index type,
     we need not test it.  */

  if (tree_int_cst_equal (node->low, TYPE_MIN_VALUE (index_type)))
    return true;

  /* If this node has a left branch, the value at the left must be less
     than that at this node, so it cannot be bounded at the bottom and
     we need not bother testing any further.  */

  if (node->left)
    return false;

  low_minus_one = fold_build2 (MINUS_EXPR, TREE_TYPE (node->low), node->low,
			       build_int_cst (TREE_TYPE (node->low), 1));

  /* If the subtraction above overflowed, we can't verify anything.
     Otherwise, look for a parent that tests our value - 1.  */

  if (!tree_int_cst_lt (low_minus_one, node->low))
    return false;

  for (pnode = node->parent; pnode; pnode = pnode->parent)
    if (tree_int_cst_equal (low_minus_one, pnode->high))
      return true;

  return false;
}

/* Search the parent sections of the case node tree
   to see if a test for the upper bound of NODE would be redundant.
   INDEX_TYPE is the type of the index expression.

   The instructions to generate the case decision tree are
   output in the same order as nodes are processed so it is
   known that if a parent node checks the range of the current
   node plus one that the current node is bounded at its upper
   span.  Thus the test would be redundant.  */

static bool
node_has_high_bound (case_node_ptr node, tree index_type)
{
  tree high_plus_one;
  case_node_ptr pnode;

  /* If there is no upper bound, obviously no test is needed.  */

  if (TYPE_MAX_VALUE (index_type) == NULL)
    return true;

  /* If the upper bound of this node is the highest value in the type
     of the index expression, we need not test against it.  */

  if (tree_int_cst_equal (node->high, TYPE_MAX_VALUE (index_type)))
    return true;

  /* If this node has a right branch, the value at the right must be greater
     than that at this node, so it cannot be bounded at the top and
     we need not bother testing any further.  */

  if (node->right)
    return false;

  high_plus_one = fold_build2 (PLUS_EXPR, TREE_TYPE (node->high), node->high,
			       build_int_cst (TREE_TYPE (node->high), 1));

  /* If the addition above overflowed, we can't verify anything.
     Otherwise, look for a parent that tests our value + 1.  */

  if (!tree_int_cst_lt (node->high, high_plus_one))
    return false;

  for (pnode = node->parent; pnode; pnode = pnode->parent)
    if (tree_int_cst_equal (high_plus_one, pnode->low))
      return true;

  return false;
}

/* Search the parent sections of the
   case node tree to see if both tests for the upper and lower
   bounds of NODE would be redundant.  */

static bool
node_is_bounded (case_node_ptr node, tree index_type)
{
  return (node_has_low_bound (node, index_type)
	  && node_has_high_bound (node, index_type));
}
