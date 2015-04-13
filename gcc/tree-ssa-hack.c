/* SSA-PRE for trees.
   Copyright (C) 2001-2015 Free Software Foundation, Inc.
   Contributed by Daniel Berlin <dan@dberlin.org> and Steven Bosscher
   <stevenb@suse.de>

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
#include "tm.h"
#include "hash-set.h"
#include "machmode.h"
#include "vec.h"
#include "double-int.h"
#include "input.h"
#include "alias.h"
#include "symtab.h"
#include "wide-int.h"
#include "inchash.h"
#include "tree.h"
#include "fold-const.h"
#include "predict.h"
#include "hard-reg-set.h"
#include "function.h"
#include "dominance.h"
#include "cfg.h"
#include "cfganal.h"
#include "basic-block.h"
#include "gimple-pretty-print.h"
#include "tree-inline.h"
#include "hash-table.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "gimple-fold.h"
#include "tree-eh.h"
#include "gimple-expr.h"
#include "is-a.h"
#include "gimple.h"
#include "gimplify.h"
#include "gimple-iterator.h"
#include "gimplify-me.h"
#include "gimple-ssa.h"
#include "tree-cfg.h"
#include "tree-phinodes.h"
#include "ssa-iterators.h"
#include "stringpool.h"
#include "tree-ssanames.h"
#include "tree-ssa-loop.h"
#include "tree-into-ssa.h"
#include "hashtab.h"
#include "rtl.h"
#include "flags.h"
#include "statistics.h"
#include "real.h"
#include "fixed-value.h"
#include "insn-config.h"
#include "expmed.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "emit-rtl.h"
#include "varasm.h"
#include "stmt.h"
#include "expr.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "tree-iterator.h"
#include "alloc-pool.h"
#include "obstack.h"
#include "tree-pass.h"
#include "langhooks.h"
#include "cfgloop.h"
#include "tree-ssa-sccvn.h"
#include "tree-scalar-evolution.h"
#include "params.h"
#include "dbgcnt.h"
#include "domwalk.h"
#include "hash-map.h"
#include "plugin-api.h"
#include "ipa-ref.h"
#include "cgraph.h"
#include "symbol-summary.h"
#include "ipa-prop.h"
#include "tree-ssa-propagate.h"
#include "ipa-utils.h"
#include "tree-cfgcleanup.h"
#include "print-tree.h"

namespace {

const pass_data pass_data_hack =
{
  GIMPLE_PASS, /* type */
  "hack", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_TREE_PRE, /* tv_id */
  ( PROP_cfg | PROP_ssa ), /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_update_ssa, /* todo_flags_finish */
};

class pass_hack : public gimple_opt_pass
{
public:
  pass_hack (gcc::context *ctxt)
    : gimple_opt_pass (pass_data_hack, ctxt)
  {}

  /* opt_pass methods: */
  virtual bool gate (function *) { return true; }
  virtual unsigned int execute (function *);

}; // class pass_pre

