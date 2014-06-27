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

/* Interprocedural Identical Code Folding for functions and
   read-only variables.

   The goal of this transformation is to discover functions and read-only
   variables which do have exactly the same semantics.

   In case of functions,
   we could either create a virtual clone or do a simple function wrapper
   that will call equivalent function. If the function is just locally visible,
   all function calls can be redirected. For read-only variables, we create
   aliases if possible.

   Optimization pass arranges as follows:
   1) All functions and read-only variables are visited and internal
      data structure, either sem_function or sem_variables is created.
   2) For every symbol from the previous step, VAR_DECL and FUNCTION_DECL are
      saved and matched to corresponding sem_items.
   3) These declaration are ignored for equality check and are solved
      by Value Numbering algorithm published by Alpert, Zadeck in 1992.
   4) We compute hash value for each symbol.
   5) Congruence classes are created based on hash value. If hash value are
      equal, equals function is called and symbols are deeply compared.
      We must prove that all SSA names, declarations and other items
      correspond.
   6) Value Numbering is executed for these classes. At the end of the process
      all symbol members in remaining classes can be merged.
   7) Merge operation creates alias in case of read-only variables. For
      callgraph node, we must decide if we can redirect local calls,
      create an alias or a thunk.

*/

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
#include "tree-phinodes.h"
#include "stringpool.h"
#include "tree-ssanames.h"
#include "tree-dfa.h"
#include "tree-pass.h"
#include "gimple-pretty-print.h"
#include "ipa-inline.h"
#include "cfgloop.h"
#include "except.h"
#include "hash-table.h"
#include "coverage.h"
#include "pointer-set.h"
#include "attribs.h"
#include "print-tree.h"
#include "lto-streamer.h"
#include "data-streamer.h"
#include "ipa-utils.h"
#include "ipa-icf.h"

namespace ipa_icf {

/* Itializes internal structures according to given number of
   source and target SSA names. The number of source names is SSA_SOURCE,
   respectively SSA_TARGET.  */

func_checker::func_checker (unsigned ssa_source, unsigned ssa_target)
{
  m_source_ssa_names.create (ssa_source);
  m_target_ssa_names.create (ssa_target);

  for (unsigned int i = 0; i < ssa_source; i++)
    m_source_ssa_names.safe_push (-1);

  for (unsigned int i = 0; i < ssa_target; i++)
    m_target_ssa_names.safe_push (-1);

  m_edge_map = new pointer_map <edge> ();
  m_decl_map = new pointer_map <tree> ();
}

/* Memory release routine.  */

func_checker::~func_checker ()
{
  delete m_edge_map;
  delete m_decl_map;
  m_source_ssa_names.release();
  m_target_ssa_names.release();
}

/* Verifies that trees T1 and T2 are equivalent from perspective of ICF.  */

bool
func_checker::compare_ssa_name (tree t1, tree t2)
{
  unsigned i1 = SSA_NAME_VERSION (t1);
  unsigned i2 = SSA_NAME_VERSION (t2);

  if (m_source_ssa_names[i1] == -1)
    m_source_ssa_names[i1] = i2;
  else if (m_source_ssa_names[i1] != (int) i2)
    return false;

  if(m_target_ssa_names[i2] == -1)
    m_target_ssa_names[i2] = i1;
  else if (m_target_ssa_names[i2] != (int) i1)
    return false;

  return true;
}

/* Verification function for edges E1 and E2.  */

bool
func_checker::compare_edge (edge e1, edge e2)
{
  if (e1->flags != e2->flags)
    return false;

  bool existed_p;

  edge *slot = m_edge_map->insert (e1, &existed_p);
  if (existed_p)
    {
      SE_EXIT_DEBUG (*slot == e2);
    }
  else
    *slot = e2;

  return true;
}

/* Verification function for declaration trees T1 and T2 that
   come from functions FUNC1 and FUNC2.  */

bool
func_checker::compare_decl (tree t1, tree t2, tree func1, tree func2)
{
  if (!auto_var_in_fn_p (t1, func1) || !auto_var_in_fn_p (t2, func2))
    SE_EXIT_DEBUG (t1 == t2);

  if (!types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2)))
    SE_EXIT_FALSE ();

  bool existed_p;

  tree *slot = m_decl_map->insert (t1, &existed_p);
  if (existed_p)
    {
      SE_EXIT_DEBUG (*slot == t2);
    }
  else
    *slot = t2;

  return true;
}

/* Constructor for key value pair, where _ITEM is key and _INDEX is a target.  */

sem_usage_pair::sem_usage_pair (sem_item *_item, unsigned int _index):
  item (_item), index (_index)
{
}

/* Semantic item constructor for a node of _TYPE, where STACK is used
   for bitmap memory allocation.  */

sem_item::sem_item (enum sem_item_type _type,
		    bitmap_obstack *stack): type(_type), hash(0)
{
  setup (stack);
}

/* Semantic item constructor for a node of _TYPE, where STACK is used
   for bitmap memory allocation. The item is based on symtab node _NODE
   with computed _HASH.  */

sem_item::sem_item (enum sem_item_type _type, struct symtab_node *_node,
		    hashval_t _hash, bitmap_obstack *stack): type(_type),
  node (_node), hash (_hash)
{
  decl = node->decl;
  setup (stack);
}

/* Initialize internal data structures. Bitmap STACK is used for
   bitmap memory allocation process.  */

void
sem_item::setup (bitmap_obstack *stack)
{
  gcc_checking_assert (node);

  refs.create (0);
  tree_refs.create (0);
  usages.create (0);
  tree_refs_set = pointer_set_create ();
  usage_index_bitmap = BITMAP_ALLOC (stack);
}

sem_item::~sem_item ()
{
  if (tree_refs_set)
    pointer_set_destroy (tree_refs_set);

  for (unsigned i = 0; i < usages.length (); i++)
    delete usages[i];

  refs.release ();
  tree_refs.release ();
  usages.release ();

  BITMAP_FREE (usage_index_bitmap);
}

/* Dump function for debugging purpose.  */

DEBUG_FUNCTION void
sem_item::dump (void)
{
  if (dump_file)
    {
      fprintf (dump_file, "[%s] %s (%u) (tree:%p)\n", type == FUNC ? "func" : "var",
	       name(), node->order, (void *) node->decl);
      fprintf (dump_file, "  hash: %u\n", get_hash ());
      fprintf (dump_file, "  references: ");

      for (unsigned i = 0; i < refs.length (); i++)
	fprintf (dump_file, "%s%s ", refs[i]->name (),
		 i < refs.length() - 1 ? "," : "");

      fprintf (dump_file, "\n");
    }
}

/* If strict aliasing is enabled, function compares if given types are
   in the same alias set.  */
bool
sem_item::compare_for_aliasing (tree t1, tree t2)
{
  if (flag_strict_aliasing)
    {
      alias_set_type s1 = get_deref_alias_set (TREE_TYPE (t1));
      alias_set_type s2 = get_deref_alias_set (TREE_TYPE (t2));

      return s1 == s2;
    }

  return true;
}

/* Semantic function constructor that uses STACK as bitmap memory stack.  */

sem_function::sem_function (bitmap_obstack *stack): sem_item (FUNC, stack),
  m_checker (NULL), m_compared_func (NULL)
{
  arg_types.create (0);
  bb_sizes.create (0);
  bb_sorted.create (0);
}

/*  Constructor based on callgraph node _NODE with computed hash _HASH.
    Bitmap STACK is used for memory allocation.  */
sem_function::sem_function (cgraph_node *node, hashval_t hash,
			    bitmap_obstack *stack):
  sem_item (FUNC, node, hash, stack),
  m_checker (NULL), m_compared_func (NULL)
{
  arg_types.create (0);
  bb_sizes.create (0);
  bb_sorted.create (0);
}

sem_function::~sem_function ()
{
  for (unsigned i = 0; i < bb_sorted.length (); i++)
    free (bb_sorted[i]);

  arg_types.release ();
  bb_sizes.release ();
  bb_sorted.release ();
}

/* Calculates hash value based on a BASIC_BLOCK.  */

hashval_t
sem_function::get_bb_hash (const sem_bb_t *basic_block)
{
  hashval_t hash = basic_block->nondbg_stmt_count;
  hash = iterative_hash_object (basic_block->edge_count, hash);

  return hash;
}

/* References independent hash function.  */

hashval_t
sem_function::get_hash (void)
{
  if(!hash)
    {
      hash = 177454; /* Random number for function type.  */

      hash = iterative_hash_object (arg_count, hash);
      hash = iterative_hash_object (cfg_checksum, hash);
      hash = iterative_hash_object (gcode_hash, hash);

      for (unsigned i = 0; i < bb_sorted.length (); i++)
	hash = iterative_hash_object (hash, get_bb_hash (bb_sorted[i]));

      for (unsigned i = 0; i < bb_sizes.length (); i++)
	hash = iterative_hash_object (bb_sizes[i], hash);
    }

  return hash;
}

/* Fast equality function based on knowledge known in WPA.  */

bool
sem_function::equals_wpa (sem_item *item)
{
  gcc_assert (item->type == FUNC);

  m_compared_func = static_cast<sem_function *> (item);

  if (arg_types.length () != m_compared_func->arg_types.length ())
    SE_EXIT_FALSE_WITH_MSG ("different number of arguments");

  /* Checking types of arguments.  */
  for (unsigned i = 0; i < arg_types.length (); i++)
    {
      /* This guard is here for function pointer with attributes (pr59927.c).  */
      if (!arg_types[i] || !m_compared_func->arg_types[i])
	SE_EXIT_FALSE_WITH_MSG ("NULL arg type");

      if (!types_compatible_p (arg_types[i], m_compared_func->arg_types[i]))
	SE_EXIT_FALSE_WITH_MSG("argument type is different");
    }

  /* Result type checking.  */
  if (!types_compatible_p (result_type, m_compared_func->result_type))
    SE_EXIT_FALSE_WITH_MSG("result types are different");

  return true;
}

/* Returns true if the item equals to ITEM given as argument.  */

