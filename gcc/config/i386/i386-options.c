/* Subroutines used for code generation on IA-32.
   Copyright (C) 1988-2019 Free Software Foundation, Inc.

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

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "memmodel.h"
#include "gimple.h"
#include "cfghooks.h"
#include "cfgloop.h"
#include "df.h"
#include "tm_p.h"
#include "stringpool.h"
#include "expmed.h"
#include "optabs.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "cfgbuild.h"
#include "alias.h"
#include "fold-const.h"
#include "attribs.h"
#include "calls.h"
#include "stor-layout.h"
#include "varasm.h"
#include "output.h"
#include "insn-attr.h"
#include "flags.h"
#include "except.h"
#include "explow.h"
#include "expr.h"
#include "cfgrtl.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "reload.h"
#include "gimplify.h"
#include "dwarf2.h"
#include "tm-constrs.h"
#include "params.h"
#include "cselib.h"
#include "sched-int.h"
#include "opts.h"
#include "tree-pass.h"
#include "context.h"
#include "pass_manager.h"
#include "target-globals.h"
#include "gimple-iterator.h"
#include "tree-vectorizer.h"
#include "shrink-wrap.h"
#include "builtins.h"
#include "rtl-iter.h"
#include "tree-iterator.h"
#include "dbgcnt.h"
#include "case-cfn-macros.h"
#include "dojump.h"
#include "fold-const-call.h"
#include "tree-vrp.h"
#include "tree-ssanames.h"
#include "selftest.h"
#include "selftest-rtl.h"
#include "print-rtl.h"
#include "intl.h"
#include "ifcvt.h"
#include "symbol-summary.h"
#include "ipa-prop.h"
#include "ipa-fnsummary.h"
#include "wide-int-bitmask.h"
#include "tree-vector-builder.h"
#include "debug.h"
#include "dwarf2out.h"

/* Handle "cdecl", "stdcall", "fastcall", "regparm", "thiscall",
   and "sseregparm" calling convention attributes;
   arguments as in struct attribute_spec.handler.  */

