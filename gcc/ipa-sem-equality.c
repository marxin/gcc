/* Interprocedural semantic function equality pass
   Copyright (C) 2013 Free Software Foundation, Inc.

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

/* Interprocedural sematic function equality pass.

   The goal of this transformation is to discover functions which do have
   exactly the same semantics. For such function we could either create
   a virtual clone or do a simple function wrapper that will call
   equivalent function. If the function is just locally visible, all function
   calls can be redirected.

   The algorithm basically consists of 3 stages. In the first, we calculate
   for each newly visited function a simple checksum that includes
   number of arguments, types of that arguments, number of basic blocks and
   statements nested in each block. The checksum is saved to hashtable,
   where all functions having the same checksum live in a linked list.
   Each table collision is a candidate for semantic equality.

   Second, deep comparison phase, is based on further function collation.
   We traverse all basic blocks and each statement living in the block,
   building bidictionaries of SSA names, functions, parameters and variable
   declarations. Corresponding statement types are mandatory, each statement
   operand must point to an appropriate one in a function we do
   comparison with.

   Edge bidictionary is helpfull for phi node collation, where all phi node
   arguments must point to an appropriate basic block.

   Finally, function attribute chain is traversed and checked for having
   same set of values.

   If we encounter two candidates being really substitutable, we do merge type
   decision. We either process function aliasing or a simple wrapper
   is constructed.  */

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

#define SE_DUMP_MESSAGE(message) \
  do \
  { \
    if (dump_file && (dump_flags & TDF_DETAILS)) \
      fprintf (dump_file, "Debug message: %s (%s:%u)\n", message, __func__, __LINE__); \
  } \
  while (false);

#define SE_EXIT_FALSE_WITH_MSG(message) \
  do \
  { \
    if (dump_file && (dump_flags & TDF_DETAILS)) \
      fprintf (dump_file, "False returned: '%s' (%s:%u)\n", message, __func__, __LINE__); \
    return false; \
  } \
  while (false);

#define SE_EXIT_FALSE() \
  SE_EXIT_FALSE_WITH_MSG("")

#define SE_EXIT_DEBUG(result) \
  do \
  { \
    if (!result && dump_file && (dump_flags & TDF_DETAILS)) \
      fprintf (dump_file, "False returned (%s:%u)\n", __func__, __LINE__); \
    return result; \
  } \
  while (false);

#define SE_DIFF_STATEMENT(s1, s2, code) \
  do \
  { \
    if (dump_file && (dump_flags & TDF_DETAILS)) \
      { \
        fprintf (dump_file, "Different statement for code: %s:\n", code); \
        print_gimple_stmt (dump_file, s1, 3, TDF_DETAILS); \
        print_gimple_stmt (dump_file, s2, 3, TDF_DETAILS); \
      } \
    return false; \
  } \
  while (false);

#define SE_CF_EXIT_FALSE() \
do \
{ \
  if (dump_file && (dump_flags & TDF_DETAILS)) \
    fprintf (dump_file, "False returned (%s:%u)\n", __func__, __LINE__); \
  result = false; \
  goto exit_label; \
} \
while (false);

/* Forward struct declaration.  */
typedef struct sem_bb sem_bb_t;
typedef struct sem_func sem_func_t;

/* Function struct for sematic equality pass.  */
typedef struct sem_func
{
  /* Global unique function index.  */
  unsigned int index;
  /* Call graph structure reference.  */
  struct cgraph_node *node;
  /* Function declaration tree node.  */
  tree func_decl;
  /* Exception handling region tree.  */
  eh_region region_tree;
  /* Result type tree node.  */
  tree result_type;
  /* Array of argument tree types.  */
  tree *arg_types;
  /* Number of function arguments.  */
  unsigned int arg_count;
  /* Basic block count.  */
  unsigned int bb_count;
  /* Total amount of edges in the function.  */
  unsigned int edge_count;
  /* Array of sizes of all basic blocks.  */
  unsigned int *bb_sizes;
  /* Control flow graph checksum.  */
  hashval_t cfg_checksum;
  /* Total number of SSA names used in the function.  */
  unsigned ssa_names_size;
  /* Array of structures for all basic blocks.  */
  sem_bb_t **bb_sorted;
  /* Vector for all calls done by the function.  */
  vec<tree> called_functions;
  /* Computed semantic function hash value.  */
  hashval_t hash;
} sem_func_t;

/* Basic block struct for sematic equality pass.  */
typedef struct sem_bb
{
  /* Basic block the structure belongs to.  */
  basic_block bb;
  /* Reference to the semantic function this BB belongs to.  */
  sem_func_t *func;
  /* Number of non-debug statements in the basic block.  */
  unsigned nondbg_stmt_count;
  /* Number of edges connected to the block.  */
  unsigned edge_count;
  /* Computed hash value for basic block.  */
  hashval_t hash;
} sem_bb_t;

/* Tree declaration used for hash table purpose.  */
typedef struct decl_pair
{
  tree source;
  tree target;
} decl_pair_t;

/* Call graph edge pair used for hash table purpose.  */
typedef struct edge_pair
{
  edge source;
  edge target;
} edge_pair_t;

/* Global vector for all semantic functions.  */
static vec<sem_func_t *> semantic_functions;

/* Hash table struct used for a pair of declarations.  */

struct decl_var_hash: typed_noop_remove <decl_pair_t>
{
  typedef decl_pair_t value_type;
  typedef decl_pair_t compare_type;
  static inline hashval_t hash (const value_type *);
  static inline int equal (const value_type *, const compare_type *);
};

/* Hash compute function returns hash for a given declaration pair.  */

inline hashval_t
decl_var_hash::hash (const decl_pair_t *pair)
{
  return iterative_hash_expr (pair->source, 0);
}

/* Returns zero if PAIR1 and PAIR2 are equal.  */

inline int
decl_var_hash::equal (const decl_pair_t *pair1, const decl_pair_t *pair2)
{
  return pair1->source == pair2->source;
}

/* Hash table struct used for a pair of edges  */

struct edge_var_hash: typed_noop_remove <edge_pair_t>
{
  typedef edge_pair_t value_type;
  typedef edge_pair_t compare_type;
  static inline hashval_t hash (const value_type *);
  static inline int equal (const value_type *, const compare_type *);  
};

/* Hash compute function returns hash for a given edge pair.  */

inline hashval_t
edge_var_hash::hash (const edge_pair_t *pair)
{
  return htab_hash_pointer (pair->source);
}

/* Returns zero if PAIR1 and PAIR2 are equal.  */

inline int
edge_var_hash::equal (const edge_pair_t *pair1, const edge_pair_t *pair2)
{
  return pair1->source == pair2->source;
}

/* Struct used for all kind of function dictionaries like
   SSA names, call graph edges and all kind of declarations.  */
typedef struct func_dict
{
  /* Source mapping of SSA names.  */
  vec<int> source;
  /* Target mapping of SSA names.  */
  vec<int> target;
  /* Hash table for correspondence declarations.  */
  hash_table <decl_var_hash> decl_hash;
  /* Hash table for correspondence of edges.  */
  hash_table <edge_var_hash> edge_hash;
} func_dict_t;

/* Allocates and initializes VECTOR with N items of SSA_NAMES.  */

static void
init_ssa_names_vec (vec<int> &vector, unsigned n)
{
  unsigned i;

  vector.create (n);

  for (i = 0; i < n; i++)
    vector.safe_push (-1);
}

/* Function dictionary initializer, all members of D are itiliazed.
   Arrays for SSA names are allocated according to SSA_NAMES_SIZE1 and
   SSA_NAMES_SIZE2 arguments.  */

static void
func_dict_init (func_dict_t *d, unsigned ssa_names_size1,
                unsigned ssa_names_size2)
{
  init_ssa_names_vec (d->source, ssa_names_size1);
  init_ssa_names_vec (d->target, ssa_names_size2);

  d->decl_hash.create (10);
  d->edge_hash.create (10);
}

/* Releases function dictionary item D.  */