bool
sem_function::equals (sem_item *item)
{
  gcc_assert (item->type == FUNC);
  bool eq = equals_private (item);

  if (m_checker != NULL)
    {
      delete m_checker;
      m_checker = NULL;
    }

  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file,
	     "Equals called for:%s:%s (%u:%u) (%s:%s) with result: %s\n\n",
	     name(), item->name (), node->order, item->node->order, asm_name (),
	     item->asm_name (), eq ? "true" : "false");

  return eq;
}

/* Processes function equality comparison.  */

bool
sem_function::equals_private (sem_item *item)
{
  if (item->type != FUNC)
    return false;

  basic_block bb1, bb2;
  edge e1, e2;
  edge_iterator ei1, ei2;
  int *bb_dict = NULL;
  bool result = true;
  tree arg1, arg2;

  m_compared_func = static_cast<sem_function *> (item);

  gcc_assert (decl != item->decl);

  if (bb_sorted.length () != m_compared_func->bb_sorted.length ()
      || edge_count != m_compared_func->edge_count
      || cfg_checksum != m_compared_func->cfg_checksum)
    SE_EXIT_FALSE();

  if (!equals_wpa (item))
    return false;

  /* Checking function arguments.  */
  tree decl1 = DECL_ATTRIBUTES (decl);
  tree decl2 = DECL_ATTRIBUTES (m_compared_func->decl);

  while (decl1)
    {
      if (decl2 == NULL)
	SE_EXIT_FALSE();

      if (get_attribute_name (decl1) != get_attribute_name (decl2))
	SE_EXIT_FALSE();

      tree attr_value1 = TREE_VALUE (decl1);
      tree attr_value2 = TREE_VALUE (decl2);

      if (attr_value1 && attr_value2)
	{
	  bool ret = compare_operand (TREE_VALUE (attr_value1),
				      TREE_VALUE (attr_value2), decl,
				      m_compared_func->decl);
	  if (!ret)
	    SE_EXIT_FALSE_WITH_MSG ("attribute values are different")
	  }
      else if (!attr_value1 && !attr_value2)
	{}
      else
	SE_EXIT_FALSE ();

      decl1 = TREE_CHAIN (decl1);
      decl2 = TREE_CHAIN (decl2);
    }

  if (decl1 != decl2)
    SE_EXIT_FALSE();

  m_checker = new func_checker (ssa_names_size, m_compared_func->ssa_names_size);

  for (arg1 = DECL_ARGUMENTS (decl),
       arg2 = DECL_ARGUMENTS (m_compared_func->decl);
       arg1; arg1 = DECL_CHAIN (arg1), arg2 = DECL_CHAIN (arg2))
    m_checker->compare_decl (arg1, arg2, decl, m_compared_func->decl);

  /* Exception handling regions comparison.  */
  if (!compare_eh_region (region_tree, m_compared_func->region_tree, decl,
			  m_compared_func->decl))
    SE_EXIT_FALSE();

  /* Checking all basic blocks.  */
  for (unsigned i = 0; i < bb_sorted.length (); ++i)
    if(!compare_bb (bb_sorted[i], m_compared_func->bb_sorted[i], decl,
		    m_compared_func->decl))
      SE_EXIT_FALSE();

  SE_DUMP_MESSAGE ("All BBs are equal\n");

  /* Basic block edges check.  */
  for (unsigned i = 0; i < bb_sorted.length (); ++i)
    {
      bb_dict = XNEWVEC (int, bb_sorted.length () + 2);
      memset (bb_dict, -1, (bb_sorted.length () + 2) * sizeof (int));

      bb1 = bb_sorted[i]->bb;
      bb2 = m_compared_func->bb_sorted[i]->bb;

      ei2 = ei_start (bb2->preds);

      for (ei1 = ei_start (bb1->preds); ei_cond (ei1, &e1); ei_next (&ei1))
	{
	  ei_cond (ei2, &e2);

	  if (e1->flags != e2->flags)
	    SE_EXIT_FALSE_WITH_MSG("flags comparison returns false");

	  if (!bb_dict_test (bb_dict, e1->src->index, e2->src->index))
	    SE_EXIT_FALSE_WITH_MSG("edge comparison returns false");

	  if (!bb_dict_test (bb_dict, e1->dest->index, e2->dest->index))
	    SE_EXIT_FALSE_WITH_MSG("BB comparison returns false");

	  if (!m_checker->compare_edge (e1, e2))
	    SE_EXIT_FALSE_WITH_MSG("edge comparison returns false");

	  ei_next (&ei2);
	}
    }

  /* Basic block PHI nodes comparison.  */
  for (unsigned i = 0; i < bb_sorted.length (); i++)
    if (!compare_phi_node (bb_sorted[i]->bb, m_compared_func->bb_sorted[i]->bb,
			   decl, m_compared_func->decl))
      SE_EXIT_FALSE_WITH_MSG ("PHI node comparison returns false");

  return result;
}

/* Initializes references to another sem_item for tree T.  */

void
sem_function::init_refs_for_tree (tree t)
{
  switch (TREE_CODE (t))
    {
    case VAR_DECL:
    case FUNCTION_DECL:
      tree_refs.safe_push (t);
      break;
    case MEM_REF:
    case ADDR_EXPR:
    case OBJ_TYPE_REF:
      init_refs_for_tree (TREE_OPERAND (t, 0));
      break;
    case FIELD_DECL:
      init_refs_for_tree (DECL_FCONTEXT (t));
      break;
    default:
      break;
    }
}

/* Initializes references to another sem_item for gimple STMT of type assign.  */

void
sem_function::init_refs_for_assign (gimple stmt)
{
  if (gimple_num_ops (stmt) != 2)
    return;

  tree rhs = gimple_op (stmt, 1);

  init_refs_for_tree (rhs);
}

/* Initializes references to other semantic functions/variables.  */

void
sem_function::init_refs (void)
{
  for (unsigned i = 0; i < bb_sorted.length (); i++)
    {
      basic_block bb = bb_sorted[i]->bb;

      for (gimple_stmt_iterator gsi = gsi_start_bb (bb); !gsi_end_p (gsi);
	   gsi_next (&gsi))
	{
	  gimple stmt = gsi_stmt (gsi);
	  hashval_t code = (hashval_t) gimple_code (stmt);

	  switch (code)
	    {
	    case GIMPLE_CALL:
	      {
		tree funcdecl = gimple_call_fndecl (stmt);

		/* Function pointer variables are not supported yet.  */
		if (funcdecl)
		  tree_refs.safe_push (funcdecl);

		break;
	      }
	    case GIMPLE_ASSIGN:
	      init_refs_for_assign (stmt);
	      break;
	    default:
	      break;
	    }
	}
    }
}

/* Merges instance with an ALIAS_ITEM, where alias, thunk or redirection can
   be applied.  */
bool
sem_function::merge (sem_item *alias_item)
{
  gcc_assert (alias_item->type == FUNC);

  sem_function *alias_func = static_cast<sem_function *> (alias_item);

  struct cgraph_node *original = get_node ();
  struct cgraph_node *local_original = original;
  struct cgraph_node *alias = alias_func->get_node ();
  bool original_address_matters;
  bool alias_address_matters;

  bool create_thunk = false;
  bool create_alias = false;
  bool redirect_callers = false;
  bool original_discardable = false;

  /* Do not attempt to mix functions from different user sections;
     we do not know what user intends with those.  */
  if (((DECL_SECTION_NAME (original->decl) && !original->implicit_section)
       || (DECL_SECTION_NAME (alias->decl) && !alias->implicit_section))
      && DECL_SECTION_NAME (original->decl) != DECL_SECTION_NAME (alias->decl))
    {
      if (dump_file)
	fprintf (dump_file,
		 "Not unifying; original and alias are in different sections.\n\n");
      return false;
    }

  /* See if original is in a section that can be discarded if the main
     symbol is not used.  */
  if (DECL_EXTERNAL (original->decl))
    original_discardable = true;
  if (original->resolution == LDPR_PREEMPTED_REG
      || original->resolution == LDPR_PREEMPTED_IR)
    original_discardable = true;
  if (symtab_can_be_discarded (original))
    original_discardable = true;

  /* See if original and/or alias address can be compared for equality.  */
  original_address_matters
    = (!DECL_VIRTUAL_P (original->decl)
       && (original->externally_visible
	   || address_taken_from_non_vtable_p (original)));
  alias_address_matters
    = (!DECL_VIRTUAL_P (alias->decl)
       && (alias->externally_visible
	   || address_taken_from_non_vtable_p (alias)));

  /* If alias and original can be compared for address equality, we need
     to create a thunk.  Also we can not create extra aliases into discardable
     section (or we risk link failures when section is discarded).  */
  if ((original_address_matters
       && alias_address_matters)
      || original_discardable)
    {
      create_thunk = !stdarg_p (TREE_TYPE (alias->decl));
      create_alias = false;
      /* When both alias and original are not overwritable, we can save
         the extra thunk wrapper for direct calls.  */
      redirect_callers
	= (!original_discardable
	   && cgraph_function_body_availability (alias) > AVAIL_OVERWRITABLE
	   && cgraph_function_body_availability (original) > AVAIL_OVERWRITABLE);
    }
  else
    {
      create_alias = true;
      create_thunk = false;
      redirect_callers = false;
    }

  if (create_alias && DECL_COMDAT_GROUP (alias->decl))
    {
      create_alias = false;
      create_thunk = true;
    }

  /* We want thunk to always jump to the local function body
     unless the body is comdat and may be optimized out.  */
  if ((create_thunk || redirect_callers)
      && (!original_discardable
	  || (DECL_COMDAT_GROUP (original->decl)
	      && (DECL_COMDAT_GROUP (original->decl)
		  == DECL_COMDAT_GROUP (alias->decl)))))
    local_original
      = cgraph (symtab_nonoverwritable_alias (original));

  if (redirect_callers)
    {
      /* If alias is non-overwritable then
         all direct calls are safe to be redirected to the original.  */
      bool redirected = false;
      while (alias->callers)
	{
	  struct cgraph_edge *e = alias->callers;
	  cgraph_redirect_edge_callee (e, local_original);
	  push_cfun (DECL_STRUCT_FUNCTION (e->caller->decl));

	  if (e->call_stmt)
	    cgraph_redirect_edge_call_stmt_to_callee (e);

	  pop_cfun ();
	  redirected = true;
	}

      /* The alias function is removed if symbol address
         does not matter.  */
      if (!alias_address_matters)
	cgraph_remove_node (alias);

      if (dump_file && redirected)
	fprintf (dump_file, "Callgraph local calls have been redirected.\n\n");
    }
  /* If the condtion above is not met, we are lucky and can turn the
     function into real alias.  */
  else if (create_alias)
    {
      /* Remove the function's body.  */
      ipa_merge_profiles (original, alias);
      cgraph_release_function_body (alias);
      cgraph_reset_node (alias);

      /* Create the alias.  */
      cgraph_create_function_alias (alias_func->decl, decl);
      symtab_resolve_alias (alias, original);

      if (dump_file)
	fprintf (dump_file, "Callgraph alias has been created.\n\n");
    }
  else if (create_thunk)
    {
      if (DECL_COMDAT_GROUP (alias->decl))
	{
	  if (dump_file)
	    fprintf (dump_file, "Callgraph thunk cannot be created because of COMDAT\n");

	  return 0;
	}

      ipa_merge_profiles (local_original, alias);
      cgraph_make_wrapper (alias, local_original);

      if (dump_file)
	fprintf (dump_file, "Callgraph thunk has been created.\n\n");
    }
  else if (dump_file)
    fprintf (dump_file, "Callgraph merge operation cannot be performed.\n\n");

  return true;
}