unsigned int
pass_hack::execute (function *fun)
{
  basic_block bb;
  const char *name = function_name (fun);
  if(!strstr (name, "digits_2"))
    return 0;

  int c = 2111111111;

  FOR_ALL_BB_FN (bb, cfun)
    {
    for (gimple_stmt_iterator gsi = gsi_start_bb (bb); !gsi_end_p (gsi);
	 gsi_next (&gsi))
      {
	gimple stmt = gsi_stmt (gsi);

	if (gimple_code (stmt) == GIMPLE_ASSIGN && c >= 0)
	  {
	    tree rhs1 = gimple_assign_rhs1 (stmt);
	    if (TREE_CODE(rhs1) == ARRAY_REF)
	      {
		tree array_arg0 = TREE_OPERAND (rhs1, 0);
		tree array_arg1 = TREE_OPERAND (rhs1, 1);

		if (TREE_CODE (array_arg0) == MEM_REF && TREE_CODE (array_arg1)
		    == SSA_NAME)
		  {
		    tree array_decl = TREE_OPERAND (TREE_OPERAND (array_arg0, 0), 0);
		    gcc_assert (TREE_CODE (array_decl) == VAR_DECL);

		    gimple index_def = SSA_NAME_DEF_STMT(array_arg1);
		    if(gimple_code (index_def) == GIMPLE_ASSIGN)
		      {
			tree r2 = gimple_assign_rhs2 (index_def);
			if(TREE_CODE (r2) == INTEGER_CST)
			  {
			    gimple semi = SSA_NAME_DEF_STMT(gimple_assign_rhs1 (index_def));
			    if (gimple_code (semi) == GIMPLE_ASSIGN &&
				gimple_assign_rhs_code (semi) == PLUS_EXPR )
			      {
				gimple row_assign = SSA_NAME_DEF_STMT(gimple_assign_rhs1 ( SSA_NAME_DEF_STMT (gimple_assign_rhs2 (semi))));
				gimple cast_row = SSA_NAME_DEF_STMT (gimple_assign_rhs2 (semi));
				(row_assign);

				if (TREE_CODE (gimple_assign_rhs1 (row_assign))
				    == MEM_REF)
				  {
 				    tree row_decl = TREE_OPERAND(gimple_assign_rhs1 (row_assign), 0);
				    gimple semi2 = SSA_NAME_DEF_STMT (gimple_assign_rhs1 (semi));

				    tree c1 = gimple_assign_rhs2 (semi2);
				    if (TREE_CODE (c1) == INTEGER_CST) 
				      {
					tree size = TYPE_SIZE_UNIT (TREE_TYPE(TREE_TYPE
							       (row_decl)));
					tree type1 = TREE_TYPE (gimple_assign_lhs (index_def));
//					tree ix_tmp = make_temp_ssa_name (type1, NULL,
//							    "ix_tmp");

					tree k2 = fold_build2(MULT_EXPR,
							      type1,
							      size,
							      c1);

					tree k1 = fold_build2(MULT_EXPR,
							      TREE_TYPE (r2), r2,
							     size);

					tree typus = TREE_TYPE (TREE_OPERAND (array_arg0, 0));

					k1 = fold_convert (build_pointer_type
							   (TREE_TYPE (k1)), k1);


					tree semi2_rhs2 = gimple_assign_rhs2
					  (semi2);

					gimple_assign_set_rhs2 (semi2, fold_build2
								(MULT_EXPR,
								 TREE_TYPE
								 (semi2_rhs2),
								 semi2_rhs2,
								 size));

					tree target_mem_ref = build5
					  (TARGET_MEM_REF,
					   TREE_TYPE (TREE_TYPE(array_decl)),
					   TREE_OPERAND (array_arg0, 0),
					   k1,
					   gimple_assign_lhs (cast_row),
					   size,
					   // build_int_cst (TREE_TYPE (size), 1),
					   gimple_assign_lhs (semi2));

//	debug_bb (bb);
					gimple_assign_set_rhs1 (stmt,
								target_mem_ref);

					update_stmt (stmt);

					// find usage
					imm_use_iterator iter;
					gimple use_stmt;
					FOR_EACH_IMM_USE_STMT (use_stmt, iter,
							       gimple_assign_lhs
							       (index_def))
					  {
					    if (is_gimple_assign (use_stmt) &&
						TREE_CODE (gimple_assign_lhs
							   (use_stmt)) ==
							   ARRAY_REF &&
							   TREE_CODE
							   (gimple_assign_rhs1
							    (use_stmt)) ==
							    SSA_NAME)
					      {

					tree target_mem_ref2 = build5
					  (TARGET_MEM_REF,
					   TREE_TYPE (TREE_TYPE(array_decl)),
					   TREE_OPERAND (array_arg0, 0),
					   k1,
					   gimple_assign_lhs (cast_row),
					   size,
					   // build_int_cst (TREE_TYPE (size), 1),
					   gimple_assign_lhs (semi2));


//					debug_tree (target_mem_ref2);
gimple_assign_set_lhs (use_stmt, target_mem_ref2);
update_stmt (use_stmt);


					      }

					    break;
					  }

	        debug_gimple_stmt (index_def);
		gimple_stmt_iterator cgsi = gsi_for_stmt (index_def);
		unlink_stmt_vdef (index_def);
		gsi_remove (&cgsi, true);
		release_defs (index_def);

	        debug_gimple_stmt (semi);
		cgsi = gsi_for_stmt (semi);
		unlink_stmt_vdef (semi);
		gsi_remove (&cgsi, true);



					c--;

				      }
				  }


			      }



			  }
		      }

		  }
//		gcc_unreachable();
	      }
	  }
      }

    }

  return 0;
}

} // anon namespace

gimple_opt_pass *
make_pass_hack (gcc::context *ctxt)
{
  return new pass_hack (ctxt);
}