static void
func_dict_free (func_dict_t *d)
{
  d->source.release ();
  d->target.release ();

  d->decl_hash.dispose ();
  d->edge_hash.dispose ();
}

/* Releases memory for a semantic function F.  */

static inline void
sem_func_free (sem_func_t *f)
{
  unsigned int i;

  f->called_functions.release ();

  for (i = 0; i < f->bb_count; ++i)
    free (f->bb_sorted[i]);

  free (f->arg_types);
  free (f->bb_sizes);
  free (f->bb_sorted);
  free (f);
}

/* Basic block hash function combines for BASIC_BLOCK number of statements
   and number of edges.  */

static hashval_t
bb_hash (const void *basic_block)
{
  const sem_bb_t *bb = (const sem_bb_t *) basic_block;

  hashval_t hash = bb->nondbg_stmt_count;
  hash = iterative_hash_object (bb->edge_count, hash);

  return hash;
}

/* Module independent semantic equality computation function.  */

static hashval_t
independent_hash (sem_func_t *f)
{
  unsigned int i;
  hashval_t hash = 0;

  hash = iterative_hash_object (f->arg_count, hash);
  hash = iterative_hash_object (f->bb_count, hash);
  hash = iterative_hash_object (f->edge_count, hash);
  hash = iterative_hash_object (f->cfg_checksum, hash);

  for (i = 0; i < f->bb_count; ++i)
    hash = iterative_hash_object (f->bb_sizes[i], hash);

  for (i = 0; i < f->bb_count; ++i)
    hash = iterative_hash_object (f->bb_sorted[i]->hash, hash);

  return hash;
}

/* Checks two SSA names SSA1 and SSA2 from a different functions and
 * returns true if equal. Function dictionary D is equired for a correct
 * comparison.  */

static bool
func_dict_ssa_lookup (func_dict_t *d, tree ssa1, tree ssa2)
{
  unsigned i1, i2;

  i1 = SSA_NAME_VERSION (ssa1);
  i2 = SSA_NAME_VERSION (ssa2);

  if (d->source[i1] == -1)
    d->source[i1] = i2;
  else if (d->source[i1] != (int) i2)
    return false;

  if(d->target[i2] == -1)
    d->target[i2] = i1;
  else if (d->target[i2] != (int) i1)
    return false;

  return true;
}

/* In global context all known trees are visited
 * for given semantic function F.  */

static void
parse_semfunc_trees (sem_func_t *f)
{
  tree result;
  tree fnargs = DECL_ARGUMENTS (f->func_decl);
  unsigned int param_num = 0;

  f->arg_types = XNEWVEC (tree, f->arg_count);

  for (tree parm = fnargs; parm; parm = DECL_CHAIN (parm))
    f->arg_types[param_num++] = TYPE_CANONICAL (DECL_ARG_TYPE (parm));

  /* Function result type.  */
  result = DECL_RESULT (f->func_decl);

  if (result)
    f->result_type = TYPE_CANONICAL (TREE_TYPE (result));
  else
    f->result_type = NULL;
}

/* Semantic equality visit function loads all basic informations 
   about a function NODE and save them to a structure used for a further
   analysis. Successfull parsing fills F and returns true.  */

static bool
visit_function (struct cgraph_node *node, sem_func_t *f)
{
  tree fndecl, fnargs, parm, funcdecl;
  unsigned int param_num, nondbg_stmt_count, bb_count;
  struct function *func;
  gimple_stmt_iterator gsi;
  gimple stmt;
  basic_block bb;
  sem_bb_t *sem_bb;
  hashval_t gcode_hash, code;
  edge_iterator ei;
  edge e;

  fndecl = node->decl;
  func = DECL_STRUCT_FUNCTION (fndecl);

  f->called_functions.create (0);

  if (!func || !cgraph_function_with_gimple_body_p (node))
    return false;

  f->ssa_names_size = SSANAMES (func)->length ();
  f->node = node;

  f->func_decl = fndecl;
  f->region_tree = func->eh->region_tree;
  fnargs = DECL_ARGUMENTS (fndecl);

  /* iterating all function arguments.  */
  param_num = 0;
  for (parm = fnargs; parm; parm = DECL_CHAIN (parm))
    param_num++;

  f->arg_count = param_num;

  /* basic block iteration.  */
  f->bb_count = n_basic_blocks_for_fn (func) - 2;

  f->edge_count = n_edges_for_fn (func);
  f->bb_sizes = XNEWVEC (unsigned int, f->bb_count);

  f->bb_sorted = XNEWVEC (sem_bb_t *, f->bb_count);
  f->cfg_checksum = coverage_compute_cfg_checksum (func);

  bb_count = 0;
  FOR_EACH_BB_FN (bb, func)
    {
      nondbg_stmt_count = 0;
      gcode_hash = 0;

      for (ei = ei_start (bb->preds); ei_cond (ei, &e); ei_next (&ei))
        f->cfg_checksum = iterative_hash_host_wide_int (e->flags,
                                                        f->cfg_checksum);

      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
        {
          stmt = gsi_stmt (gsi);
          code = (hashval_t) gimple_code (stmt);

          /* We ignore all debug statements.  */
          if (code != GIMPLE_DEBUG)
          {
            nondbg_stmt_count++;
            gcode_hash = iterative_hash_object (code, gcode_hash);

            /* More precise hash could be enhanced by function call.  */
            if (code == GIMPLE_CALL)
            {
              funcdecl = gimple_call_fndecl (stmt);

              /* Function pointer variables are not support yet.  */
              if (funcdecl)
                f->called_functions.safe_push (funcdecl);
            }
          }
        }

      f->bb_sizes[bb_count] = nondbg_stmt_count;

      /* Inserting basic block to hash table.  */
      sem_bb = XNEW (sem_bb_t);
      sem_bb->bb = bb;
      sem_bb->func = f;
      sem_bb->nondbg_stmt_count = nondbg_stmt_count;
      sem_bb->edge_count = EDGE_COUNT (bb->preds) + EDGE_COUNT (bb->succs);
      sem_bb->hash = iterative_hash_object (gcode_hash, bb_hash (sem_bb));

      f->bb_sorted[bb_count++] = sem_bb;
    }

  return true;
}

/* Declaration comparer- global declarations are compared for a pointer equality,
   local one are stored in the function dictionary.  */

static bool
check_declaration (tree t1, tree t2, func_dict_t *d, tree func1, tree func2)
{
  decl_pair_t **slot;
  bool ret;
  decl_pair_t *decl_pair, *slot_decl_pair;

  decl_pair = XNEW (decl_pair_t);
  decl_pair->source = t1;
  decl_pair->target = t2;

  if (!auto_var_in_fn_p (t1, func1) || !auto_var_in_fn_p (t2, func2))
    {
      ret = t1 == t2; /* global variable declaration.  */
      SE_EXIT_DEBUG (ret);
    }

  if (!types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2)))
    SE_EXIT_FALSE ();

  slot = d->decl_hash.find_slot (decl_pair, INSERT);

  slot_decl_pair = (decl_pair_t *) *slot;

  if (slot_decl_pair)
    {
      ret = decl_pair->target == slot_decl_pair->target;
      free (decl_pair);

      SE_EXIT_DEBUG (ret);
    }
  else
    *slot = decl_pair;

  return true;
}

/* Function dictionary D compares if edges E1 and E2 correspond.
   Returns true if equal, false otherwise.  */

static bool
check_edges (edge e1, edge e2, func_dict_t *d)
{
  edge_pair_t **slot;
  bool r;
  edge_pair_t *edge_pair, *slot_edge_pair;

  if (e1->flags != e2->flags)
    return false;

  edge_pair = XNEW (edge_pair_t);
  edge_pair->source = e1;
  edge_pair->target = e2;

  slot = d->edge_hash.find_slot (edge_pair, INSERT);

  slot_edge_pair = (edge_pair_t *) *slot;

  if (slot_edge_pair)
    {
      r = edge_pair->target == slot_edge_pair->target;
      free (edge_pair);

      return r;
    }
  else
    *slot = edge_pair;

  return true;
}