/* Semantic item initialization function.  */

void
sem_function::init (void)
{
  if (in_lto_p)
    cgraph_get_body (get_node ());

  tree fndecl = node->decl;
  struct function *func = DECL_STRUCT_FUNCTION (fndecl);

  gcc_assert (func);
  gcc_assert (SSANAMES (func));

  ssa_names_size = SSANAMES (func)->length ();
  node = node;

  decl = fndecl;
  region_tree = func->eh->region_tree;

  /* iterating all function arguments.  */
  arg_count = count_formal_params (fndecl);

  edge_count = n_edges_for_fn (func);
  cfg_checksum = coverage_compute_cfg_checksum (func);

  gcode_hash = 0;

  basic_block bb;
  FOR_EACH_BB_FN (bb, func)
  {
    unsigned nondbg_stmt_count = 0;

    edge e;
    for (edge_iterator ei = ei_start (bb->preds); ei_cond (ei, &e); ei_next (&ei))
      cfg_checksum = iterative_hash_host_wide_int (e->flags,
		     cfg_checksum);

    for (gimple_stmt_iterator gsi = gsi_start_bb (bb); !gsi_end_p (gsi);
	 gsi_next (&gsi))
      {
	gimple stmt = gsi_stmt (gsi);
	hashval_t code = (hashval_t) gimple_code (stmt);

	/* We ignore all debug statements.  */
	if (code != GIMPLE_DEBUG)
	  {
	    nondbg_stmt_count++;
	    gcode_hash = iterative_hash_object (code, gcode_hash);
	  }
      }

    bb_sizes.safe_push (nondbg_stmt_count);

    /* Inserting basic block to hash table.  */
    sem_bb_t *sem_bb = XNEW (sem_bb_t);
    sem_bb->bb = bb;
    sem_bb->nondbg_stmt_count = nondbg_stmt_count;
    sem_bb->edge_count = EDGE_COUNT (bb->preds) + EDGE_COUNT (bb->succs);

    bb_sorted.safe_push (sem_bb);
  }

  parse_tree_args ();
}

/* For a given call graph NODE, the function constructs new
   semantic function item.  */

sem_function *
sem_function::parse (struct cgraph_node *node, bitmap_obstack *stack)
{
  tree fndecl = node->decl;
  struct function *func = DECL_STRUCT_FUNCTION (fndecl);

  if (!func || !cgraph_function_with_gimple_body_p (node))
    return NULL;

  if (lookup_attribute_by_prefix ("omp ", DECL_ATTRIBUTES (node->decl)) != NULL)
    return NULL;

  sem_function *f = new sem_function (node, 0, stack);

  f->init ();

  return f;
}

/* Parses function arguments and result type.  */

void
sem_function::parse_tree_args (void)
{
  tree result;

  if (arg_types.exists ())
    arg_types.release ();

  arg_types.create (4);
  tree fnargs = DECL_ARGUMENTS (decl);

  for (tree parm = fnargs; parm; parm = DECL_CHAIN (parm))
    arg_types.safe_push (TYPE_CANONICAL (DECL_ARG_TYPE (parm)));

  /* Function result type.  */
  result = DECL_RESULT (decl);
  result_type = result ? TYPE_CANONICAL (TREE_TYPE (result)) : NULL;

  /* During WPA, we can get arguments by following method.  */
  if (!fnargs)
    {
      tree type = TYPE_ARG_TYPES (TREE_TYPE (decl));
      for (tree parm = type; parm; parm = TREE_CHAIN (parm))
	arg_types.safe_push (TYPE_CANONICAL (TREE_VALUE (parm)));

      result_type = TREE_TYPE (TREE_TYPE (decl));
    }
}

/* For given basic blocks BB1 and BB2 (from functions FUNC1 and FUNC),
   return true if phi nodes are sematically equivalent in these blocks .  */

bool
sem_function::compare_phi_node (basic_block bb1, basic_block bb2,
				tree func1, tree func2)
{
  gimple_stmt_iterator si1, si2;
  gimple phi1, phi2;
  unsigned size1, size2, i;
  tree t1, t2;
  edge e1, e2;

  gcc_assert (bb1 != NULL);
  gcc_assert (bb2 != NULL);

  si2 = gsi_start_phis (bb2);
  for (si1 = gsi_start_phis (bb1); !gsi_end_p (si1);
       gsi_next (&si1))
    {
      gsi_next_nonvirtual_phi (&si1);
      gsi_next_nonvirtual_phi (&si2);

      if (gsi_end_p (si1) && gsi_end_p (si2))
	break;

      if (gsi_end_p (si1) || gsi_end_p (si2))
	SE_EXIT_FALSE ();

      phi1 = gsi_stmt (si1);
      phi2 = gsi_stmt (si2);

      size1 = gimple_phi_num_args (phi1);
      size2 = gimple_phi_num_args (phi2);

      if (size1 != size2)
	SE_EXIT_FALSE ();

      for (i = 0; i < size1; ++i)
	{
	  t1 = gimple_phi_arg (phi1, i)->def;
	  t2 = gimple_phi_arg (phi2, i)->def;

	  if (!compare_operand (t1, t2, func1, func2))
	    SE_EXIT_FALSE ();

	  e1 = gimple_phi_arg_edge (phi1, i);
	  e2 = gimple_phi_arg_edge (phi2, i);

	  if (!m_checker->compare_edge (e1, e2))
	    SE_EXIT_FALSE ();
	}

      gsi_next (&si2);
    }

  return true;
}

/* For given basic blocks BB1 and BB2 (from functions FUNC1 and FUNC),
   true value is returned if exception handling regions are equivalent
   in these blocks.  */

bool
sem_function::compare_eh_region (eh_region r1, eh_region r2, tree func1,
				 tree func2)
{
  eh_landing_pad lp1, lp2;
  eh_catch c1, c2;
  tree t1, t2;

  while (1)
    {
      if (!r1 && !r2)
	return true;

      if (!r1 || !r2)
	return false;

      if (r1->index != r2->index || r1->type != r2->type)
	return false;

      /* Landing pads comparison */
      lp1 = r1->landing_pads;
      lp2 = r2->landing_pads;

      while (lp1 && lp2)
	{
	  if (lp1->index != lp2->index)
	    return false;

	  /* Comparison of post landing pads. */
	  if (lp1->post_landing_pad && lp2->post_landing_pad)
	    {
	      t1 = lp1->post_landing_pad;
	      t2 = lp2->post_landing_pad;

	      gcc_assert (TREE_CODE (t1) == LABEL_DECL);
	      gcc_assert (TREE_CODE (t2) == LABEL_DECL);

	      if (!compare_tree_ssa_label (t1, t2, func1, func2))
		return false;
	    }
	  else if (lp1->post_landing_pad || lp2->post_landing_pad)
	    return false;

	  lp1 = lp1->next_lp;
	  lp2 = lp2->next_lp;
	}

      if (lp1 || lp2)
	return false;

      switch (r1->type)
	{
	case ERT_TRY:
	  c1 = r1->u.eh_try.first_catch;
	  c2 = r2->u.eh_try.first_catch;

	  while (c1 && c2)
	    {
	      /* Catch label checking */
	      if (c1->label && c2->label)
		{
		  if (!compare_tree_ssa_label (c1->label, c2->label,
					       func1, func2))
		    return false;
		}
	      else if (c1->label || c2->label)
		return false;

	      /* Type list checking */
	      if (!compare_type_list (c1->type_list, c2->type_list))
		return false;

	      c1 = c1->next_catch;
	      c2 = c2->next_catch;
	    }

	  break;

	case ERT_ALLOWED_EXCEPTIONS:
	  if (r1->u.allowed.filter != r2->u.allowed.filter)
	    return false;

	  if (!compare_type_list (r1->u.allowed.type_list,
				  r2->u.allowed.type_list))
	    return false;

	  break;
	case ERT_CLEANUP:
	  break;
	case ERT_MUST_NOT_THROW:
	  if (r1->u.must_not_throw.failure_decl != r1->u.must_not_throw.failure_decl)
	    return false;
	  break;
	default:
	  gcc_unreachable ();
	  break;
	}

      /* If there are sub-regions, process them.  */
      if ((!r1->inner && r2->inner) || (!r1->next_peer && r2->next_peer))
	return false;

      if (r1->inner)
	{
	  r1 = r1->inner;
	  r2 = r2->inner;
	}

      /* If there are peers, process them.  */
      else if (r1->next_peer)
	{
	  r1 = r1->next_peer;
	  r2 = r2->next_peer;
	}
      /* Otherwise, step back up the tree to the next peer.  */
      else
	{
	  do
	    {
	      r1 = r1->outer;
	      r2 = r2->outer;

	      /* All nodes have been visited. */
	      if (!r1 && !r2)
		return true;
	    }
	  while (r1->next_peer == NULL);

	  r1 = r1->next_peer;
	  r2 = r2->next_peer;
	}
    }

  return false;
}

