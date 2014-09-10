/* Interprocedural Identical Code Folding pass
   Copyright (C) 2014 Free Software Foundation, Inc.

   Contributed by Jan Hubicka <hubicka@ucw.cz> and Martin Liska <mliska@suse.cz>

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
#include "coretypes.h"
#include "tree.h"
#include "basic-block.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "gimple-expr.h"
#include "is-a.h"
#include "gimple.h"
#include "expr.h"
#include "gimple-iterator.h"
#include "gimple-ssa.h"
#include "tree-cfg.h"
#include "stringpool.h"
#include "tree-dfa.h"
#include "tree-pass.h"
#include "gimple-pretty-print.h"
#include "cfgloop.h"
#include "except.h"
#include "data-streamer.h"
#include "ipa-utils.h"
#include "ipa-icf.h"

#include "tree-phinodes.h"
#include "ssa-iterators.h"
#include "tree-eh.h"

namespace ipa_icf {

static bool
stmt_local_def (gimple stmt)
{
  basic_block bb, def_bb;
  imm_use_iterator iter;
  use_operand_p use_p;
  tree val;
  def_operand_p def_p;

  if (gimple_has_side_effects (stmt)
      || stmt_could_throw_p (stmt)
      || gimple_vdef (stmt) != NULL_TREE)
    return false;

  def_p = SINGLE_SSA_DEF_OPERAND (stmt, SSA_OP_DEF);
  if (def_p == NULL)
    return false;

  val = DEF_FROM_PTR (def_p);
  if (val == NULL_TREE || TREE_CODE (val) != SSA_NAME)
    return false;

  def_bb = gimple_bb (stmt);

  FOR_EACH_IMM_USE_FAST (use_p, iter, val)
    {
      if (is_gimple_debug (USE_STMT (use_p)))
	continue;
      bb = gimple_bb (USE_STMT (use_p));
      if (bb == def_bb)
	continue;

      if (gimple_code (USE_STMT (use_p)) == GIMPLE_PHI
	  && EDGE_PRED (bb, PHI_ARG_INDEX_FROM_USE (use_p))->src == def_bb)
	continue;

      return false;
    }

  return true;
}

static void
gsi_advance_fw_nondebug_nonlocal (gimple_stmt_iterator *gsi, bool skip_local_defs)
{
  gimple stmt;

  while (true)
    {
      if (gsi_end_p (*gsi))
	return;
      stmt = gsi_stmt (*gsi);

      if (is_gimple_debug (stmt))
      {}      
      else if (!stmt_local_def (stmt) || !skip_local_defs)
	return;

      gsi_next_nondebug (gsi);
    }
}


/* Basic block equivalence comparison function that returns true if
   basic blocks BB1 and BB2 (from functions FUNC1 and FUNC2) correspond.  */