/* Returns true if SSA names T1 and T2 do correspond in functions FUNC1 and
   FUNC2. Function dictionary D is responsible for a correspondence.  */

static bool
check_ssa_names (func_dict_t *d, tree t1, tree t2, tree func1,
                          tree func2)
{
  tree b1, b2;
  bool ret;

  if (!func_dict_ssa_lookup (d, t1, t2))
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
        case PARM_DECL:
        case RESULT_DECL:
          ret = check_declaration (b1, b2, d, func1, func2);

#if 0
          if (!ret && dump_file)
            {
              print_node (dump_file, "", b1, 3);
              print_node (dump_file, "", b2, 3);
            }
#endif

          SE_EXIT_DEBUG (ret);
        default:
          // TODO: remove after development
          gcc_unreachable ();
          return false;
        }
    }
  else
    return true;
}

/* Returns true if handled components T1 and T2 do correspond in functions
   FUNC1 and FUNC2. Handled components are decomposed and the function is
   called recursivelly for arguments.  */

static bool
compare_handled_component (tree t1, tree t2, func_dict_t *d,
                           tree func1, tree func2)
{
  tree base1, base2, x1, x2, y1, y2;
  HOST_WIDE_INT offset1, offset2;
  bool ret;

   /* TODO: We need to compare alias classes for loads & stores.
     We also need to care about type based devirtualization.  */
  if (!types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2)))
    SE_EXIT_FALSE_WITH_MSG ("");

  base1 = get_addr_base_and_unit_offset (t1, &offset1);
  base2 = get_addr_base_and_unit_offset (t2, &offset2);

  if (base1 && base2)
    {
      if (offset1 != offset2)
        SE_EXIT_FALSE_WITH_MSG ("");

      t1 = base1;
      t2 = base2;
    }

  if (TREE_CODE (t1) != TREE_CODE (t2))
    SE_EXIT_FALSE_WITH_MSG ("");

  switch (TREE_CODE (t1))
    {
    case ARRAY_REF:
    case ARRAY_RANGE_REF:
    {
      x1 = TREE_OPERAND (t1, 0);
      x2 = TREE_OPERAND (t2, 0);
      y1 = TREE_OPERAND (t1, 1);
      y2 = TREE_OPERAND (t2, 1);

      if (!compare_handled_component (array_ref_low_bound (t1),
				      array_ref_low_bound (t2),
				      d, func1, func2))
        SE_EXIT_FALSE_WITH_MSG ("");
      if (!compare_handled_component (array_ref_element_size (t1),
				      array_ref_element_size (t2),
				      d, func1, func2))
        SE_EXIT_FALSE_WITH_MSG ("");
      if (!compare_handled_component (x1, x2, d, func1, func2))
        SE_EXIT_FALSE_WITH_MSG ("");
      return compare_handled_component (y1, y2, d, func1, func2);
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
      if (flag_strict_aliasing)
        {
          alias_set_type s1 = get_deref_alias_set (TREE_TYPE (y1));
          alias_set_type s2 = get_deref_alias_set (TREE_TYPE (y2));

          if (s1 != s2)
            SE_EXIT_FALSE_WITH_MSG ("");
        }

      if (!compare_handled_component (x1, x2, d, func1, func2))
        SE_EXIT_FALSE_WITH_MSG ("");
      /* Type of the offset on MEM_REF does not matter.  */
      return tree_to_double_int (y1) == tree_to_double_int (y2);
    }
    case COMPONENT_REF:
    {
      x1 = TREE_OPERAND (t1, 0);
      x2 = TREE_OPERAND (t2, 0);
      y1 = TREE_OPERAND (t1, 1);
      y2 = TREE_OPERAND (t2, 1);

      ret = compare_handled_component (x1, x2, d, func1, func2)
	            && compare_handled_component (y1, y2, d, func1, func2);

      SE_EXIT_DEBUG (ret);
    }
    case ADDR_EXPR:
    {
      x1 = TREE_OPERAND (t1, 0);
      x2 = TREE_OPERAND (t2, 0);

      ret = compare_handled_component (x1, x2, d, func1, func2);
      SE_EXIT_DEBUG (ret);
    }
    case SSA_NAME:
    {
      ret = check_ssa_names (d, t1, t2, func1, func2);
      SE_EXIT_DEBUG (ret);
    }
    case INTEGER_CST:
    {
      ret = types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2))
              && tree_to_double_int (t1) == tree_to_double_int (t2);

      SE_EXIT_DEBUG (ret);
    }
    case STRING_CST:
    {
      ret = operand_equal_p (t1, t2, OEP_ONLY_CONST);
      SE_EXIT_DEBUG (ret);
    }
    case FUNCTION_DECL:
    case FIELD_DECL:
    {
      ret = t1 == t2;
      SE_EXIT_DEBUG (ret);
    }
    case VAR_DECL:
    case PARM_DECL:
    case LABEL_DECL:
    case RESULT_DECL:
    case CONST_DECL:
    case BIT_FIELD_REF:
    {
      ret = check_declaration (t1, t2, d, func1, func2);
      SE_EXIT_DEBUG (ret);
    }
    default:
      // TODO: remove after development
      debug_tree (t1);
      gcc_unreachable ();
      return false;
    }
}


/* Operand comparison function takes operand T1 from a function FUNC1 and
   compares it to a given operand T2 from a function FUNC2.  */

static bool
check_operand (tree t1, tree t2, func_dict_t *d, tree func1, tree func2)
{
  enum tree_code tc1, tc2;
  unsigned length1, length2, i;
  bool ret;

  if (t1 == NULL && t2 == NULL)
    return true;

  if (t1 == NULL || t2 == NULL)
    SE_EXIT_FALSE_WITH_MSG ("");

  tc1 = TREE_CODE (t1);
  tc2 = TREE_CODE (t2);

  if (tc1 != tc2)
    SE_EXIT_FALSE_WITH_MSG ("");

  switch (tc1)
    {
    case CONSTRUCTOR:
      length1 = vec_safe_length (CONSTRUCTOR_ELTS (t1));
      length2 = vec_safe_length (CONSTRUCTOR_ELTS (t2));

      if (length1 != length2)
        SE_EXIT_FALSE_WITH_MSG ("");

      for (i = 0; i < length1; i++)
        if (!check_operand (CONSTRUCTOR_ELT (t1, i)->value,
          CONSTRUCTOR_ELT (t2, i)->value, d, func1, func2))
            SE_EXIT_FALSE_WITH_MSG ("");

      return true;
    case VAR_DECL:
    case PARM_DECL:
    case LABEL_DECL:
      ret = check_declaration (t1, t2, d, func1, func2);
      SE_EXIT_DEBUG (ret);
    case SSA_NAME:
      ret = check_ssa_names (d, t1, t2, func1, func2);
      SE_EXIT_DEBUG (ret);
    case INTEGER_CST:
      ret = (types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2))
             && tree_to_double_int (t1) == tree_to_double_int (t2));
      SE_EXIT_DEBUG (ret);
    default:
      break;
    }

  if ((handled_component_p (t1) && handled_component_p (t1))
    || tc1 == ADDR_EXPR || tc1 == MEM_REF || tc1 == REALPART_EXPR
      || tc1 == IMAGPART_EXPR)
    ret = compare_handled_component (t1, t2, d, func1, func2);
  else /* COMPLEX_CST, VECTOR_CST compared correctly here.  */
    ret = operand_equal_p (t1, t2, OEP_ONLY_CONST);

#if 0
  if (!ret && dump_file)
    {
      print_node (dump_file, "\n", t1, 3);
      print_node (dump_file, "\n", t2, 3);
    }
#endif

  SE_EXIT_DEBUG (ret);
}

/* Call comparer takes statements S1 from a function FUNC1 and S2 from
   a function FUNC2. True is returned in case of call pointing to the
   same function, where all arguments and return type must be
   in correspondence.  */

static sem_func_t *
find_func_by_decl (tree decl);

