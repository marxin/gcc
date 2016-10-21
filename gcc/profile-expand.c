/* Profile expand pass.
   Copyright (C) 2003-2016 Free Software Foundation, Inc.

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
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "memmodel.h"
#include "backend.h"
#include "target.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "tree-pass.h"
#include "ssa.h"
#include "coverage.h"
#include "varasm.h"
#include "tree-nested.h"
#include "gimplify.h"
#include "gimple-iterator.h"
#include "gimplify-me.h"
#include "tree-cfg.h"
#include "tree-into-ssa.h"
#include "value-prof.h"
#include "profile.h"
#include "tree-cfgcleanup.h"
#include "params.h"

void
expand_coverage_counter_ifns (void)
{
  basic_block bb;
  tree f = builtin_decl_explicit (LONG_LONG_TYPE_SIZE > 32
				  ? BUILT_IN_ATOMIC_FETCH_ADD_8:
				  BUILT_IN_ATOMIC_FETCH_ADD_4);

  FOR_EACH_BB_FN (bb, cfun)
    {
      gimple_stmt_iterator gsi;
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
	{
	  gimple *stmt = gsi_stmt (gsi);
	  if (gimple_call_internal_p (stmt, IFN_UPDATE_COVERAGE_COUNTER))
	    {
	      tree addr = gimple_call_arg (stmt, 0);
	      tree value = gimple_call_arg (stmt, 1);
	      if (flag_profile_update == PROFILE_UPDATE_ATOMIC)
		{
		  gcall *stmt
		    = gimple_build_call (f, 3, addr, value,
					 build_int_cst (integer_type_node,
							MEMMODEL_RELAXED));
		  gsi_replace (&gsi, stmt, true);
		}
	      else
		{
		  gcc_assert (TREE_CODE (addr) == ADDR_EXPR);
		  tree ref = TREE_OPERAND (addr, 0);
		  tree gcov_type_tmp_var
		    = make_temp_ssa_name (get_gcov_type (), NULL,
					  "PROF_edge_counter");
		  gassign *stmt1 = gimple_build_assign (gcov_type_tmp_var, ref);
		  gcov_type_tmp_var
		    = make_temp_ssa_name (get_gcov_type (), NULL,
					  "PROF_edge_counter");
		  gassign *stmt2
		    = gimple_build_assign (gcov_type_tmp_var, PLUS_EXPR,
					   gimple_assign_lhs (stmt1), value);
		  gassign *stmt3
		    = gimple_build_assign (unshare_expr (ref),
					   gimple_assign_lhs (stmt2));
		  gsi_insert_seq_before (&gsi, stmt1, GSI_SAME_STMT);
		  gsi_insert_seq_before (&gsi, stmt2, GSI_SAME_STMT);
		  gsi_replace (&gsi, stmt3, GSI_SAME_STMT);
		}
	    }
	}
    }
}

/* Profile expand pass.  */

namespace {

const pass_data pass_data_profile_expand =
{
  GIMPLE_PASS, /* type */
  "profile_expand", /* name */
  OPTGROUP_LOOP, /* optinfo_flags */
  TV_LIM, /* tv_id */
  PROP_cfg, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_update_ssa, /* todo_flags_finish */
};

class pass_profile_expand : public gimple_opt_pass
{
public:
  pass_profile_expand (gcc::context *ctxt)
    : gimple_opt_pass (pass_data_profile_expand, ctxt)
  {}

  /* opt_pass methods: */
  opt_pass * clone () { return new pass_profile_expand (m_ctxt); }
  virtual bool gate (function *) { return true; }
  virtual unsigned int execute (function *);

}; // class pass_profile_expand

unsigned int
pass_profile_expand::execute (function *)
{
  expand_coverage_counter_ifns ();

  return 0;
}

} // anon namespace

gimple_opt_pass *
make_pass_profile_expand (gcc::context *ctxt)
{
  return new pass_profile_expand (ctxt);
}