bool
sem_function::compare_bb (sem_bb *bb1, sem_bb *bb2, tree func1, tree func2, bool skip_local_defs)
{
  unsigned i;
  gimple_stmt_iterator gsi1, gsi2;
  gimple s1, s2;

  if (bb1->edge_count != bb2->edge_count)
    return RETURN_FALSE ();

  if (skip_local_defs)
  {
    if (bb1->nondbg_nonlocal_stmt_count != bb2->nondbg_nonlocal_stmt_count)
      return false;
  }
  else
  {
    if (bb1->nondbg_stmt_count != bb2->nondbg_stmt_count)
      return false;
  }

  unsigned int boundary = skip_local_defs ? bb1->nondbg_nonlocal_stmt_count : bb1->nondbg_stmt_count;

  gsi1 = gsi_start_bb (bb1->bb);
  gsi2 = gsi_start_bb (bb2->bb);

  for (i = 0; i < boundary; i++)
    {
      s1 = gsi_stmt (gsi1);
      s2 = gsi_stmt (gsi2);

      if (is_gimple_debug (s1) || stmt_local_def (s1))
	gsi_advance_fw_nondebug_nonlocal (&gsi1, skip_local_defs);

      if (is_gimple_debug (s2) || stmt_local_def (s2))
	gsi_advance_fw_nondebug_nonlocal (&gsi2, skip_local_defs);

      s1 = gsi_stmt (gsi1);
      s2 = gsi_stmt (gsi2);

      if (gimple_code (s1) != gimple_code (s2))
	return RETURN_FALSE_WITH_MSG ("gimple codes are different");

      switch (gimple_code (s1))
	{
	case GIMPLE_CALL:
	  if (!compare_gimple_call (s1, s2, func1, func2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_CALL");
	  break;
	case GIMPLE_ASSIGN:
	  if (!compare_gimple_assign (s1, s2, func1, func2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_ASSIGN");
	  break;
	case GIMPLE_COND:
	  if (!compare_gimple_cond (s1, s2, func1, func2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_COND");
	  break;
	case GIMPLE_SWITCH:
	  if (!compare_gimple_switch (s1, s2, func1, func2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_SWITCH");
	  break;
	case GIMPLE_DEBUG:
	case GIMPLE_EH_DISPATCH:
	  break;
	case GIMPLE_RESX:
	  if (!compare_gimple_resx (s1, s2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_RESX");
	  break;
	case GIMPLE_LABEL:
	  if (!compare_gimple_label (s1, s2, func1, func2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_LABEL");
	  break;
	case GIMPLE_RETURN:
	  if (!compare_gimple_return (s1, s2, func1, func2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_RETURN");
	  break;
	case GIMPLE_GOTO:
	  if (!compare_gimple_goto (s1, s2, func1, func2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_GOTO");
	  break;
	case GIMPLE_ASM:
	  if (!compare_gimple_asm (s1, s2))
	    return RETURN_DIFFERENT_STMTS (s1, s2, "GIMPLE_ASM");
	  break;
	case GIMPLE_PREDICT:
	case GIMPLE_NOP:
	  return true;
	default:
	  return RETURN_FALSE_WITH_MSG ("Unknown GIMPLE code reached");
	}

      gsi_next (&gsi1);
      gsi_next (&gsi2);
    }

  return true;
}


/* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
   call statements are semantically equivalent.  */

bool
sem_function::compare_gimple_call (gimple s1, gimple s2, tree func1, tree func2)
{
  unsigned i;
  tree t1, t2;

  if (gimple_call_num_args (s1) != gimple_call_num_args (s2))
    return false;

  t1 = gimple_call_fndecl (s1);
  t2 = gimple_call_fndecl (s2);

  /* Function pointer variables are not supported yet.  */
  if (t1 == NULL || t2 == NULL)
    {
      if (!compare_operand (t1, t2, func1, func2))
	return RETURN_FALSE();
    }
  else if (!compare_function_decl (t1, t2))
    return false;

  /* Checking of argument.  */
  for (i = 0; i < gimple_call_num_args (s1); ++i)
    {
      t1 = gimple_call_arg (s1, i);
      t2 = gimple_call_arg (s2, i);

      if (!compare_operand (t1, t2, func1, func2))
	return false;
    }

  /* Return value checking.  */
  t1 = gimple_get_lhs (s1);
  t2 = gimple_get_lhs (s2);

  return compare_operand (t1, t2, func1, func2, false);
}

/* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
   assignment statements are semantically equivalent.  */

bool
sem_function::compare_gimple_assign (gimple s1, gimple s2, tree func1,
				     tree func2)
{
  tree arg1, arg2;
  tree_code code1, code2;
  unsigned i;

  code1 = gimple_expr_code (s1);
  code2 = gimple_expr_code (s2);

  if (code1 != code2)
    return false;

  code1 = gimple_assign_rhs_code (s1);
  code2 = gimple_assign_rhs_code (s2);

  if (code1 != code2)
    return false;

  for (i = 0; i < gimple_num_ops (s1); i++)
    {
      arg1 = gimple_op (s1, i);
      arg2 = gimple_op (s2, i);

      if (!compare_operand (arg1, arg2, func1, func2, i != 0))
	return false;
    }

  return true;
}

/* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
   condition statements are semantically equivalent.  */

bool
sem_function::compare_gimple_cond (gimple s1, gimple s2, tree func1, tree func2)
{
  tree t1, t2;
  tree_code code1, code2;

  code1 = gimple_expr_code (s1);
  code2 = gimple_expr_code (s2);

  if (code1 != code2)
    return false;

  t1 = gimple_cond_lhs (s1);
  t2 = gimple_cond_lhs (s2);

  if (!compare_operand (t1, t2, func1, func2))
    return false;

  t1 = gimple_cond_rhs (s1);
  t2 = gimple_cond_rhs (s2);

  return compare_operand (t1, t2, func1, func2);
}

/* Verifies that tree labels T1 and T2 correspond in FUNC1 and FUNC2.  */

bool
sem_function::compare_tree_ssa_label (tree t1, tree t2, tree func1, tree func2)
{
  return compare_operand (t1, t2, func1, func2);
}

/* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
   label statements are semantically equivalent.  */

bool
sem_function::compare_gimple_label (gimple g1, gimple g2, tree func1,
				    tree func2)
{
  tree t1 = gimple_label_label (g1);
  tree t2 = gimple_label_label (g2);

  return compare_tree_ssa_label (t1, t2, func1, func2);
}

/* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
   switch statements are semantically equivalent.  */

bool
sem_function::compare_gimple_switch (gimple g1, gimple g2, tree func1,
				     tree func2)
{
  unsigned lsize1, lsize2, i;
  tree t1, t2, low1, low2, high1, high2;

  lsize1 = gimple_switch_num_labels (g1);
  lsize2 = gimple_switch_num_labels (g2);

  if (lsize1 != lsize2)
    return false;

  t1 = gimple_switch_index (g1);
  t2 = gimple_switch_index (g2);

  if (TREE_CODE (t1) != SSA_NAME || TREE_CODE(t2) != SSA_NAME)
    return false;

  if (!compare_operand (t1, t2, func1, func2))
    return false;

  for (i = 0; i < lsize1; i++)
    {
      low1 = CASE_LOW (gimple_switch_label (g1, i));
      low2 = CASE_LOW (gimple_switch_label (g2, i));

      if ((low1 != NULL) != (low2 != NULL)
	  || (low1 && low2 && TREE_INT_CST_LOW (low1) != TREE_INT_CST_LOW (low2)))
	return false;

      high1 = CASE_HIGH (gimple_switch_label (g1, i));
      high2 = CASE_HIGH (gimple_switch_label (g2, i));

      if ((high1 != NULL) != (high2 != NULL)
	  || (high1 && high2
	      && TREE_INT_CST_LOW (high1) != TREE_INT_CST_LOW (high2)))
	return false;
    }

  return true;
}

/* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
   return statements are semantically equivalent.  */

bool
sem_function::compare_gimple_return (gimple g1, gimple g2, tree func1,
				     tree func2)
{
  tree t1, t2;

  t1 = gimple_return_retval (g1);
  t2 = gimple_return_retval (g2);

  /* Void return type.  */
  if (t1 == NULL && t2 == NULL)
    return true;
  else
    return compare_operand (t1, t2, func1, func2);
}

/* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
   goto statements are semantically equivalent.  */

bool
sem_function::compare_gimple_goto (gimple g1, gimple g2, tree func1, tree func2)
{
  tree dest1, dest2;

  dest1 = gimple_goto_dest (g1);
  dest2 = gimple_goto_dest (g2);

  if (TREE_CODE (dest1) != TREE_CODE (dest2) || TREE_CODE (dest1) != SSA_NAME)
    return false;

  return compare_operand (dest1, dest2, func1, func2);
}

/* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
   resx statements are semantically equivalent.  */

bool
sem_function::compare_gimple_resx (gimple g1, gimple g2)
{
  return gimple_resx_region (g1) == gimple_resx_region (g2);
}

/* Verifies for given GIMPLEs S1 and S2 that ASM statements are equivalent.
   For the beginning, the pass only supports equality for
   '__asm__ __volatile__ ("", "", "", "memory")'.  */

bool
sem_function::compare_gimple_asm (gimple g1, gimple g2)
{
  if (gimple_asm_volatile_p (g1) != gimple_asm_volatile_p (g2))
    return false;

  if (gimple_asm_ninputs (g1) || gimple_asm_ninputs (g2))
    return false;

  if (gimple_asm_noutputs (g1) || gimple_asm_noutputs (g2))
    return false;

  if (gimple_asm_nlabels (g1) || gimple_asm_nlabels (g2))
    return false;

  if (gimple_asm_nclobbers (g1) != gimple_asm_nclobbers (g2))
    return false;

  for (unsigned i = 0; i < gimple_asm_nclobbers (g1); i++)
    {
      tree clobber1 = TREE_VALUE (gimple_asm_clobber_op (g1, i));
      tree clobber2 = TREE_VALUE (gimple_asm_clobber_op (g2, i));

      if (!operand_equal_p (clobber1, clobber2, OEP_ONLY_CONST))
	return false;
    }

  return true;
}

} // ipa_icf namespace