static bool
check_gimple_call (gimple s1, gimple s2, func_dict_t *d, tree func1, tree func2)
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
      if (!check_operand (t1, t2, d, func1, func2))
        SE_EXIT_FALSE();
    }
  else
    if (find_func_by_decl (t1) == NULL &&
        find_func_by_decl (t2) == NULL && t1 != t2)
      return false;

  /* Checking of argument.  */
  for (i = 0; i < gimple_call_num_args (s1); ++i)
    {
      t1 = gimple_call_arg (s1, i);
      t2 = gimple_call_arg (s2, i);

      if (!check_operand (t1, t2, d, func1, func2))
        return false;
    }

  /* Return value checking.  */
  t1 = gimple_get_lhs (s1);
  t2 = gimple_get_lhs (s2);

  return check_operand (t1, t2, d, func1, func2);
}

/* Functions FUNC1 and FUNC2 are considered equal if assignment statements
   S1 and S2 contain all operands equal. Equality is checked by function
   dictionary D.  */

static bool
check_gimple_assign (gimple s1, gimple s2, func_dict_t *d, tree func1, tree func2)
{
  tree arg1, arg2;
  enum tree_code code1, code2;
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

    if (!check_operand (arg1, arg2, d, func1, func2))
      return false;
  }

  return true;
}

/* Returns true if conditions S1 coming from a function FUNC1 and S2 comming
   from FUNC2 do correspond. Collation is based on function dictionary D.  */

static bool
check_gimple_cond (gimple s1, gimple s2, func_dict_t *d, tree func1, tree func2)
{
  tree t1, t2;
  enum tree_code code1, code2;

  code1 = gimple_expr_code (s1);
  code2 = gimple_expr_code (s2);

  if (code1 != code2)
    return false;

  t1 = gimple_cond_lhs (s1);
  t2 = gimple_cond_lhs (s2);

  if (!check_operand (t1, t2, d, func1, func2))
    return false;

  t1 = gimple_cond_rhs (s1);
  t2 = gimple_cond_rhs (s2);

  return check_operand (t1, t2, d, func1, func2);
}

/* Returns true if labels T1 and T2 collate in functions FUNC1 and FUNC2.
   Function dictionary D is reposponsible for all correspondence checks.  */

static bool
check_tree_ssa_label (tree t1, tree t2, func_dict_t *d, tree func1, tree func2)
{
  return check_operand (t1, t2, d, func1, func2);
}

/* Returns true if labels G1 and G2 collate in functions FUNC1 and FUNC2.
   Function dictionary D is reposponsible for all correspondence checks.  */

static bool
check_gimple_label (gimple g1, gimple g2, func_dict_t *d, tree func1, tree func2)
{
  tree t1 = gimple_label_label (g1);
  tree t2 = gimple_label_label (g2);

  return check_tree_ssa_label (t1, t2, d, func1, func2);
}

/* Switch checking function takes switch statements G1 and G2 and process
   collation based on function dictionary D. All cases are compared separately,
   statements must come from functions FUNC1 and FUNC2.  */

static bool
check_gimple_switch (gimple g1, gimple g2, func_dict_t *d, tree func1, tree func2)
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

  if (!check_operand (t1, t2, d, func1, func2))
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

/* Return statements G1 and G2, comming from functions FUNC1 and FUNC2, are
   equal if types in function dictionary D do collate.  */

static bool
check_gimple_return (gimple g1, gimple g2, func_dict_t *d, tree func1, tree func2)
{
  tree t1, t2;

  t1 = gimple_return_retval (g1);
  t2 = gimple_return_retval (g2);

  /* Void return type.  */
  if (t1 == NULL && t2 == NULL)
    return true;
  else
    return check_operand (t1, t2, d, func1, func2);
}

/* Returns true if goto statements G1 and G2 are correspoding in function
   FUNC1, respectively FUNC2. Goto operands collation is based on function
   dictionary D.  */

static bool
check_gimple_goto (gimple g1, gimple g2, func_dict_t *d, tree func1, tree func2)
{
  tree dest1, dest2;

  dest1 = gimple_goto_dest (g1);
  dest2 = gimple_goto_dest (g2);

  if (TREE_CODE (dest1) != TREE_CODE (dest2) || TREE_CODE (dest1) != SSA_NAME)
    return false;

  return check_operand (dest1, dest2, d, func1, func2);
}

/* Returns true if resx gimples G1 and G2 are corresponding
   in both function. */

static bool
check_gimple_resx (gimple g1, gimple g2)
{
  return gimple_resx_region (g1) == gimple_resx_region (g2);
}

/* Returns for a given GSI statement first nondebug statement.  */

static void
gsi_next_nondebug_stmt (gimple_stmt_iterator &gsi)
{
  gimple s;

  s = gsi_stmt (gsi);

  while (gimple_code (s) == GIMPLE_DEBUG)
  {
    gsi_next (&gsi);
    gcc_assert (!gsi_end_p (gsi));

    s = gsi_stmt (gsi);
  }
}

static void
gsi_next_nonvirtual_phi (gimple_stmt_iterator &it)
{
  gimple phi;

  if (gsi_end_p (it))
    return;

  phi = gsi_stmt (it);
  gcc_assert (phi != NULL);

  while (virtual_operand_p (gimple_phi_result (phi)))
  {
    gsi_next (&it);

    if (gsi_end_p (it))
      return;

    phi = gsi_stmt (it);
  }
}

/* Basic block comparison for blocks BB1 and BB2 that are a part of functions
   FUNC1 and FUNC2 uses function dictionary D as a collation lookup data
   structure. All statements are iterated, type distinguished and
   a call of corresponding collation function is called. If any particular
   item does not equal, false is returned.  */

static bool
compare_bb (sem_bb_t *bb1, sem_bb_t *bb2, func_dict_t *d,
            tree func1, tree func2)
{
  unsigned i;
  gimple_stmt_iterator gsi1, gsi2;
  gimple s1, s2;

  if (bb1->nondbg_stmt_count != bb2->nondbg_stmt_count
      || bb1->edge_count != bb2->edge_count)
    SE_EXIT_FALSE ();

  gsi1 = gsi_start_bb (bb1->bb);
  gsi2 = gsi_start_bb (bb2->bb);

  for (i = 0; i < bb1->nondbg_stmt_count; i++)
  {
    gsi_next_nondebug_stmt (gsi1);
    gsi_next_nondebug_stmt (gsi2);

    s1 = gsi_stmt (gsi1);
    s2 = gsi_stmt (gsi2);

    if (gimple_code (s1) != gimple_code (s2))
      SE_EXIT_FALSE ();

    switch (gimple_code (s1))
      {
      case GIMPLE_CALL:
        if (!check_gimple_call (s1, s2, d, func1, func2))
          SE_DIFF_STATEMENT (s1, s2, "GIMPLE_CALL");
        break;
      case GIMPLE_ASSIGN:
        if (!check_gimple_assign (s1, s2, d, func1, func2))
          SE_DIFF_STATEMENT (s1, s2, "GIMPLE_ASSIGN");
        break;
      case GIMPLE_COND:
        if (!check_gimple_cond (s1, s2, d, func1, func2))
          SE_DIFF_STATEMENT (s1, s2, "GIMPLE_COND");
        break;
      case GIMPLE_SWITCH:
        if (!check_gimple_switch (s1, s2, d, func1, func2))
          SE_DIFF_STATEMENT (s1, s2, "GIMPLE_SWITCH");
        break;
      case GIMPLE_DEBUG:
      case GIMPLE_EH_DISPATCH:
        break;
      case GIMPLE_RESX:
        if (!check_gimple_resx (s1, s2))
          SE_DIFF_STATEMENT (s1, s2, "GIMPLE_RESX");
        break;
      case GIMPLE_LABEL:
        if (!check_gimple_label (s1, s2, d, func1, func2))
          SE_DIFF_STATEMENT (s1, s2, "GIMPLE_LABEL");
        break;
      case GIMPLE_RETURN:
        if (!check_gimple_return (s1, s2, d, func1, func2))
          SE_DIFF_STATEMENT (s1, s2, "GIMPLE_RETURN");
        break;
      case GIMPLE_GOTO:
        if (!check_gimple_goto (s1, s2, d, func1, func2))
          SE_DIFF_STATEMENT (s1, s2, "GIMPLE_GOTO");
        break;
      case GIMPLE_ASM:
        if (dump_file)
          {
            fprintf (dump_file, "Not supported gimple statement reached:\n");
            print_gimple_stmt (dump_file, s1, 0, TDF_DETAILS);
          }

        SE_EXIT_FALSE();
      default:
        debug_gimple_stmt (s1);
        gcc_unreachable ();
        return false;
      }

    gsi_next (&gsi1);
    gsi_next (&gsi2);
  }

  return true;
}