/* Verifies that trees T1 and T2, representing function declarations
   are equivalent from perspective of ICF.  */

bool
sem_function::compare_function_decl (tree t1, tree t2)
{
  if (t1 == t2)
    return true;

  bool ret = pointer_set_contains (tree_refs_set, t1)
	     && pointer_set_contains (m_compared_func->tree_refs_set, t2);

  if (ret)
    return true;

  /* If function decl is WEAKREF, we compare targets.  */
  struct cgraph_node *f1 = cgraph_get_node (t1);
  struct cgraph_node *f2 = cgraph_get_node (t2);

  if(f1 && f2 && f1->weakref && f2->weakref)
    ret = f1->alias_target == f2->alias_target;

  return ret;
}

/* Verifies that trees T1 and T2 do correspond.  */

bool
sem_function::compare_variable_decl (tree t1, tree t2, tree func1, tree func2)
{
  if (t1 == t2)
    return true;

  bool ret = pointer_set_contains (tree_refs_set, t1)
	     && pointer_set_contains (m_compared_func->tree_refs_set, t2);

  if (ret)
    return true;

  ret = m_checker->compare_decl (t1, t2, func1, func2);

  SE_EXIT_DEBUG (ret);
}


/* Returns true if tree T can be compared as a handled component.  */

bool
sem_function::icf_handled_component_p (tree t)
{
  enum tree_code tc = TREE_CODE (t);

  return ((handled_component_p (t))
	  || tc == ADDR_EXPR || tc == MEM_REF || tc == REALPART_EXPR
	  || tc == IMAGPART_EXPR || tc == OBJ_TYPE_REF);
}

/* Basic blocks dictionary BB_DICT returns true if SOURCE index BB
   corresponds to TARGET.  */

bool
sem_function::bb_dict_test (int* bb_dict, int source, int target)
{
  if (bb_dict[source] == -1)
    {
      bb_dict[source] = target;
      return true;
    }
  else
    return bb_dict[source] == target;
}

/* If T1 and T2 are SSA names, dictionary comparison is processed. Otherwise,
   declaration comparasion is executed.  */

bool
sem_function::compare_ssa_name (tree t1, tree t2, tree func1, tree func2)
{
  tree b1, b2;
  bool ret;

  if (!m_checker->compare_ssa_name (t1, t2))
    SE_EXIT_FALSE ();

  if (SSA_NAME_IS_DEFAULT_DEF (t1))
    {
      b1 = SSA_NAME_VAR (t1);
      b2 = SSA_NAME_VAR (t2);

      if (b1 == NULL && b2 == NULL)
	return true;

      if (b1 == NULL || b2 == NULL || TREE_CODE (b1) != TREE_CODE (b2))
	SE_EXIT_FALSE ();

      switch (TREE_CODE (b1))
	{
	case VAR_DECL:
	  SE_EXIT_DEBUG (compare_variable_decl (t1, t2, func1, func2));
	case PARM_DECL:
	case RESULT_DECL:
	  ret = m_checker->compare_decl (b1, b2, func1, func2);
	  SE_EXIT_DEBUG (ret);
	default:
	  SE_EXIT_FALSE_WITH_MSG ("Unknown TREE code reached")
	}
    }
  else
    return true;
}

/* Function responsible for comparison of handled components T1 and T2.
   If these components, from functions FUNC1 and FUNC2, are equal, true
   is returned.  */

bool
sem_function::compare_operand (tree t1, tree t2,
			       tree func1, tree func2)
{
  tree base1, base2, x1, x2, y1, y2, z1, z2;
  HOST_WIDE_INT offset1, offset2;
  bool ret;

  if (!t1 && !t2)
    return true;
  else if (!t1 || !t2)
    return false;

  if (!types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2)))
    SE_EXIT_FALSE_WITH_MSG ("types are not compatible");

  if (TREE_CODE (t1) == RECORD_TYPE && TREE_CODE (t2) == RECORD_TYPE)
    {
      tree ctx1 = DECL_CONTEXT (t1);
      tree ctx2 = DECL_CONTEXT (t2);

      if (TYPE_BINFO (ctx1) && TYPE_BINFO (ctx2)
	  && polymorphic_type_binfo_p (TYPE_BINFO (ctx1))
	  && polymorphic_type_binfo_p (TYPE_BINFO (ctx2)))
	if (!types_same_for_odr (t1, t2))
	  SE_EXIT_FALSE_WITH_MSG ("polymorphic types detected");
    }

  base1 = get_addr_base_and_unit_offset (t1, &offset1);
  base2 = get_addr_base_and_unit_offset (t2, &offset2);

  if (offset1 != offset2)
    SE_EXIT_FALSE_WITH_MSG ("base offsets are different");

  if (base1 && base2)
    {
      t1 = base1;
      t2 = base2;
    }

  if (TREE_CODE (t1) != TREE_CODE (t2))
    SE_EXIT_FALSE_WITH_MSG ("");

  switch (TREE_CODE (t1))
    {
    case CONSTRUCTOR:
      {
	unsigned length1 = vec_safe_length (CONSTRUCTOR_ELTS (t1));
	unsigned length2 = vec_safe_length (CONSTRUCTOR_ELTS (t2));

	if (length1 != length2)
	  SE_EXIT_FALSE_WITH_MSG ("");

	for (unsigned i = 0; i < length1; i++)
	  if (!compare_operand (CONSTRUCTOR_ELT (t1, i)->value,
				CONSTRUCTOR_ELT (t2, i)->value, func1, func2))
	    SE_EXIT_FALSE_WITH_MSG ("");

	return true;
      }
    case ARRAY_REF:
    case ARRAY_RANGE_REF:
      {
	x1 = TREE_OPERAND (t1, 0);
	x2 = TREE_OPERAND (t2, 0);
	y1 = TREE_OPERAND (t1, 1);
	y2 = TREE_OPERAND (t2, 1);

	if (!compare_operand (array_ref_low_bound (t1),
			      array_ref_low_bound (t2),
			      func1, func2))
	  SE_EXIT_FALSE_WITH_MSG ("");
	if (!compare_operand (array_ref_element_size (t1),
			      array_ref_element_size (t2),
			      func1, func2))
	  SE_EXIT_FALSE_WITH_MSG ("");
	if (!compare_operand (x1, x2, func1, func2))
	  SE_EXIT_FALSE_WITH_MSG ("");
	return compare_operand (y1, y2, func1, func2);
      }

    case MEM_REF:
      {
	x1 = TREE_OPERAND (t1, 0);
	x2 = TREE_OPERAND (t2, 0);
	y1 = TREE_OPERAND (t1, 1);
	y2 = TREE_OPERAND (t2, 1);

	/* See if operand is an memory access (the test originate from
	 gimple_load_p).

	In this case the alias set of the function being replaced must
	be subset of the alias set of the other function.  At the moment
	we seek for equivalency classes, so simply require inclussion in
	both directions.  */

	if (!sem_item::compare_for_aliasing (y1, y2))
	  SE_EXIT_FALSE_WITH_MSG ("strict aliasing types do not match");

	if (!compare_operand (x1, x2, func1, func2))
	  SE_EXIT_FALSE_WITH_MSG ("");

	/* Type of the offset on MEM_REF does not matter.  */
	return wi::to_offset  (y1) == wi::to_offset  (y2);
      }
    case COMPONENT_REF:
      {
	x1 = TREE_OPERAND (t1, 0);
	x2 = TREE_OPERAND (t2, 0);
	y1 = TREE_OPERAND (t1, 1);
	y2 = TREE_OPERAND (t2, 1);

	ret = compare_operand (x1, x2, func1, func2)
	      && compare_operand (y1, y2, func1, func2);

	SE_EXIT_DEBUG (ret);
      }
    /* Virtual table call.  */
    case OBJ_TYPE_REF:
      {
	x1 = TREE_OPERAND (t1, 0);
	x2 = TREE_OPERAND (t2, 0);
	y1 = TREE_OPERAND (t1, 1);
	y2 = TREE_OPERAND (t2, 1);
	z1 = TREE_OPERAND (t1, 2);
	z2 = TREE_OPERAND (t2, 2);

	ret = compare_operand (x1, x2, func1, func2)
	      && compare_operand (y1, y2, func1, func2)
	      && compare_operand (z1, z2, func1, func2);

	SE_EXIT_DEBUG (ret);
      }
    case ADDR_EXPR:
      {
	x1 = TREE_OPERAND (t1, 0);
	x2 = TREE_OPERAND (t2, 0);

	ret = compare_operand (x1, x2, func1, func2);
	SE_EXIT_DEBUG (ret);
      }
    case SSA_NAME:
      {
	ret = compare_ssa_name (t1, t2, func1, func2);
	SE_EXIT_DEBUG (ret);
      }
    case INTEGER_CST:
      {
	ret = types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2))
	      && wi::to_offset  (t1) == wi::to_offset  (t2);

	SE_EXIT_DEBUG (ret);
      }
    case COMPLEX_CST:
    case VECTOR_CST:
    case STRING_CST:
    case REAL_CST:
      {
	ret = operand_equal_p (t1, t2, OEP_ONLY_CONST);
	SE_EXIT_DEBUG (ret);
      }
    case FUNCTION_DECL:
      {
	ret = compare_function_decl (t1, t2);
	SE_EXIT_DEBUG (ret);
      }
    case VAR_DECL:
      SE_EXIT_DEBUG (compare_variable_decl (t1, t2, func1, func2));
    case FIELD_DECL:
      {
	tree fctx1 = DECL_FCONTEXT (t1);
	tree fctx2 = DECL_FCONTEXT (t2);

	tree offset1 = DECL_FIELD_OFFSET (t1);
	tree offset2 = DECL_FIELD_OFFSET (t1);

	ret = compare_operand (fctx1, fctx2, func1, func2)
	      && compare_operand (offset1, offset2, func1, func2);

	SE_EXIT_DEBUG (ret);
      }
    case PARM_DECL:
    case LABEL_DECL:
    case RESULT_DECL:
    case CONST_DECL:
    case BIT_FIELD_REF:
      {
	ret = m_checker->compare_decl (t1, t2, func1, func2);
	SE_EXIT_DEBUG (ret);
      }
    default:
      SE_EXIT_FALSE_WITH_MSG ("Unknown TREE code reached")
    }
}