static tree
ix86_handle_cconv_attribute (tree *node, tree name, tree args, int,
			     bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != METHOD_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
	       name);
      *no_add_attrs = true;
      return NULL_TREE;
    }

  /* Can combine regparm with all attributes but fastcall, and thiscall.  */
  if (is_attribute_p ("regparm", name))
    {
      tree cst;

      if (lookup_attribute ("fastcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and regparm attributes are not compatible");
	}

      if (lookup_attribute ("thiscall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("regparam and thiscall attributes are not compatible");
	}

      cst = TREE_VALUE (args);
      if (TREE_CODE (cst) != INTEGER_CST)
	{
	  warning (OPT_Wattributes,
		   "%qE attribute requires an integer constant argument",
		   name);
	  *no_add_attrs = true;
	}
      else if (compare_tree_int (cst, REGPARM_MAX) > 0)
	{
	  warning (OPT_Wattributes, "argument to %qE attribute larger than %d",
		   name, REGPARM_MAX);
	  *no_add_attrs = true;
	}

      return NULL_TREE;
    }

  if (TARGET_64BIT)
    {
      /* Do not warn when emulating the MS ABI.  */
      if ((TREE_CODE (*node) != FUNCTION_TYPE
	   && TREE_CODE (*node) != METHOD_TYPE)
	  || ix86_function_type_abi (*node) != MS_ABI)
	warning (OPT_Wattributes, "%qE attribute ignored",
	         name);
      *no_add_attrs = true;
      return NULL_TREE;
    }

  /* Can combine fastcall with stdcall (redundant) and sseregparm.  */
  if (is_attribute_p ("fastcall", name))
    {
      if (lookup_attribute ("cdecl", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and cdecl attributes are not compatible");
	}
      if (lookup_attribute ("stdcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and stdcall attributes are not compatible");
	}
      if (lookup_attribute ("regparm", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and regparm attributes are not compatible");
	}
      if (lookup_attribute ("thiscall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("fastcall and thiscall attributes are not compatible");
	}
    }

  /* Can combine stdcall with fastcall (redundant), regparm and
     sseregparm.  */
  else if (is_attribute_p ("stdcall", name))
    {
      if (lookup_attribute ("cdecl", TYPE_ATTRIBUTES (*node)))
        {
	  error ("stdcall and cdecl attributes are not compatible");
	}
      if (lookup_attribute ("fastcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("stdcall and fastcall attributes are not compatible");
	}
      if (lookup_attribute ("thiscall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("stdcall and thiscall attributes are not compatible");
	}
    }

  /* Can combine cdecl with regparm and sseregparm.  */
  else if (is_attribute_p ("cdecl", name))
    {
      if (lookup_attribute ("stdcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("stdcall and cdecl attributes are not compatible");
	}
      if (lookup_attribute ("fastcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and cdecl attributes are not compatible");
	}
      if (lookup_attribute ("thiscall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("cdecl and thiscall attributes are not compatible");
	}
    }
  else if (is_attribute_p ("thiscall", name))
    {
      if (TREE_CODE (*node) != METHOD_TYPE && pedantic)
	warning (OPT_Wattributes, "%qE attribute is used for non-class method",
	         name);
      if (lookup_attribute ("stdcall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("stdcall and thiscall attributes are not compatible");
	}
      if (lookup_attribute ("fastcall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("fastcall and thiscall attributes are not compatible");
	}
      if (lookup_attribute ("cdecl", TYPE_ATTRIBUTES (*node)))
	{
	  error ("cdecl and thiscall attributes are not compatible");
	}
    }

  /* Can combine sseregparm with all attributes.  */

  return NULL_TREE;
}

#ifndef CHECK_STACK_LIMIT
#define CHECK_STACK_LIMIT (-1)
#endif

/* The transactional memory builtins are implicitly regparm or fastcall
   depending on the ABI.  Override the generic do-nothing attribute that
   these builtins were declared with, and replace it with one of the two
   attributes that we expect elsewhere.  */

static tree
ix86_handle_tm_regparm_attribute (tree *node, tree, tree,
				  int flags, bool *no_add_attrs)
{
  tree alt;

  /* In no case do we want to add the placeholder attribute.  */
  *no_add_attrs = true;

  /* The 64-bit ABI is unchanged for transactional memory.  */
  if (TARGET_64BIT)
    return NULL_TREE;

  /* ??? Is there a better way to validate 32-bit windows?  We have
     cfun->machine->call_abi, but that seems to be set only for 64-bit.  */
  if (CHECK_STACK_LIMIT > 0)
    alt = tree_cons (get_identifier ("fastcall"), NULL, NULL);
  else
    {
      alt = tree_cons (NULL, build_int_cst (NULL, 2), NULL);
      alt = tree_cons (get_identifier ("regparm"), alt, NULL);
    }
  decl_attributes (node, alt, flags);

  return NULL_TREE;
}

/* Handle a "force_align_arg_pointer" attribute.  */

static tree
ix86_handle_force_align_arg_pointer_attribute (tree *node, tree name,
					       tree, int, bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != METHOD_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
	       name);
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle a "ms_struct" or "gcc_struct" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
ix86_handle_struct_attribute (tree *node, tree name, tree, int,
			      bool *no_add_attrs)
{
  tree *type = NULL;
  if (DECL_P (*node))
    {
      if (TREE_CODE (*node) == TYPE_DECL)
	type = &TREE_TYPE (*node);
    }
  else
    type = node;

  if (!(type && RECORD_OR_UNION_TYPE_P (*type)))
    {
      warning (OPT_Wattributes, "%qE attribute ignored",
	       name);
      *no_add_attrs = true;
    }

  else if ((is_attribute_p ("ms_struct", name)
	    && lookup_attribute ("gcc_struct", TYPE_ATTRIBUTES (*type)))
	   || ((is_attribute_p ("gcc_struct", name)
		&& lookup_attribute ("ms_struct", TYPE_ATTRIBUTES (*type)))))
    {
      warning (OPT_Wattributes, "%qE incompatible attribute ignored",
               name);
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle a "callee_pop_aggregate_return" attribute; arguments as
   in struct attribute_spec handler.  */

static tree
ix86_handle_callee_pop_aggregate_return (tree *node, tree name, tree args, int,
					 bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != METHOD_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
	       name);
      *no_add_attrs = true;
      return NULL_TREE;
    }
  if (TARGET_64BIT)
    {
      warning (OPT_Wattributes, "%qE attribute only available for 32-bit",
	       name);
      *no_add_attrs = true;
      return NULL_TREE;
    }
  if (is_attribute_p ("callee_pop_aggregate_return", name))
    {
      tree cst;

      cst = TREE_VALUE (args);
      if (TREE_CODE (cst) != INTEGER_CST)
	{
	  warning (OPT_Wattributes,
		   "%qE attribute requires an integer constant argument",
		   name);
	  *no_add_attrs = true;
	}
      else if (compare_tree_int (cst, 0) != 0
	       && compare_tree_int (cst, 1) != 0)
	{
	  warning (OPT_Wattributes,
		   "argument to %qE attribute is neither zero, nor one",
		   name);
	  *no_add_attrs = true;
	}

      return NULL_TREE;
    }

  return NULL_TREE;
}

/* Handle a "ms_abi" or "sysv" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
ix86_handle_abi_attribute (tree *node, tree name, tree, int,
			   bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != METHOD_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
	       name);
      *no_add_attrs = true;
      return NULL_TREE;
    }

  /* Can combine regparm with all attributes but fastcall.  */
  if (is_attribute_p ("ms_abi", name))
    {
      if (lookup_attribute ("sysv_abi", TYPE_ATTRIBUTES (*node)))
        {
	  error ("ms_abi and sysv_abi attributes are not compatible");
	}

      return NULL_TREE;
    }
  else if (is_attribute_p ("sysv_abi", name))
    {
      if (lookup_attribute ("ms_abi", TYPE_ATTRIBUTES (*node)))
        {
	  error ("ms_abi and sysv_abi attributes are not compatible");
	}

      return NULL_TREE;
    }

  return NULL_TREE;
}

static tree
ix86_handle_fndecl_attribute (tree *node, tree name, tree args, int,
			      bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
               name);
      *no_add_attrs = true;
    }

  if (is_attribute_p ("indirect_branch", name))
    {
      tree cst = TREE_VALUE (args);
      if (TREE_CODE (cst) != STRING_CST)
	{
	  warning (OPT_Wattributes,
		   "%qE attribute requires a string constant argument",
		   name);
	  *no_add_attrs = true;
	}
      else if (strcmp (TREE_STRING_POINTER (cst), "keep") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk-inline") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk-extern") != 0)
	{
	  warning (OPT_Wattributes,
		   "argument to %qE attribute is not "
		   "(keep|thunk|thunk-inline|thunk-extern)", name);
	  *no_add_attrs = true;
	}
    }

  if (is_attribute_p ("function_return", name))
    {
      tree cst = TREE_VALUE (args);
      if (TREE_CODE (cst) != STRING_CST)
	{
	  warning (OPT_Wattributes,
		   "%qE attribute requires a string constant argument",
		   name);
	  *no_add_attrs = true;
	}
      else if (strcmp (TREE_STRING_POINTER (cst), "keep") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk-inline") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk-extern") != 0)
	{
	  warning (OPT_Wattributes,
		   "argument to %qE attribute is not "
		   "(keep|thunk|thunk-inline|thunk-extern)", name);
	  *no_add_attrs = true;
	}
    }

  return NULL_TREE;
}

static tree
ix86_handle_no_caller_saved_registers_attribute (tree *, tree, tree,
						 int, bool *)
{
  return NULL_TREE;
}

static tree
ix86_handle_interrupt_attribute (tree *node, tree, tree, int, bool *)
{
  /* DECL_RESULT and DECL_ARGUMENTS do not exist there yet,
     but the function type contains args and return type data.  */
  tree func_type = *node;
  tree return_type = TREE_TYPE (func_type);

  int nargs = 0;
  tree current_arg_type = TYPE_ARG_TYPES (func_type);
  while (current_arg_type
	 && ! VOID_TYPE_P (TREE_VALUE (current_arg_type)))
    {
      if (nargs == 0)
	{
	  if (! POINTER_TYPE_P (TREE_VALUE (current_arg_type)))
	    error ("interrupt service routine should have a pointer "
		   "as the first argument");
	}
      else if (nargs == 1)
	{
	  if (TREE_CODE (TREE_VALUE (current_arg_type)) != INTEGER_TYPE
	      || TYPE_MODE (TREE_VALUE (current_arg_type)) != word_mode)
	    error ("interrupt service routine should have %qs "
		   "as the second argument",
		   TARGET_64BIT
		   ? (TARGET_X32 ? "unsigned long long int"
				 : "unsigned long int")
		   : "unsigned int");
	}
      nargs++;
      current_arg_type = TREE_CHAIN (current_arg_type);
    }
  if (!nargs || nargs > 2)
    error ("interrupt service routine can only have a pointer argument "
	   "and an optional integer argument");
  if (! VOID_TYPE_P (return_type))
    error ("interrupt service routine can%'t have non-void return value");

  return NULL_TREE;
}

/* Handle fentry_name / fentry_section attribute.  */

static tree
ix86_handle_fentry_name (tree *node, tree name, tree args,
			 int, bool *no_add_attrs)
{
  if (TREE_CODE (*node) == FUNCTION_DECL
      && TREE_CODE (TREE_VALUE (args)) == STRING_CST)
    /* Do nothing else, just set the attribute.  We'll get at
       it later with lookup_attribute.  */
    ;
  else
    {
      warning (OPT_Wattributes, "%qE attribute ignored", name);
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Table of valid machine attributes.  */
const struct attribute_spec ix86_attribute_table[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req,
       affects_type_identity, handler, exclude } */
  /* Stdcall attribute says callee is responsible for popping arguments
     if they are not variable.  */
  { "stdcall",   0, 0, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Fastcall attribute says callee is responsible for popping arguments
     if they are not variable.  */
  { "fastcall",  0, 0, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Thiscall attribute says callee is responsible for popping arguments
     if they are not variable.  */
  { "thiscall",  0, 0, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Cdecl attribute says the callee is a normal C declaration */
  { "cdecl",     0, 0, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Regparm attribute specifies how many integer arguments are to be
     passed in registers.  */
  { "regparm",   1, 1, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Sseregparm attribute says we are using x86_64 calling conventions
     for FP arguments.  */
  { "sseregparm", 0, 0, false, true, true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* The transactional memory builtins are implicitly regparm or fastcall
     depending on the ABI.  Override the generic do-nothing attribute that
     these builtins were declared with.  */
  { "*tm regparm", 0, 0, false, true, true, true,
    ix86_handle_tm_regparm_attribute, NULL },
  /* force_align_arg_pointer says this function realigns the stack at entry.  */
  { "force_align_arg_pointer", 0, 0,
    false, true,  true, false, ix86_handle_force_align_arg_pointer_attribute,
    NULL },
#if TARGET_DLLIMPORT_DECL_ATTRIBUTES
  { "dllimport", 0, 0, false, false, false, false, handle_dll_attribute,
    NULL },
  { "dllexport", 0, 0, false, false, false, false, handle_dll_attribute,
    NULL },
  { "shared",    0, 0, true,  false, false, false,
    ix86_handle_shared_attribute, NULL },
#endif
  { "ms_struct", 0, 0, false, false,  false, false,
    ix86_handle_struct_attribute, NULL },
  { "gcc_struct", 0, 0, false, false,  false, false,
    ix86_handle_struct_attribute, NULL },
#ifdef SUBTARGET_ATTRIBUTE_TABLE
  SUBTARGET_ATTRIBUTE_TABLE,
#endif
  /* ms_abi and sysv_abi calling convention function attributes.  */
  { "ms_abi", 0, 0, false, true, true, true, ix86_handle_abi_attribute, NULL },
  { "sysv_abi", 0, 0, false, true, true, true, ix86_handle_abi_attribute,
    NULL },
  { "ms_abi va_list", 0, 0, false, false, false, false, NULL, NULL },
  { "sysv_abi va_list", 0, 0, false, false, false, false, NULL, NULL },
  { "ms_hook_prologue", 0, 0, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },
  { "callee_pop_aggregate_return", 1, 1, false, true, true, true,
    ix86_handle_callee_pop_aggregate_return, NULL },
  { "interrupt", 0, 0, false, true, true, false,
    ix86_handle_interrupt_attribute, NULL },
  { "no_caller_saved_registers", 0, 0, false, true, true, false,
    ix86_handle_no_caller_saved_registers_attribute, NULL },
  { "naked", 0, 0, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },
  { "indirect_branch", 1, 1, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },
  { "function_return", 1, 1, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },
  { "indirect_return", 0, 0, false, true, true, false,
    NULL, NULL },
  { "fentry_name", 1, 1, true, false, false, false,
    ix86_handle_fentry_name, NULL },
  { "fentry_section", 1, 1, true, false, false, false,
    ix86_handle_fentry_name, NULL },
  { "cf_check", 0, 0, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },

  /* End element.  */
  { NULL, 0, 0, false, false, false, false, NULL, NULL }
};