/* Returns true if basic blocks BB1 and BB2 have all phi nodes in collation,
   blocks are defined in functions FUNC1 and FUNC2 and partial operands are
   checked with help of function dictionary D.  */

static bool
compare_phi_nodes (basic_block bb1, basic_block bb2, func_dict_t *d,
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
    gsi_next_nonvirtual_phi (si1);
    gsi_next_nonvirtual_phi (si2);

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

      if (!check_operand (t1, t2, d, func1, func2))
        SE_EXIT_FALSE ();

      e1 = gimple_phi_arg_edge (phi1, i);
      e2 = gimple_phi_arg_edge (phi2, i);

      if (!check_edges (e1, e2, d))
        SE_EXIT_FALSE ();
    }

    gsi_next (&si2);
  }

  return true;
}

/* Returns true if an item at index SOURCE points to index TARGET.
   If SOURCE index is not filled, we insert TARGET index and return true.  */

static bool
bb_dict_test (int* bb_dict, int source, int target)
{
  if (bb_dict[source] == -1)
  {
    bb_dict[source] = target;
    return true;
  }
  else
    return bb_dict[source] == target;
}

/* Iterates all tree types in T1 and T2 and returns true if all types
   are compatible. */

static bool
compare_type_lists (tree t1, tree t2)
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

/* Returns true if both exception handindling trees are equal. */

static bool
compare_eh_regions (eh_region r1, eh_region r2, func_dict_t *d,
  tree func1, tree func2)
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

              if (!check_tree_ssa_label (t1, t2, d, func1, func2))
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
                  if (!check_tree_ssa_label (c1->label, c2->label,
                                             d, func1, func2))
                    return false;
                }
              else if (c1->label || c2->label)
                return false;

              /* Type list checking */
              if (!compare_type_lists (c1->type_list, c2->type_list))
                return false;

              c1 = c1->next_catch;
              c2 = c2->next_catch;
            }

          break;

        case ERT_ALLOWED_EXCEPTIONS:
          if (r1->u.allowed.filter != r2->u.allowed.filter)
            return false;

          if (!compare_type_lists (r1->u.allowed.type_list,
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

/* Main comparison called for semantic function struct F1 and F2 returns
   true if functions are considered semantically equal.  */

static bool
compare_functions (sem_func_t *f1, sem_func_t *f2)
{
  tree decl1, decl2;
  basic_block bb1, bb2;
  edge e1, e2;
  edge_iterator ei1, ei2;
  int *bb_dict = NULL;
  unsigned int i;
  func_dict_t func_dict;
  bool result = true;
  tree arg1, arg2;

  gcc_assert (f1->func_decl != f2->func_decl);

  if (f1->arg_count != f2->arg_count
      || f1->bb_count != f2->bb_count
      || f1->edge_count != f2->edge_count
      || f1->cfg_checksum != f2->cfg_checksum)
    SE_CF_EXIT_FALSE();

  /* Result type checking.  */
  if (f1->result_type != f2->result_type)
    SE_CF_EXIT_FALSE();

  /* Checking types of arguments.  */
  for (i = 0; i < f1->arg_count; ++i)
    if (!types_compatible_p (f1->arg_types[i], f2->arg_types[i]))
      SE_CF_EXIT_FALSE();

  /* Checking function arguments.  */
  decl1 = DECL_ATTRIBUTES (f1->node->decl);
  decl2 = DECL_ATTRIBUTES (f2->node->decl);

  while (decl1)
    {
      if (decl2 == NULL)
        SE_CF_EXIT_FALSE();

      if (get_attribute_name (decl1) != get_attribute_name (decl2))
        SE_CF_EXIT_FALSE();

      decl1 = TREE_CHAIN (decl1);
      decl2 = TREE_CHAIN (decl2);
    }

  if (decl1 != decl2)
    SE_CF_EXIT_FALSE();

  func_dict_init (&func_dict, f1->ssa_names_size, f2->ssa_names_size);
  for (arg1 = DECL_ARGUMENTS (f1->func_decl), arg2 = DECL_ARGUMENTS (f2->func_decl);
       arg1; arg1 = DECL_CHAIN (arg1), arg2 = DECL_CHAIN (arg2))
     check_declaration (arg1, arg2, &func_dict, f1->func_decl, f2->func_decl);

  /* Exception handling regions comparison.  */
  if (!compare_eh_regions (f1->region_tree, f2->region_tree,
                           &func_dict, f1->func_decl, f2->func_decl))
    SE_CF_EXIT_FALSE();

  /* Checking all basic blocks.  */
  for (i = 0; i < f1->bb_count; ++i)
    if(!compare_bb (f1->bb_sorted[i], f2->bb_sorted[i], &func_dict,
      f1->func_decl, f2->func_decl))
      {
        result = false;
        goto free_func_dict;
      }

  SE_DUMP_MESSAGE ("All BBs are equal\n");

  /* Basic block edges check.  */
  for (i = 0; i < f1->bb_count; ++i)
    {
      bb_dict = XNEWVEC (int, f1->bb_count + 2);
      memset (bb_dict, -1, (f1->bb_count + 2) * sizeof (int));

      bb1 = f1->bb_sorted[i]->bb;
      bb2 = f2->bb_sorted[i]->bb;

      ei2 = ei_start (bb2->preds);

      for (ei1 = ei_start (bb1->preds); ei_cond (ei1, &e1); ei_next (&ei1))
        {
          ei_cond (ei2, &e2);

          if (!bb_dict_test (bb_dict, e1->src->index, e2->src->index))
            {
              result = false;
              SE_DUMP_MESSAGE ("edge comparison returns false");
              goto free_bb_dict;
            }

          if (!bb_dict_test (bb_dict, e1->dest->index, e2->dest->index))
            {
              result = false;
              SE_DUMP_MESSAGE ("edge comparison returns false");
              goto free_bb_dict;
            }

          if (e1->flags != e2->flags)
            {
              result = false;
              SE_DUMP_MESSAGE ("edge comparison returns false");
              goto free_bb_dict;
            }

          if (!check_edges (e1, e2, &func_dict))
            {
              result = false;
              SE_DUMP_MESSAGE ("edge comparison returns false");
              goto free_bb_dict;
            }

          ei_next (&ei2);
        }
      }

  /* Basic block PHI nodes comparison.  */
  for (i = 0; i < f1->bb_count; ++i)
    if (!compare_phi_nodes (f1->bb_sorted[i]->bb, f2->bb_sorted[i]->bb,
        &func_dict, f1->func_decl, f2->func_decl))
    {
      result = false;
      SE_DUMP_MESSAGE ("PHI node comparison returns false");
    }

  free_bb_dict:
    free (bb_dict);

  free_func_dict:
    func_dict_free (&func_dict);

  exit_label:
    if (dump_file && (dump_flags & TDF_DETAILS))
      fprintf (dump_file, "compare_functions called for:%s:%s, result:%u\n\n",
               f1->node->name (),
               f2->node->name (),
               result);

    return result;
}

/* Two semantically equal function are merged.  */

static void
merge_functions (sem_func_t *original_func, sem_func_t *alias_func)
{
  struct cgraph_node *original = original_func->node;
  struct cgraph_node *local_original = original;
  struct cgraph_node *alias = alias_func->node;
  bool original_address_matters;
  bool alias_address_matters;

  bool create_thunk = false;
  bool create_alias = false;
  bool redirect_callers = false;
  bool original_discardable = false;

  if (dump_file)
    {
      fprintf (dump_file, "Semantic equality hit:%s->%s\n",
               original->name (), alias->name ());
       fprintf (dump_file, "Assembler function names:%s->%s\n",
               original->name (), alias->name ());
   }

  if (dump_file)
  {
    dump_function_to_file (original_func->func_decl, dump_file, TDF_DETAILS);
    dump_function_to_file (alias_func->func_decl, dump_file, TDF_DETAILS);
  }

  /* Do not attempt to mix functions from different user sections;
     we do not know what user intends with those.  */
  if (((DECL_SECTION_NAME (original->decl)
        && !DECL_HAS_IMPLICIT_SECTION_NAME_P (original->decl))
       || (DECL_SECTION_NAME (alias->decl)
           && !DECL_HAS_IMPLICIT_SECTION_NAME_P (alias->decl)))
      && DECL_SECTION_NAME (original->decl)
         != DECL_SECTION_NAME (alias->decl))
    {
      if (dump_file)
        fprintf (dump_file,
                 "Not unifying; original and alias are in different sections.\n\n");
      return;
    }

  /* See if original is in a section that can be discarded if the main
     symbol is not used.  */
  if (DECL_EXTERNAL (original->decl))
    original_discardable = true;
  if (original->resolution == LDPR_PREEMPTED_REG
      || original->resolution == LDPR_PREEMPTED_IR)
    original_discardable = true;
  if (DECL_ONE_ONLY (original->decl)
      && original->resolution != LDPR_PREVAILING_DEF
      && original->resolution != LDPR_PREVAILING_DEF_IRONLY
      && original->resolution != LDPR_PREVAILING_DEF_IRONLY_EXP)
    original_discardable = true;

#if 0
    /* If original can be discarded and replaced by an different (semantically
     equivalent) implementation, we risk creation of cycles from
     wrappers of equivalent functions.  Do not attempt to unify for now.  */
  if (original_discardable
      && (DECL_COMDAT_GROUP (original->decl)
	  != DECL_COMDAT_GROUP (alias->decl)))
    {
      if (dump_file)
        fprintf (dump_file, "Not unifying; risk of creation of cycle.\n\n");
      return;
    }
#endif

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
      create_alias = true;
      redirect_callers = false;
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

  if (create_thunk)
    {
      return;

      /* Preserve DECL_RESULT so we get right by reference flag.  */
      tree result = DECL_RESULT (alias->decl);

      /* Remove the function's body.  */
      cgraph_release_function_body (alias);
      cgraph_reset_node (alias);

      DECL_RESULT (alias->decl) = result;
      allocate_struct_function (alias_func->node->decl, false);
      set_cfun (NULL);

      /* Turn alias into thunk and expand it into GIMPLE representation.  */
      alias->definition = true;
      alias->thunk.thunk_p = true;
      cgraph_create_edge (alias, local_original,
                          NULL, 0, CGRAPH_FREQ_BASE);
      expand_thunk (alias, true);
      if (dump_file)
        fprintf (dump_file, "Thunk has been created.\n\n");
    }
  /* If the condtion above is not met, we are lucky and can turn the
     function into real alias.  */
  else if (create_alias)
    {
      /* Remove the function's body.  */
      cgraph_release_function_body (alias);
      cgraph_reset_node (alias);

      /* Create the alias.  */
      cgraph_create_function_alias (alias_func->func_decl, original_func->func_decl);
      symtab_resolve_alias (alias, original);
      if (dump_file)
        fprintf (dump_file, "Alias has been created.\n\n");
    }
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
	  cgraph_redirect_edge_call_stmt_to_callee (e);
	  pop_cfun ();
	  redirected = true;
        }
      if (dump_file && redirected)
        fprintf (dump_file, "Local calls have been redirected.\n\n");
    }
}