/* Iterates all tree types in T1 and T2 and returns true if all types
   are compatible.  */

bool
sem_function::compare_type_list (tree t1, tree t2)
{
  tree tv1, tv2;
  enum tree_code tc1, tc2;

  if (!t1 && !t2)
    return true;

  while (t1 != NULL && t2 != NULL)
    {
      tv1 = TREE_VALUE (t1);
      tv2 = TREE_VALUE (t2);

      tc1 = TREE_CODE (tv1);
      tc2 = TREE_CODE (tv2);

      if (tc1 == NOP_EXPR && tc2 == NOP_EXPR)
	{}
      else if (tc1 == NOP_EXPR || tc2 == NOP_EXPR)
	return false;
      else if (!types_compatible_p (tv1, tv2))
	return false;

      t1 = TREE_CHAIN (t1);
      t2 = TREE_CHAIN (t2);
    }

  return !(t1 || t2);
}


/* Semantic variable constructor that uses STACK as bitmap memory stack.  */

sem_variable::sem_variable (bitmap_obstack *stack): sem_item (VAR, stack)
{
}

/*  Constructor based on varpool node _NODE with computed hash _HASH.
    Bitmap STACK is used for memory allocation.  */

sem_variable::sem_variable (varpool_node *node, hashval_t _hash,
			    bitmap_obstack *stack): sem_item(VAR,
				  node, _hash, stack)
{
  gcc_checking_assert (node);
  gcc_checking_assert (get_node ());
}

/* Returns true if the item equals to ITEM given as argument.  */

bool
sem_variable::equals (sem_item *item)
{
  gcc_assert (item->type == VAR);

  return sem_variable::equals (ctor, static_cast<sem_variable *>(item)->ctor);
}

/* Compares trees T1 and T2 for semantic equality.  */

bool
sem_variable::equals (tree t1, tree t2)
{
  tree_code tc1 = TREE_CODE (t1);
  tree_code tc2 = TREE_CODE (t2);

  if (tc1 != tc2)
    return false;

  switch (tc1)
    {
    case CONSTRUCTOR:
      {
	unsigned len1 = vec_safe_length (CONSTRUCTOR_ELTS (t1));
	unsigned len2 = vec_safe_length (CONSTRUCTOR_ELTS (t2));

	if (len1 != len2)
	  return false;

	for (unsigned i = 0; i < len1; i++)
	  if (!sem_variable::equals (CONSTRUCTOR_ELT (t1, i)->value,
				     CONSTRUCTOR_ELT (t2, i)->value))
	    return false;

	return true;
      }
    case MEM_REF:
      {
	tree x1 = TREE_OPERAND (t1, 0);
	tree x2 = TREE_OPERAND (t2, 0);
	tree y1 = TREE_OPERAND (t1, 1);
	tree y2 = TREE_OPERAND (t2, 1);

	if (!sem_item::compare_for_aliasing (y1, y2))
	  SE_EXIT_FALSE_WITH_MSG ("strict aliasing types do not match");

	/* Type of the offset on MEM_REF does not matter.  */
	return sem_variable::equals (x1, x2)
	       && wi::to_offset  (y1) == wi::to_offset  (y2);
      }
    case NOP_EXPR:
    case ADDR_EXPR:
      {
	tree op1 = TREE_OPERAND (t1, 0);
	tree op2 = TREE_OPERAND (t2, 0);
	return sem_variable::equals (op1, op2);
      }
    case FUNCTION_DECL:
    case VAR_DECL:
    case FIELD_DECL:
    case LABEL_DECL:
      return t1 == t2;
    case INTEGER_CST:
      return types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2))
	     && wi::to_offset (t1) == wi::to_offset (t2);
    case STRING_CST:
    case REAL_CST:
    case COMPLEX_CST:
      return operand_equal_p (t1, t2, OEP_ONLY_CONST);
    case COMPONENT_REF:
    case ARRAY_REF:
    case POINTER_PLUS_EXPR:
      {
	tree x1 = TREE_OPERAND (t1, 0);
	tree x2 = TREE_OPERAND (t2, 0);
	tree y1 = TREE_OPERAND (t1, 1);
	tree y2 = TREE_OPERAND (t2, 1);

	return sem_variable::equals (x1, x2) && sem_variable::equals (y1, y2);
      }
    case ERROR_MARK:
      SE_EXIT_FALSE_WITH_MSG ("ERROR_MARK");
    default:
      SE_EXIT_FALSE_WITH_MSG ("Unknown TREE code reached")
    }
}

/* Parser function that visits a varpool NODE.  */

sem_variable *
sem_variable::parse (struct varpool_node *node, bitmap_obstack *stack)
{
  tree decl = node->decl;

  bool readonly = TYPE_P (decl) ? TYPE_READONLY (decl) : TREE_READONLY (decl);
  bool can_handle = readonly && (DECL_VIRTUAL_P (decl)
				 || !TREE_ADDRESSABLE (decl));

  if (!can_handle)
    return NULL;

  tree ctor = ctor_for_folding (decl);
  if (!ctor)
    return NULL;

  sem_variable *v = new sem_variable (node, 0, stack);

  v->init ();

  return v;
}

/* References independent hash function.  */

hashval_t
sem_variable::get_hash (void)
{
  if (hash)
    return hash;

  hashval_t hash = 456346417;

  tree_code tc = TREE_CODE (ctor);
  hash = iterative_hash_object (tc, hash);

  if (TREE_CODE (ctor) == CONSTRUCTOR)
    {
      unsigned length = vec_safe_length (CONSTRUCTOR_ELTS (ctor));
      hash = iterative_hash_object (length, hash);
    }

  return hash;
}

/* Merges instance with an ALIAS_ITEM, where alias, thunk or redirection can
   be applied.  */

bool
sem_variable::merge (sem_item *alias_item)
{
  gcc_assert (alias_item->type == VAR);

  sem_variable *alias_var = static_cast<sem_variable *> (alias_item);

  struct varpool_node *original = get_node ();
  struct varpool_node *alias = alias_var->get_node ();
  bool original_discardable = false;

  /* See if original is in a section that can be discarded if the main
     symbol is not used.  */
  if (DECL_EXTERNAL (original->decl))
    original_discardable = true;
  if (original->resolution == LDPR_PREEMPTED_REG
      || original->resolution == LDPR_PREEMPTED_IR)
    original_discardable = true;
  if (symtab_can_be_discarded (original))
    original_discardable = true;

  gcc_assert (!TREE_ASM_WRITTEN (alias->decl));

  if (original_discardable || DECL_EXTERNAL (alias_var->decl) ||
      !compare_sections (alias_var))
    {
      if (dump_file)
	fprintf (dump_file, "Varpool alias cannot be created\n\n");

      return false;
    }
  else
    {
      // alias cycle creation check
      varpool_node *n = original;

      while (n->alias)
	{
	  n = varpool_alias_target (n);
	  if (n == alias)
	    {
	      if (dump_file)
		fprintf (dump_file, "Varpool alias cannot be created (alias cycle).\n\n");

	      return false;
	    }
	}

      alias->analyzed = false;

      DECL_INITIAL (alias->decl) = DECL_INITIAL (alias_var->decl);
      ipa_remove_all_references (&alias->ref_list);

      varpool_create_variable_alias (alias_var->decl, decl);
      symtab_resolve_alias (alias, original);

      if (dump_file)
	fprintf (dump_file, "Varpool alias has been created.\n\n");

      return true;
    }
}

bool
sem_variable::compare_sections (sem_variable *alias)
{
  const char *source = node->get_section ();
  const char *target = alias->node->get_section();

  if (source == NULL && target == NULL)
    return true;
  else if(!source || !target)
    return false;
  else
    return strcmp (source, target) == 0;
}

/* Dump symbol to FILE.  */

void
sem_variable::dump_to_file (FILE *file)
{
  gcc_assert (file);

  print_node (file, "", decl, 0);
  fprintf (file, "\n\n");
}

/* Iterates though a constructor and identifies tree references
   we are interested in semantic function equality.  */

void
sem_variable::parse_tree_refs (tree t)
{
  switch (TREE_CODE (t))
    {
    case CONSTRUCTOR:
      {
	unsigned length = vec_safe_length (CONSTRUCTOR_ELTS (t));

	for (unsigned i = 0; i < length; i++)
	  parse_tree_refs(CONSTRUCTOR_ELT (t, i)->value);

	break;
      }
    case NOP_EXPR:
    case ADDR_EXPR:
      {
	tree op = TREE_OPERAND (t, 0);
	parse_tree_refs (op);
	break;
      }
    case FUNCTION_DECL:
      {
	tree_refs.safe_push (t);
	break;
      }
    default:
      break;
    }
}

unsigned int sem_item_optimizer::class_id = 0;

sem_item_optimizer::sem_item_optimizer (): m_classes_count (0),
  m_split_map (NULL),
  m_cgraph_node_hooks (NULL), m_varpool_node_hooks (NULL)
{
  worklist.create (0);
  m_items.create (0);
  m_removed_items_set = pointer_set_create ();
  m_classes.create (0);
  bitmap_obstack_initialize (&m_bmstack);
}