/* All functions that could be at the end pass considered to be equal
   are deployed to congruence classes. The algorithm for congruence reduction
   is based of finite-state machine minimalization with O(N log N).  */

/* Predefined structures.  */
struct cong_item;
struct cong_class;

/* Congruence use structure is used for a congruence item and
 * indicates all used items called as nth argument.  */
typedef struct cong_use
{
  unsigned int index;
  vec<struct cong_item *> usage;
} cong_use_t;

struct cong_use_var_hash: typed_noop_remove <cong_use_t>
{
  typedef cong_use_t value_type;
  typedef cong_use_t compare_type;
  static inline hashval_t hash (const value_type *);
  static inline int equal (const value_type *, const compare_type *);
};

/* Congruence use hash functions is simply based on index.  */

inline hashval_t
cong_use_var_hash::hash (const cong_use_t *item)
{
  return item->index;
}

/* Congruence use equal function is simply based on index.  */

inline int
cong_use_var_hash::equal (const cong_use_t *item1, const cong_use_t *item2)
{
  return item1->index == item2->index;
}

/* Congruence item.  */
typedef struct cong_item
{
  /* Global index.  */
  unsigned int index;
  /* Semantic function representation structure.  */
  sem_func_t *func;
  /* Congruence class the item belongs to.  */
  struct cong_class *parent_class;
  /* Bitmap of indeces where the item is used.  */
  bitmap usage_bitmap;
  /* Map of all use occurences: map<unsigned, vec<cong_item_t *>>.  */
  hash_table <cong_use_var_hash> usage;
  /* Total number of usage of the item.  */
  unsigned int usage_count;
} cong_item_t;

/* Congruence class.  */
typedef struct cong_class
{
  /* Global index.  */
  unsigned int index;
  /* All members of the group.  */
  vec <cong_item_t *> *members;
} cong_class_t;

/* Congruence class set structure.  */
struct cong_class_var_hash: typed_noop_remove <cong_class_t>
{
  typedef cong_class_t value_type;
  typedef cong_class_t compare_type;
  static inline hashval_t hash (const value_type *);
  static inline int equal (const value_type *, const compare_type *);
};

/* Hash for congruence class set is derived just from a pointer.  */

inline hashval_t
cong_class_var_hash::hash (const cong_class_t *item)
{
  return htab_hash_pointer (item);
}

/* Equal function compares pointer addresses.  */

inline int
cong_class_var_hash::equal (const cong_class_t *item1,
                            const cong_class_t *item2)
{
  return item1 == item2;
}

/* Global list of all congruence items.  */
static vec<cong_item_t *> congruence_items;

/* Global list of all congruence classes.  */
static vec<cong_class_t *> congruence_classes;

/* Global hash for fast tree to sem_func_t translation.  */
static pointer_map<sem_func_t *> tree_decl_map;

/* SOURCE item is used in cungruence item ITEM. The item is used
 * at position INDEX in that congruence item.  */

void
cong_usage_insert (struct cong_item *source, unsigned int index,
                 struct cong_item *item)
{
  cong_use_t **slot;
  cong_use_t *u = XNEW (cong_use_t);

  if (!source->usage.is_created())
    source->usage.create (2);

  u->index = index;
  slot = source->usage.find_slot (u, INSERT);

  if (!(*slot))
  {
    *slot = u;
    u->usage.create (2);
  }
  else
  {
    XDELETE (u);
    u = *slot;
  }

  u->usage.safe_push (item);

  bitmap_set_bit (source->usage_bitmap, index);
  source->usage_count++;
}

/* Returns vector of all usages of the ITEM. Congruence must occure
 * at the INDEX-th position.  */

vec <cong_item_t *> *
cong_use_find (cong_item_t *item, unsigned int index)
{
  cong_use_t use;
  cong_use_t *result;

  use.index = index;

  result = item->usage.find (&use);

  if (!result)
    return NULL;

  return &result->usage;
}

/* Congruence class dump.  */