sem_item_optimizer::~sem_item_optimizer ()
{
  for (unsigned int i = 0; i < m_items.length (); i++)
    delete m_items[i];

  for (hash_table<congruence_class_group_hash>::iterator it = m_classes.begin ();
       it != m_classes.end (); ++it)
    {
      for (unsigned int i = 0; i < (*it).classes.length (); i++)
	delete (*it).classes[i];

      (*it).classes.release ();
    }

  m_classes.dispose ();
  worklist.dispose ();

  m_items.release ();

  if (m_split_map != NULL)
    delete m_split_map;

  bitmap_obstack_release (&m_bmstack);
  pointer_set_destroy (m_removed_items_set);
}

/* Write IPA ICF summary for symbols.  */

void
sem_item_optimizer::write_summary (void)
{
  unsigned int count = 0;

  struct output_block *ob = create_output_block (LTO_section_ipa_icf);
  lto_symtab_encoder_t encoder = ob->decl_state->symtab_node_encoder;
  ob->cgraph_node = NULL;

  /* Calculate number of symbols to be serialized.  */
  for (lto_symtab_encoder_iterator lsei = lsei_start_in_partition (encoder);
       !lsei_end_p (lsei);
       lsei_next_in_partition (&lsei))
    {
      struct symtab_node *node = lsei_node (lsei);

      if (m_symtab_node_map.contains (node))
	count++;
    }

  streamer_write_uhwi (ob, count);

  /* Process all of the symbols.  */
  for (lto_symtab_encoder_iterator lsei = lsei_start_in_partition (encoder);
       !lsei_end_p (lsei);
       lsei_next_in_partition (&lsei))
    {
      struct symtab_node *node = lsei_node (lsei);

      sem_item **item = m_symtab_node_map.contains (node);

      if (item && *item)
	{
	  int node_ref = lto_symtab_encoder_encode (encoder, node);
	  streamer_write_uhwi_stream (ob->main_stream, node_ref);

	  streamer_write_uhwi (ob, (*item)->get_hash ());
	}
    }

  streamer_write_char_stream (ob->main_stream, 0);
  produce_asm (ob, NULL);
  destroy_output_block (ob);
}

/* Reads a section from LTO stream file FILE_DATA. Input block for DATA
   contains LEN bytes.  */

void
sem_item_optimizer::read_section (struct lto_file_decl_data *file_data,
				  const char *data, size_t len)
{
  const struct lto_function_header *header =
  (const struct lto_function_header *) data;
  const int cfg_offset = sizeof (struct lto_function_header);
  const int main_offset = cfg_offset + header->cfg_size;
  const int string_offset = main_offset + header->main_size;
  struct data_in *data_in;
  struct lto_input_block ib_main;
  unsigned int i;
  unsigned int count;

  LTO_INIT_INPUT_BLOCK (ib_main, (const char *) data + main_offset, 0,
			header->main_size);

  data_in =
    lto_data_in_create (file_data, (const char *) data + string_offset,
			header->string_size, vNULL);

  count = streamer_read_uhwi (&ib_main);

  for (i = 0; i < count; i++)
    {
      unsigned int index;
      struct symtab_node *node;
      lto_symtab_encoder_t encoder;

      index = streamer_read_uhwi (&ib_main);
      encoder = file_data->symtab_node_encoder;
      node = lto_symtab_encoder_deref (encoder, index);

      hashval_t hash = streamer_read_uhwi (&ib_main);

      gcc_assert (node->definition);

      if (dump_file)
	fprintf (dump_file, "Symbol added:%s (tree: %p, uid:%u)\n", node->asm_name (),
		 (void *) node->decl, node->order);

      if (is_a_helper<cgraph_node *>::test (node))
	{
	  cgraph_node *cnode = cgraph (node);

	  m_items.safe_push (new sem_function (cnode, hash, &m_bmstack));
	}
      else
	{
	  varpool_node *vnode = varpool (node);

	  m_items.safe_push (new sem_variable (vnode, hash, &m_bmstack));
	}
    }

  lto_free_section_data (file_data, LTO_section_ipa_icf, NULL, data,
			 len);
  lto_data_in_delete (data_in);
}

/* Read IPA IPA ICF summary for symbols.  */

void
sem_item_optimizer::read_summary (void)
{
  struct lto_file_decl_data **file_data_vec = lto_get_file_decl_data ();
  struct lto_file_decl_data *file_data;
  unsigned int j = 0;

  while ((file_data = file_data_vec[j++]))
    {
      size_t len;
      const char *data = lto_get_section_data (file_data,
			 LTO_section_ipa_icf, NULL, &len);

      if (data)
	read_section (file_data, data, len);
    }
}

/* Register callgraph and varpool hooks.  */

void
sem_item_optimizer::register_hooks (void)
{
  m_cgraph_node_hooks = cgraph_add_node_removal_hook (
			  &sem_item_optimizer::cgraph_removal_hook, this);

  m_varpool_node_hooks = varpool_add_node_removal_hook (
			   &sem_item_optimizer::varpool_removal_hook, this);
}

/* Unregister callgraph and varpool hooks.  */

void
sem_item_optimizer::unregister_hooks (void)
{
  if (m_cgraph_node_hooks)
    cgraph_remove_node_removal_hook (m_cgraph_node_hooks);

  if (m_varpool_node_hooks)
    varpool_remove_node_removal_hook (m_varpool_node_hooks);
}

/* Adds a CLS to hashtable associated by hash value.  */

void
sem_item_optimizer::add_class (congruence_class *cls)
{
  gcc_assert (cls->members.length ());

  congruence_class_group_t *group = get_group_by_hash (
				      cls->members[0]->get_hash (),
				      cls->members[0]->type);
  group->classes.safe_push (cls);
}

/* Gets a congruence class group based on given HASH value and TYPE.  */

congruence_class_group_t *
sem_item_optimizer::get_group_by_hash (hashval_t hash, sem_item_type type)
{
  congruence_class_group_t *item = XNEW (congruence_class_group_t);
  item->hash = hash;
  item->type = type;

  congruence_class_group **slot = m_classes.find_slot (item, INSERT);

  if (*slot)
    free (item);
  else
    {
      item->classes.create (1);
      *slot = item;
    }

  return *slot;
}

/* Callgraph removal hook called for a NODE with a custom DATA.  */

void
sem_item_optimizer::cgraph_removal_hook (struct cgraph_node *node, void *data)
{
  sem_item_optimizer *optimizer = (sem_item_optimizer *) data;
  optimizer->remove_symtab_node (node);
}

/* Varpool removal hook called for a NODE with a custom DATA.  */

void
sem_item_optimizer::varpool_removal_hook (struct varpool_node *node, void *data)
{
  sem_item_optimizer *optimizer = (sem_item_optimizer *) data;
  optimizer->remove_symtab_node (node);
}

/* Remove symtab NODE triggered by symtab removal hooks.  */

void
sem_item_optimizer::remove_symtab_node (struct symtab_node *node)
{
  gcc_assert (!m_classes.elements());

  pointer_set_insert (m_removed_items_set, node);
}

/* Removes all callgraph and varpool nodes that are marked by symtab
   as deleted.  */

void
sem_item_optimizer::filter_removed_items (void)
{
  vec <sem_item *> filtered;
  filtered.create (m_items.length());

  for (unsigned int i = 0; i < m_items.length(); i++)
    {
      sem_item *item = m_items[i];

      if (!flag_ipa_icf_functions && item->type == FUNC)
	continue;

      if (!flag_ipa_icf_variables && item->type == VAR)
	continue;

      bool no_body_function = false;

      if (item->type == FUNC)
	{
	  struct cgraph_node *cnode = static_cast <sem_function *>(item)->get_node ();

	  no_body_function = in_lto_p && (cnode->alias || cnode->body_removed);
	}

      if(!pointer_set_contains (m_removed_items_set, m_items[i]->node)
	  && !no_body_function)
	{
	  if (item->type == VAR || (!DECL_CXX_CONSTRUCTOR_P (item->decl)
				    && !DECL_CXX_DESTRUCTOR_P (item->decl)))
	    filtered.safe_push (m_items[i]);
	}
    }

  m_items.release ();

  for (unsigned int i = 0; i < filtered.length(); i++)
    m_items.safe_push (filtered[i]);

  filtered.release ();
}

/* Optimizer entry point.  */

void
sem_item_optimizer::execute (void)
{
  filter_removed_items ();
  build_hash_based_classes ();

  if (dump_file)
    fprintf (dump_file, "Dump after hash based groups\n");
  dump_cong_classes ();

  for (unsigned int i = 0; i < m_items.length(); i++)
    m_items[i]->init_wpa ();

  subdivide_classes_by_equality (true);

  if (dump_file)
    fprintf (dump_file, "Dump after WPA based types groups\n");
  dump_cong_classes ();

  parse_nonsingleton_classes ();
  subdivide_classes_by_equality ();

  if (dump_file)
    fprintf (dump_file, "Dump after full equality comparison of groups\n");

  dump_cong_classes ();

  unsigned int prev_class_count = m_classes_count;

  process_cong_reduction ();
  dump_cong_classes ();
  merge_classes (prev_class_count);

  if (dump_file && (dump_flags & TDF_DETAILS))
    dump_symtab (dump_file);
}

/* Function responsible for visiting all potential functions and
   read-only variables that can be merged.  */

void
sem_item_optimizer::parse_funcs_and_vars (void)
{
  struct cgraph_node *cnode;
  sem_item **slot;

  if (flag_ipa_icf_functions)
    FOR_EACH_DEFINED_FUNCTION (cnode)
    {
      sem_function *f = sem_function::parse (cnode, &m_bmstack);
      if (f)
	{
	  m_items.safe_push (f);
	  slot = m_symtab_node_map.insert (cnode);
	  *slot = f;

	  if (dump_file)
	    fprintf (dump_file, "Parsed function:%s\n", f->asm_name ());

	  if (dump_file && (dump_flags & TDF_DETAILS))
	    f->dump_to_file (dump_file);
	}
      else if (dump_file)
	fprintf (dump_file, "Not parsed function:%s\n", cnode->asm_name ());
    }

  varpool_node *vnode;

  if (flag_ipa_icf_variables)
    FOR_EACH_DEFINED_VARIABLE (vnode)
    {
      sem_variable *v = sem_variable::parse (vnode, &m_bmstack);

      if (v)
	{
	  m_items.safe_push (v);
	  slot = m_symtab_node_map.insert (vnode);
	  *slot = v;
	}
    }
}