/*
static void
dump_cong_classes (void)
{
  if (!dump_file)
    return;

  fprintf (dump_file, "\nCongruence classes dump\n");
  for (unsigned i = 0; i < congruence_classes.length (); ++i)
  {
    fprintf (dump_file, " class %u:\n", i);

    for (unsigned j = 0; j < congruence_classes[i]->members->length (); ++j)
    {
      cong_item_t *item = (*congruence_classes[i]->members)[j];
      fprintf (dump_file, "   %s (%u)\n", cgraph_node_name (item->func->node),
               item->index);

      for (hash_table <cong_use_var_hash>::iterator it = item->usage.begin ();
           it != item->usage.end (); ++it)
        {
          cong_use_t *use = &(*it);

          for (unsigned int k = 0; k < use->usage.length (); k++)
            {
              cong_item_t *item2 = use->usage[k];
              fprintf (dump_file, "     used in: %s (%u)\n",
                       cgraph_node_name (item2->func->node), use->index);
            }
        }
    }
  }
}
*/

/* After new congruence class C is created, we have to redirect
 * all members to the class.  */

static void
redirect_cong_item_parents (cong_class_t *c)
{
  for (unsigned int i = 0; i < c->members->length (); i++)
    (*c->members)[i]->parent_class = c;
}

/* New conguence item is compared to all existing groups if has the same
 * hash. If yes, the item is saved to existing group. Otherwise, we create
 * new congruence group and the item is assigned to that group.  */

static void
insert_cong_item_to_group (cong_item_t *f)
{
  cong_class_t *c;

  for (unsigned int j = 0; j < congruence_classes.length (); j++)
    {
      c = congruence_classes [j];

      if ((*c->members)[0]->func->hash == f->func->hash
          && compare_functions ((*c->members)[0]->func, f->func))
          {
            f->parent_class = c;
            (*c->members).safe_push (f);
            return;
          }
    }

  /* New group should be created.  */
  c = XCNEW (cong_class_t);
  c->index = congruence_classes.length ();
  c->members = XCNEW(vec<cong_item_t *>);
  c->members->create (2);
  c->members->safe_push (f);
  f->parent_class = c;

  congruence_classes.safe_push (c);
}

static void
build_tree_decl_map (void)
{
  bool existed_p;
  sem_func_t **slot;

  for (unsigned int i = 0; i < semantic_functions.length (); i++)
    {
      slot = tree_decl_map.insert (semantic_functions[i]->func_decl,
                                   &existed_p);

      gcc_assert (!existed_p);
      *slot = semantic_functions[i];
    }
}

/* Function declaration DECL is searched in a collection of all
 * semantic functions.  */

static sem_func_t *
find_func_by_decl (tree decl)
{
  sem_func_t **slot;

  slot = tree_decl_map.contains (decl);

  return slot == NULL ? NULL : *slot;
}

/* Congruence classes creation function. All existing semantic function
 * candidates are sorted to congruence classes according to hash value and
 * deep comparison.  */

static void
build_cong_classes (void)
{
  cong_item_t *item;

  congruence_classes.create (2);
  congruence_items.create (2);

  /* Cong item structure is allocated for each function.  */
  for (unsigned int i = 0; i < semantic_functions.length (); i++)
    {
      item = XCNEW (cong_item_t);
      item->index = i;
      item->func = semantic_functions[i];
      item->usage_bitmap = BITMAP_GGC_ALLOC ();
      item->usage.create (2);

      congruence_items.safe_push (item);
    }

  /* All cong items are placed to corresponding groups that are allocated.  */
  for (unsigned int i = 0; i < congruence_items.length (); i++)
    insert_cong_item_to_group (congruence_items [i]);

  /* Function usage is constructed.  */
  for (unsigned int i = 0; i < congruence_items.length (); i++)
    {
      item = congruence_items[i];

      for (unsigned int j = 0; j < item->func->called_functions.length (); j++)
        {
          sem_func_t *sf = find_func_by_decl (item->func->called_functions[j]);

          if (sf != NULL)
            {
              cong_item_t *item2 =
                congruence_items[sf->index];

              cong_usage_insert(item2, j, item);
            }
        }
    }
}

/* Adds class C to worklist for conguence reduction.  */

static void
add_to_worklist (hash_table<cong_class_var_hash> &worklist, cong_class_t *c)
{
  cong_class_t **result;

  result = worklist.find_slot (c, INSERT);

  if (*result)
    return;

  *result = c;
}

/* Basic structure monitoring usage of items in a group.  */
typedef struct cong_info_entry
{
  bitmap bm;
  unsigned int count;
} cong_info_entry_t;

typedef struct cong_info
{
  unsigned int index;
  cong_info_entry_t split[2];
} cong_info_t;

struct cong_info_var_hash: typed_noop_remove <cong_info_t>
{
  typedef cong_info_t value_type;
  typedef cong_info_t compare_type;
  static inline hashval_t hash (const value_type *);
  static inline int equal (const value_type *, const compare_type *);
  static inline void remove (value_type *);
};

inline hashval_t
cong_info_var_hash::hash (const cong_info_t *item)
{
  return item->index;
}

/* Equal function compares pointer addresses.  */

inline int
cong_info_var_hash::equal (const cong_info_t *item1, const cong_info_t *item2)
{
  return item1->index == item2->index;
}

inline void
cong_info_var_hash::remove (cong_info_t *item)
{
  for (unsigned int i = 0; i < 2; i++)
    BITMAP_FREE (item->split[i].bm);

  free (item);
}

static cong_info_t *
find_info_for_index (hash_table<cong_info_var_hash> &htable,
                     unsigned int index, bitmap_obstack &bmstack)
{
  cong_info_t **slot;
  cong_info_t info;
  info.index = index;

  slot = htable.find_slot (&info, INSERT);

  if (*slot)
    return *slot;
  else
    {
      cong_info_t *new_info = XNEW (cong_info_t);
      new_info->index = index;

      for (unsigned int i = 0; i < 2; i++)
        {
          new_info->split[i].count = 0;
          new_info->split[i].bm = BITMAP_ALLOC (&bmstack);
        }

      *slot = new_info;

      return new_info;
    }
}

/* We iterate all members of congruence class C and mark all groups that
 * use as INDEX-th item a congruence item from C. Splitted groups are added
 * to WORKLIST.  */

static bool
do_cong_step_for_index (cong_class *c, unsigned int index,
                        hash_table<cong_class_var_hash> &worklist,
                        bitmap_obstack &bmstack)
{
  cong_use_t *use;
  bool result = false;

  hash_table<cong_info_var_hash> info_hash;
  info_hash.create (4);

  for (unsigned int i = 0; i < c->members->length (); i++)
    {
      cong_use_t u;
      u.index = index;

      use = (*c->members)[i]->usage.find (&u);
      if (use)
        for (unsigned int j = 0; j < use->usage.length (); j++)
          {
            cong_item_t *source = use->usage[j];
            cong_class_t *sc = source->parent_class;
            unsigned int target = sc == c ? 0 : 1;

            cong_info_t *info = find_info_for_index (info_hash, sc->index,
                                                     bmstack);

            bitmap_set_bit (info->split[target].bm, source->index);
            info->split[target].count++;
          }
    }

  /* New split for each existing groups is tested.  */
  for (hash_table<cong_info_var_hash>::iterator it = info_hash.begin ();
       it != info_hash.end (); ++it)
  {
    cong_info_entry_t *entry = NULL;
    cong_info_t *info = &(*it);
    unsigned int ci = info->index;

    for (unsigned int i = 0; i < 2; i++)
      if (info->split[i].count > 0
          && info->split[i].count < congruence_classes[ci]->members->length ())
        {
          entry = &info->split[i];
          break;
        }

    if (entry)
      {
        unsigned int small, large;

        unsigned int usage_count[2];
        vec<cong_item_t *> *new_members[2];

        for (unsigned int j = 0; j < 2; j++)
          {
            usage_count[j] = 0;
            new_members[j] = XNEW (vec<cong_item_t *>);
            new_members[j]->create (2);
          }

        for (unsigned int j = 0;
             j < congruence_classes[ci]->members->length (); j++)
          {
            unsigned int c = bitmap_bit_p (entry->bm,
              (*congruence_classes[ci]->members)[j]->index) ? 0 : 1;

            usage_count[c] += (*congruence_classes[ci]->members)[j]->usage_count;
            new_members[c]->safe_push ((*congruence_classes[ci]->members)[j]);
          }

        gcc_assert (new_members[0]->length () > 0);
        gcc_assert (new_members[1]->length () > 0);

        small = usage_count[0] < usage_count[1] ? 0 : 1;
        large = small == 0 ? 1 : 0;

        /* Existing group is replaced with new member list.  */
        congruence_classes[ci]->members->release ();
        XDELETE (congruence_classes[ci]->members);

        congruence_classes[ci]->members = new_members[small];

        /* New group is created and added to list of congruent classes.  */
        cong_class_t *newclass = XCNEW (cong_class_t);
        newclass->index = congruence_classes.length ();
        newclass->members = new_members[large];

        redirect_cong_item_parents (newclass);
        congruence_classes.safe_push (newclass);

        add_to_worklist (worklist, small == 0
          ? congruence_classes[ci] : congruence_classes.last ());

        result = true;
      }
  }

  info_hash.dispose ();

  return result;
}