/* Makes pairing between a congruence class CLS and semantic ITEM.  */

void
sem_item_optimizer::add_item_to_class (congruence_class *cls, sem_item *item)
{
  item->index_in_class = cls->members.length ();
  cls->members.safe_push (item);
  item->cls = cls;
}

/* Congruence classes are built by hash value.  */

void
sem_item_optimizer::build_hash_based_classes (void)
{
  for (unsigned i = 0; i < m_items.length (); i++)
    {
      sem_item *item = m_items[i];

      congruence_class_group_t *group = get_group_by_hash (item->get_hash (),
					item->type);

      if (!group->classes.length ())
	{
	  m_classes_count++;
	  group->classes.safe_push (new congruence_class (class_id++));
	}

      add_item_to_class (group->classes[0], item);
    }
}

/* Semantic items in classes having more than one element and initialized.
   In case of WPA, we load function body.  */

void
sem_item_optimizer::parse_nonsingleton_classes (void)
{
  for (hash_table <congruence_class_group_hash>::iterator it = m_classes.begin ();
       it != m_classes.end (); ++it)
    {
      for (unsigned i = 0; i < (*it).classes.length (); i++)
	{
	  congruence_class *c = (*it).classes [i];

	  if (c->members.length() > 1)
	    for (unsigned j = 0; j < c->members.length (); j++)
	      {
		sem_item *item = c->members[j];
		sem_item **slot;

		slot = m_decl_map.insert (item->decl);
		*slot = item;

	      }
	}
    }

  unsigned int init_called_count = 0;

  for (hash_table <congruence_class_group_hash>::iterator it = m_classes.begin ();
       it != m_classes.end (); ++it)
    {
      /* We fill in all declarations for sem_items.  */
      for (unsigned i = 0; i < (*it).classes.length (); i++)
	{
	  congruence_class *c = (*it).classes [i];

	  if (c->members.length() > 1)
	    for (unsigned j = 0; j < c->members.length (); j++)
	      {
		sem_item *item = c->members[j];

		item->init ();
		item->init_refs ();

		init_called_count++;

		for (unsigned j = 0; j < item->tree_refs.length (); j++)
		  {
		    sem_item **result = m_decl_map.contains (item->tree_refs[j]);

		    if(result)
		      {
			sem_item *target = *result;
			item->refs.safe_push (target);

			unsigned index = item->refs.length ();
			target->usages.safe_push (new sem_usage_pair(item, index));
			bitmap_set_bit (target->usage_index_bitmap, index);
			pointer_set_insert (item->tree_refs_set, item->tree_refs[j]);
		      }
		  }
	      }
	}
    }

  if (dump_file)
    fprintf (dump_file, "Init called for %u items (%.2f%%).\n", init_called_count,
	     100.0f * init_called_count / m_items.length ());
}

/* Equality function for semantic items is used to subdivide existing
   classes. If IN_WPA, fast equality function is invoked.  */

void
sem_item_optimizer::subdivide_classes_by_equality (bool in_wpa)
{
  for (hash_table <congruence_class_group_hash>::iterator it = m_classes.begin ();
       it != m_classes.end (); ++it)
    {
      unsigned int class_count = (*it).classes.length ();

      for (unsigned i = 0; i < class_count; i++)
	{
	  congruence_class *c = (*it).classes [i];

	  if (c->members.length() > 1)
	    {
	      vec <sem_item *> new_vector;
	      new_vector.create (c->members.length ());

	      sem_item *first = c->members[0];
	      new_vector.safe_push (first);

	      unsigned class_split_first = (*it).classes.length ();

	      for (unsigned j = 1; j < c->members.length (); j++)
		{
		  sem_item *item = c->members[j];

		  bool equals = in_wpa ? first->equals_wpa (item) : first->equals (item);

		  if (equals)
		    new_vector.safe_push (item);
		  else
		    {
		      bool integrated = false;

		      for (unsigned k = class_split_first; k < (*it).classes.length (); k++)
			{
			  sem_item *x = (*it).classes[k]->members[0];
			  bool equals = in_wpa ? x->equals_wpa (item) : x->equals (item);

			  if (equals)
			    {
			      integrated = true;
			      add_item_to_class ((*it).classes[k], item);

			      break;
			    }
			}

		      if (!integrated)
			{
			  congruence_class *c = new congruence_class (class_id++);
			  m_classes_count++;
			  add_item_to_class (c, item);

			  (*it).classes.safe_push (c);
			}
		    }
		}

	      // we replace newly created new_vector for the class we've just splitted
	      c->members.release ();
	      c->members.create (new_vector.length ());

	      for (unsigned int j = 0; j < new_vector.length (); j++)
		add_item_to_class (c, new_vector[j]);

	      new_vector.release ();
	    }
	}
    }

  verify_classes ();
}

/* Verify congruence classes if checking is enabled.  */

void
sem_item_optimizer::verify_classes (void)
{
#if ENABLE_CHECKING
  for (hash_table <congruence_class_group_hash>::iterator it = m_classes.begin ();
       it != m_classes.end (); ++it)
    {
      for (unsigned int i = 0; i < (*it).classes.length (); i++)
	{
	  congruence_class *cls = (*it).classes[i];

	  gcc_checking_assert (cls);
	  gcc_checking_assert (cls->members.length () > 0);

	  for (unsigned int j = 0; j < cls->members.length (); j++)
	    {
	      sem_item *item = cls->members[j];

	      gcc_checking_assert (item);

	      for (unsigned k = 0; k < item->usages.length (); k++)
		{
		  sem_usage_pair *usage = item->usages[k];
		  gcc_checking_assert (usage->item->index_in_class <
				       usage->item->cls->members.length ());
		}
	    }
	}
    }
#endif
}

/* Disposes split map traverse function. CLS_PTR is pointer to congruence
   class, BSLOT is bitmap slot we want to release. DATA is mandatory,
   but unused argument.  */

bool
sem_item_optimizer::release_split_map (__attribute__((__unused__)) const void
				       *cls_ptr,
				       __attribute__((__unused__)) bitmap *bslot,
				       __attribute__((__unused__)) void *data)
{
  bitmap b = *bslot;

  BITMAP_FREE (b);

  return true;
}

/* Process split operation for a class given as pointer CLS_PTR,
   where bitmap B splits congruence class members. DATA is used
   as argument of split pair.  */

bool
sem_item_optimizer::traverse_congruence_split (const void *cls_ptr,
    bitmap *bslot, void *data)
{
  const congruence_class *cls = (const congruence_class *) cls_ptr;
  bitmap b = *bslot;

  traverse_split_pair *pair = (traverse_split_pair *) data;
  sem_item_optimizer *optimizer = pair->optimizer;
  const congruence_class *splitter_cls = pair->cls;

  /* If counted bits are greater than zero and less than the number of members
     a group will be splitted.  */
  unsigned popcount = bitmap_count_bits (b);

  if (popcount > 0 && popcount < cls->members.length ())
    {
      congruence_class* newclasses[2] = { new congruence_class (class_id++), new congruence_class (class_id++) };

      for (unsigned int i = 0; i < cls->members.length (); i++)
	{
	  int target = bitmap_bit_p (b, i);
	  congruence_class *tc = newclasses[target];

	  add_item_to_class (tc, cls->members[i]);
	}

#ifdef ENABLE_CHECKING
      for (unsigned int i = 0; i < 2; i++)
	gcc_checking_assert (newclasses[i]->members.length ());
#endif

      if (splitter_cls == cls)
	optimizer->splitter_class_removed = true;

      /* Remove old class from worklist if presented.  */
      bool in_work_list = optimizer->worklist_contains (cls);

      if (in_work_list)
	optimizer->worklist_remove (cls);

      congruence_class_group g;
      g.hash = cls->members[0]->get_hash ();
      g.type = cls->members[0]->type;

      congruence_class_group *slot = optimizer->m_classes.find(&g);

      for (unsigned int i = 0; i < slot->classes.length (); i++)
	if (slot->classes[i] == cls)
	  {
	    slot->classes.ordered_remove (i);
	    break;
	  }

      /* New class will be inserted and integrated to work list.  */
      for (unsigned int i = 0; i < 2; i++)
	optimizer->add_class (newclasses[i]);

      /* Two classes replace one, so that increment just by one.  */
      optimizer->m_classes_count++;

      /* If OLD class was presented in the worklist, we remove the class
         are replace it will both newly created classes.  */
      if (in_work_list)
	for (unsigned int i = 0; i < 2; i++)
	  optimizer->worklist_push (newclasses[i]);
      else /* Just smaller class is inserted.  */
	{
	  unsigned int smaller_index = newclasses[0]->members.length () <
				       newclasses[1]->members.length () ?
				       0 : 1;
	  optimizer->worklist_push (newclasses[smaller_index]);
	}

      if (dump_file && (dump_flags & TDF_DETAILS))
	{
	  fprintf (dump_file, "  congruence class splitted:\n");
	  cls->dump (dump_file, 4);

	  fprintf (dump_file, "  newly created groups:\n");
	  for (unsigned int i = 0; i < 2; i++)
	    newclasses[i]->dump (dump_file, 4);
	}

      delete cls;
    }


  return true;
}

/* Tests if a class CLS used as INDEXth splits any congruence classes.
   Bitmap stack BMSTACK is used for bitmap allocation.  */