/* Congruence class C from WORKLIST could cause a split in the list
 * of existing groups.  */

static void
process_congruence_step (hash_table<cong_class_var_hash> &worklist,
                         cong_class *c, bitmap_obstack &bmstack)
{
  bitmap_iterator bi;
  unsigned int i;

  bitmap usage = BITMAP_ALLOC (&bmstack);

  for (unsigned int i = 0; i < c->members->length (); i++)
    bitmap_ior_into (usage, (*c->members)[i]->usage_bitmap);

  EXECUTE_IF_SET_IN_BITMAP (usage, 0, i, bi)
    {
      do_cong_step_for_index (c, i, worklist, bmstack);
    }

  BITMAP_FREE (usage);
}

/* Congruence reduction execution function.  */

static void
process_congruence_reduction (void)
{
  bitmap_obstack bmstack;

  bitmap_obstack_initialize (&bmstack);

  hash_table<cong_class_var_hash> worklist;
  worklist.create (congruence_classes.length ());

  for (unsigned int i = 0; i < congruence_classes.length (); i++)
    add_to_worklist (worklist, congruence_classes[i]);

  while (worklist.elements ())
    {
      cong_class_t *c = &(*worklist.begin ());
      worklist.remove_elt (c);

      process_congruence_step (worklist, c, bmstack);
    }

  worklist.dispose ();

  bitmap_obstack_release (&bmstack);
}

/* Function iterates all congruence classes and merges all
 * candidates that were proved to be samntically equivalent.
 * If you add dump option, statististcs are showed.  */

static void
merge_groups (unsigned int groupcount_before)
{
  cong_class_t *c;
  sem_func_t *f1, *f2;
  unsigned int func_count = semantic_functions.length ();
  unsigned int groupcount_after = congruence_classes.length ();
  unsigned int fcount = semantic_functions.length ();
  unsigned int equal = congruence_items.length ()
                       - congruence_classes.length ();

  if (dump_file)
    {
      fprintf (dump_file, "\nFunction count: %u\n", func_count);
      fprintf (dump_file, "Congruent classes before: %u, after: %u\n",
               groupcount_before, groupcount_after);
      fprintf (dump_file, "Average class size before: %.2f, after: %.2f\n",
               1.0f * fcount / groupcount_before,
               1.0f * fcount / groupcount_after);
      fprintf (dump_file, "Equal functions: %u\n", equal);
      fprintf (dump_file, "Fraction of visited functions: %.2f%%\n\n",
               100.0f * equal / fcount);
    }

  for (unsigned int i = 0; i < congruence_classes.length (); i++)
    {
      c = congruence_classes[i];

      if (c->members->length () == 1)
        continue;

      f1 = (*c->members)[0]->func;

      for (unsigned int j = 1; j < c->members->length (); j++)
        {
          f2 = (*c->members)[j]->func;

          merge_functions (f1, f2);
        }
    }
}

/* Memory release for all data structures connected to congruence reduction.  */

static void
congruence_clean_up (void)
{
  for (unsigned int i = 0; i < congruence_classes.length (); i++)
    XDELETE (congruence_classes[i]);

  congruence_classes.release ();

  for (unsigned int i = 0; i < congruence_items.length (); i++)
    {
      congruence_items[i]->usage.dispose ();
      XDELETE (congruence_items[i]);
    }

  congruence_items.release ();
}

/* IPA semantic equality pass entry point.  */

static void
visit_all_functions (void)
{
  struct cgraph_node *node;
  sem_func_t *f;

  FOR_EACH_DEFINED_FUNCTION (node)
    {
      f = XNEW (sem_func_t);

      if (visit_function (node, f))
        {
          f->index = semantic_functions.length ();
          semantic_functions.safe_push (f);
        }
      else
        XDELETE (f);
    }
}

/* Semantic equality exection function.  */

static unsigned int
semantic_equality (void)
{
  sem_func_t *f;
  unsigned int groupcount;

  /* Semantic equality pass will be rewritten to a normal IPA pass, so that
   * all following steps are grouped to future pass phases: LGEN, WPA and
   * LTRANS.  */

  /* LGEN phase: all functions are visited and independent is computed.  */

  semantic_functions.create (16);
  visit_all_functions ();
  build_tree_decl_map ();

  for (unsigned int i = 0; i < semantic_functions.length (); i++)
    semantic_functions[i]->hash = independent_hash (semantic_functions[i]);

  /* WPA phase: trees are merged, we can compare function declarations
   * and result type.  */

  for (unsigned int i = 0; i < semantic_functions.length (); i++)
    parse_semfunc_trees (semantic_functions[i]);

  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      for (unsigned int i = 0; i < semantic_functions.length (); i++)
        {
          f = semantic_functions[i];
          fprintf (dump_file, "Visited function: %s, with hash: %u\n",
                   f->node->name (), f->hash);
        }

      fprintf (dump_file, "\n");
    }

  /* LTRANS phase: functions are splitted to groups according to deep
   * equality comparison. Last step is congruence calculation.  */

  /* List of all congruent functions is constructed.  */
  build_cong_classes ();

  /* Amount of groups before contruence reductions is started */
  groupcount = congruence_classes.length ();

  /* Main congruence execution function.  */
  process_congruence_reduction ();
 
  /* All functions that are in the same groups could be merged.  */
  merge_groups (groupcount);

  /* Release of all data structures connected to contruence algorithm.  */
  congruence_clean_up ();

  for (unsigned int i = 0; i < semantic_functions.length (); i++)
    sem_func_free (semantic_functions[i]);

  semantic_functions.release ();

  return 0;
}

/* IPA pass gate function.  */

static bool
gate_sem_equality (void)
{
  return flag_ipa_sem_equality;
}

namespace {

const pass_data pass_data_ipa_sem_equality =
{
  SIMPLE_IPA_PASS,
  "sem-equality",           /* name */
  OPTGROUP_IPA,             /* optinfo_flags */
  true,                     /* has_gate */
  true,                     /* has_execute */
  TV_IPA_SEM_EQUALITY,      /* tv_id */
  0,                        /* properties_required */
  0,                        /* properties_provided */
  0,                        /* properties_destroyed */
  0,                        /* todo_flags_start */
  0,                        /* todo_flags_finish */
};

class pass_ipa_sem_equality : public simple_ipa_opt_pass
{
public:
  pass_ipa_sem_equality(gcc::context *ctxt)
    : simple_ipa_opt_pass(pass_data_ipa_sem_equality, ctxt)
  {}

  /* opt_pass methods: */
  bool gate () { return gate_sem_equality (); }
  unsigned int execute () { return semantic_equality(); }
}; // class pass_ipa_sem_equality

} // anon namespace

simple_ipa_opt_pass *
make_pass_ipa_sem_equality (gcc::context *ctxt)
{
  return new pass_ipa_sem_equality (ctxt);
}