void
sem_item_optimizer::do_congruence_step_for_index (congruence_class *cls,
    unsigned int index)
{
  /* Split map reset */
  if (m_split_map != NULL)
    delete m_split_map;

  pointer_map <bitmap> *split_map = new pointer_map <bitmap> ();

  for (unsigned int i = 0; i < cls->members.length (); i++)
    {
      sem_item *item = cls->members[i];

      /* Iterate all usages that have INDEX as usage of the item.  */
      for (unsigned int j = 0; j < item->usages.length (); j++)
	{
	  sem_usage_pair *usage = item->usages[j];

	  if (usage->index != index)
	    continue;

	  bitmap *slot = split_map->contains (usage->item->cls);

	  if(!slot)
	    {
	      slot = split_map->insert (usage->item->cls);
	      *slot = BITMAP_ALLOC (&m_bmstack);
	    }

	  bitmap b = *slot;

#if ENABLE_CHECKING
	  gcc_checking_assert (usage->item->cls);
	  gcc_checking_assert (usage->item->index_in_class <
			       usage->item->cls->members.length ());
#endif

	  bitmap_set_bit (b, usage->item->index_in_class);
	}
    }

  traverse_split_pair pair;
  pair.optimizer = this;
  pair.cls = cls;

  splitter_class_removed = false;
  split_map->traverse (&sem_item_optimizer::traverse_congruence_split, &pair);

  /* Bitmap clean-up.  */
  split_map->traverse (&sem_item_optimizer::release_split_map, NULL);
}

/* Every usage of a congruence class CLS is a candidate that can split the
   collection of classes. Bitmap stack BMSTACK is used for bitmap
   allocation.  */

void
sem_item_optimizer::do_congruence_step (congruence_class *cls)
{
  bitmap_iterator bi;
  unsigned int i;

  bitmap usage = BITMAP_ALLOC (&m_bmstack);

  for (unsigned int i = 0; i < cls->members.length (); i++)
    bitmap_ior_into (usage, cls->members[i]->usage_index_bitmap);

  EXECUTE_IF_SET_IN_BITMAP (usage, 0, i, bi)
  {
    if (dump_file && (dump_flags & TDF_DETAILS))
      fprintf (dump_file, "  processing congruece step for class: %u, index: %u\n",
	       cls->id, i);

    do_congruence_step_for_index (cls, i);

    if (splitter_class_removed)
      break;
  }

  BITMAP_FREE (usage);
}

/* Adds a newly created congruence class CLS to worklist.  */

void
sem_item_optimizer::worklist_push (congruence_class *cls)
{
  congruence_class **slot = worklist.find_slot (cls, INSERT);

  if (*slot)
    return;

  *slot = cls;
}

/* Pops a class from worklist. */

congruence_class *
sem_item_optimizer::worklist_pop (void)
{
  gcc_assert (worklist.elements ());

  congruence_class *cls = &(*worklist.begin ());
  worklist.remove_elt (cls);

  return cls;
}

/* Iterative congruence reduction function.  */

void
sem_item_optimizer::process_cong_reduction (void)
{
  for (hash_table<congruence_class_group_hash>::iterator it = m_classes.begin ();
       it != m_classes.end (); ++it)
    for (unsigned i = 0; i < (*it).classes.length (); i++)
      if ((*it).classes[i]->is_class_used ())
	worklist_push ((*it).classes[i]);

  if (dump_file)
    fprintf (dump_file, "Worklist has been filled with: %lu\n",
	     worklist.elements ());

  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "Congruence class reduction\n");

  while (worklist.elements ())
    {
      congruence_class *cls = worklist_pop ();
      do_congruence_step (cls);
    }
}

/* Debug function prints all informations about congruence classes.  */

void
sem_item_optimizer::dump_cong_classes (void)
{
  if (!dump_file)
    return;

  fprintf (dump_file,
	   "Congruence classes: %u (unique hash values: %lu), with total: %u items\n",
	   m_classes_count, m_classes.elements(), m_items.length ());

  /* Histogram calculation.  */
  unsigned int max_index = 0;
  unsigned int* histogram = XCNEWVEC (unsigned int, m_items.length ());

  for (hash_table<congruence_class_group_hash>::iterator it = m_classes.begin ();
       it != m_classes.end (); ++it)

    for (unsigned i = 0; i < (*it).classes.length (); i++)
      {
	unsigned int c = (*it).classes[i]->members.length ();
	histogram[c]++;

	if (c > max_index)
	  max_index = c;
      }

  fprintf (dump_file,
	   "Class size histogram [num of members]: number of classe number of classess\n");

  for (unsigned int i = 0; i <= max_index; i++)
    if (histogram[i])
      fprintf (dump_file, "[%u]: %u classes\n", i, histogram[i]);

  fprintf (dump_file, "\n\n");


  if (dump_flags & TDF_DETAILS)
    for (hash_table<congruence_class_group_hash>::iterator it = m_classes.begin ();
	 it != m_classes.end (); ++it)
      {
	fprintf (dump_file, "  group: with %u classes:\n", (*it).classes.length ());

	for (unsigned i = 0; i < (*it).classes.length (); i++)
	  {
	    (*it).classes[i]->dump (dump_file, 4);

	    if(i < (*it).classes.length () - 1)
	      fprintf (dump_file, " ");
	  }
      }

  free (histogram);
}

/* After reduction is done, we can declare all items in a group
   to be equal. PREV_CLASS_COUNT is start number of classes
   before reduction.  */

void
sem_item_optimizer::merge_classes (unsigned int prev_class_count)
{
  unsigned int item_count = m_items.length ();
  unsigned int class_count = m_classes_count;
  unsigned int equal_items = item_count - class_count;

  if (dump_file)
    {
      fprintf (dump_file, "\nItem count: %u\n", item_count);
      fprintf (dump_file, "Congruent classes before: %u, after: %u\n",
	       prev_class_count, class_count);
      fprintf (dump_file, "Average class size before: %.2f, after: %.2f\n",
	       1.0f * item_count / prev_class_count,
	       1.0f * item_count / class_count);
      fprintf (dump_file, "Equal symbols: %u\n", equal_items);
      fprintf (dump_file, "Fraction of visited symbols: %.2f%%\n\n",
	       100.0f * equal_items / item_count);
    }

  for (hash_table<congruence_class_group_hash>::iterator it = m_classes.begin ();
       it != m_classes.end (); ++it)
    for (unsigned int i = 0; i < (*it).classes.length (); i++)
      {
	congruence_class *c = (*it).classes[i];

	if (c->members.length () == 1)
	  continue;

	gcc_assert (c->members.length ());

	sem_item *source = c->members[0];

	for (unsigned int j = 1; j < c->members.length (); j++)
	  {
	    sem_item *alias = c->members[j];

	    if (dump_file)
	      {
		fprintf (dump_file, "Semantic equality hit:%s->%s\n",
			 source->name (), alias->name ());
		fprintf (dump_file, "Assembler symbol names:%s->%s\n",
			 source->asm_name (), alias->asm_name ());
	      }

	    if (dump_file && (dump_flags & TDF_DETAILS))
	      {
		source->dump_to_file (dump_file);
		alias->dump_to_file (dump_file);
	      }

	    source->merge (alias);
	  }
      }
}

/* Dump function prints all class members to a FILE with an INDENT.  */

void
congruence_class::dump (FILE *file, unsigned int indent) const
{
  FPRINTF_SPACES (file, indent, "class with id: %u, hash: %u, items: %u\n",
		  id, members[0]->get_hash (), members.length ());

  FPUTS_SPACES (file, indent + 2, "");
  for (unsigned i = 0; i < members.length (); i++)
    fprintf (file, "%s(%p/%u)", members[i]->asm_name (), (void *) members[i]->decl,
	     members[i]->node->order);

  fprintf (file, "\n");
}

/* Returns true if there's a member that is used from another group.  */

bool
congruence_class::is_class_used (void)
{
  for (unsigned int i = 0; i < members.length (); i++)
    if (members[i]->usages.length ())
      return true;

  return false;
}

/* Initialization and computation of symtab node hash, there data
   are propagated later on.  */

static sem_item_optimizer *optimizer = NULL;

/* Generate pass summary for IPA ICF pass.  */

static void
ipa_icf_generate_summary (void)
{
  if (!optimizer)
    optimizer = new sem_item_optimizer ();

  optimizer->parse_funcs_and_vars ();
}

/* Write pass summary for IPA ICF pass.  */

static void
ipa_icf_write_summary (void)
{
  gcc_assert (optimizer);

  optimizer->write_summary ();
}

/* Read pass summary for IPA ICF pass.  */

static void
ipa_icf_read_summary (void)
{
  if (!optimizer)
    optimizer = new sem_item_optimizer ();

  optimizer->read_summary ();
  optimizer->register_hooks ();
}

/* Semantic equality exection function.  */

static unsigned int
ipa_icf_driver (void)
{
  gcc_assert (optimizer);

  optimizer->execute ();
  optimizer->unregister_hooks ();

  delete optimizer;

  return 0;
}

const pass_data pass_data_ipa_icf =
{
  IPA_PASS,		    /* type */
  "icf",		    /* name */
  OPTGROUP_IPA,             /* optinfo_flags */
  true,                     /* has_execute */
  TV_IPA_ICF,		    /* tv_id */
  0,                        /* properties_required */
  0,                        /* properties_provided */
  0,                        /* properties_destroyed */
  0,                        /* todo_flags_start */
  0,                        /* todo_flags_finish */
};

class pass_ipa_icf : public ipa_opt_pass_d
{
public:
  pass_ipa_icf (gcc::context *ctxt)
    : ipa_opt_pass_d (pass_data_ipa_icf, ctxt,
		      ipa_icf_generate_summary, /* generate_summary */
		      ipa_icf_write_summary, /* write_summary */
		      ipa_icf_read_summary, /* read_summary */
		      NULL, /*
		      write_optimization_summary */
		      NULL, /*
		      read_optimization_summary */
		      NULL, /* stmt_fixup */
		      0, /* function_transform_todo_flags_start */
		      NULL, /* function_transform */
		      NULL) /* variable_transform */
  {}

  /* opt_pass methods: */
  virtual bool gate (function *)
  {
    return flag_ipa_icf_variables || flag_ipa_icf_functions;
  }

  virtual unsigned int execute (function *)
  {
    return ipa_icf_driver();
  }
}; // class pass_ipa_icf

} // ipa_icf namespace

ipa_opt_pass_d *
make_pass_ipa_icf (gcc::context *ctxt)
{
  return new ipa_icf::pass_ipa_icf (ctxt);
}
