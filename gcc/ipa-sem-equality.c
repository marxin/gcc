#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "tree-flow.h"
#include "tree-pass.h"
#include "tree-dump.h"
#include "langhooks.h"
#include "gimple.h"
#include "cgraph.h"
#include "cfgloop.h"
#include "tree-ssa-sccvn.h"
#include "coverage.h"
#include "hash-table.h"

#define IPA_SEM_EQUALITY_DEBUG

/* Forward struct declaration */
typedef struct sem_bb sem_bb_t;
typedef struct sem_func sem_func_t;

/* Function struct for sematic equality pass */
typedef struct sem_func
{
  struct cgraph_node *node;
  tree func_decl;
  tree result_type;
  tree *arg_types;
  unsigned int arg_count;
  unsigned int bb_count;
  unsigned int edge_count;
  unsigned int *bb_sizes;
  unsigned cfg_checksum;
  unsigned ssa_names_size;
  sem_bb_t **bb_sorted;
  sem_func_t *next;
} sem_func_t;

/* Basic block struct for sematic equality pass. */
typedef struct sem_bb
{
  basic_block bb;
  sem_func_t *func;
  unsigned stmt_count;
  unsigned edge_count;
  hashval_t hashcode;
} sem_bb_t;

/* Tree declaration used for hash table purpose. */
typedef struct decl_pair
{
  tree source;
  tree target;
} decl_pair_t;

/* Call graph edge pair used for hash table purpose. */
typedef struct edge_pair
{
  edge source;
  edge target;
} edge_pair_t;

static sem_func_t **sem_functions;
static unsigned int sem_function_count;

/* Hash combine function. */

static
hashval_t iterative_hash_tree (void *ptr, hashval_t hash)
{
  uintptr_t pointer = (uintptr_t) ptr;
  hashval_t *h = (hashval_t *) &pointer;

  for (unsigned i = 0; i < sizeof (uintptr_t) / sizeof (hashval_t); ++i)
    hash = iterative_hash_object (h[i], hash);

  return hash;
}

struct decl_var_hash: typed_noop_remove <decl_pair_t>
{
  typedef decl_pair_t value_type;
  typedef decl_pair_t compare_type;
  static inline hashval_t hash (const value_type *);
  static inline int equal (const value_type *, const compare_type *);  
};

inline hashval_t
decl_var_hash::hash (const decl_pair_t *pair)
{
  return iterative_hash_tree (pair->source, 0);
}

inline int
decl_var_hash::equal (const decl_pair_t *pair1, const decl_pair_t *pair2)
{
  return pair1->source == pair2->source;
}

struct edge_var_hash: typed_noop_remove <edge_pair_t>
{
  typedef edge_pair_t value_type;
  typedef edge_pair_t compare_type;
  static inline hashval_t hash (const value_type *);
  static inline int equal (const value_type *, const compare_type *);  
};

inline hashval_t
edge_var_hash::hash (const edge_pair_t *pair)
{
  return iterative_hash_tree (pair->source, 0);
}

inline int
edge_var_hash::equal (const edge_pair_t *pair1, const edge_pair_t *pair2)
{
  return pair1->source == pair2->source;
}

/* Struct used for all kind of function dictionaries like
   SSA names, call graph edges and all kind of declarations.*/

typedef struct func_dict
{
  int *source;
  int *target;
  hash_table <decl_var_hash> decl_hash;
  hash_table <edge_var_hash> edge_hash;
} func_dict_t;


/* Function dictionary initializer. */

static void
func_dict_init (func_dict_t *d, unsigned ssa_names_size1,
  unsigned ssa_names_size2) 
{
  d->source = XNEWVEC (int, ssa_names_size1);
  d->target = XNEWVEC (int, ssa_names_size2);

  memset (d->source, -1, ssa_names_size1 * sizeof (int));
  memset (d->target, -1, ssa_names_size2 * sizeof (int));

  d->decl_hash.create (10);
  d->edge_hash.create (10);
}

/* Free memory handler for a function dictionary.  */

static void
func_dict_free (func_dict_t *d)
{
  free (d->source);
  free (d->target);

  d->decl_hash.dispose ();
  d->edge_hash.dispose ();
}

struct sem_func_var_hash: typed_noop_remove <sem_func_t>
{
  typedef sem_func_t value_type;
  typedef sem_func_t compare_type;
  static inline hashval_t hash (const value_type *);
  static inline int equal (const value_type *, const compare_type *);  
  static inline void remove (value_type *);
};

inline hashval_t
sem_func_var_hash::hash (const sem_func_t *f)
{
  unsigned int i;

  hashval_t hash = 0;
  
  hash = iterative_hash_object (f->arg_count, hash);
  hash = iterative_hash_object (f->bb_count, hash);
  hash = iterative_hash_object (f->edge_count, hash);
  hash = iterative_hash_object (f->cfg_checksum, hash);

  for(i = 0; i < f->arg_count; ++i)
    hash = iterative_hash_tree (f->arg_types[i], hash);

  hash = iterative_hash_tree (f->result_type, hash);

  for(i = 0; i < f->bb_count; ++i)
    hash = iterative_hash_object (f->bb_sizes[i], hash);

  return hash;
}

inline int
sem_func_var_hash::equal (const value_type *f1, const compare_type *f2)
{
  return sem_func_var_hash::hash (f1) == sem_func_var_hash::hash (f2);
}

inline void
sem_func_var_hash::remove (value_type *f)
{
  unsigned int i;

  for (i = 0; i < f->bb_count; ++i)
    free (f->bb_sorted[i]);

  free (f->arg_types);
  free (f->bb_sizes);
  free (f->bb_sorted);
  free (f);
}

/* Simple basic block hash computing function. */

static hashval_t
bb_hash (const void *basic_block)
{
  const sem_bb_t *bb = (const sem_bb_t *) basic_block;

  hashval_t hash = bb->stmt_count;
  hash = iterative_hash_object (bb->edge_count, hash);

  return hash;
}

/* Checks two SSA names from a different functions and returns true
   if equal. */

static bool
func_dict_ssa_look_up (func_dict_t *d, tree ssa1, tree ssa2)
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

/* Semantic equality visit function loads all basic informations 
   about a function and save them to a structure used for a further analysis */

static bool
visit_function (struct cgraph_node *node, sem_func_t *f)
{
  tree fndecl, fnargs, parm, result;
  unsigned int param_num, gimple_count, bb_count;
  struct function *my_function;
  gimple_stmt_iterator gsi;
  gimple stmt;
  basic_block bb;
  sem_bb_t *sem_bb;
  hashval_t gcode_hash, code;

  fndecl = node->symbol.decl;    
  my_function = DECL_STRUCT_FUNCTION (fndecl);

  /* TODO: do a deep analysis what kind of functions do such a behaviour */
  if (!my_function || !my_function->gimple_df)
    return false;

  /* variadic functions are ignored */
  if (stdarg_p (TREE_TYPE (fndecl)))
    return false;

  f->ssa_names_size = SSANAMES (my_function)->length ();
  f->node = node;
  f->func_decl = fndecl;
  fnargs = DECL_ARGUMENTS (fndecl);

  /* iterating all function arguments */
  param_num = 0;
  for (parm = fnargs; parm; parm = DECL_CHAIN (parm))
    param_num++;

  f->arg_count = param_num;
  f->arg_types = XNEWVEC (tree, param_num);

  param_num = 0;
  for (parm = fnargs; parm; parm = DECL_CHAIN (parm))
    f->arg_types[param_num++] = gimple_register_canonical_type
      (DECL_ARG_TYPE (parm));

  /* Function result type */
  result = DECL_RESULT (fndecl);

  if (result)
    f->result_type = gimple_register_canonical_type
      (TREE_TYPE (DECL_RESULT (fndecl)));

  /* basic block iteration */
  f->bb_count = n_basic_blocks_for_function (my_function) - 2;

  f->edge_count = n_edges_for_function (my_function);
  f->bb_sizes = XNEWVEC (unsigned int, f->bb_count);

  f->bb_sorted = XNEWVEC (sem_bb_t *, f->bb_count);
  f->cfg_checksum = coverage_compute_cfg_checksum_fn (my_function);

  bb_count = 0;
  FOR_EACH_BB_FN (bb, my_function)
    {
      gimple_count = 0;
      gcode_hash = 0;

      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
        {
          gimple_count++;
          stmt = gsi_stmt (gsi);
          code = (hashval_t) gimple_code (stmt);
          gcode_hash = iterative_hash_object (code, gcode_hash);
        }

      f->bb_sizes[bb_count] = gimple_count;

      /* Inserting basic block to hash table */
      sem_bb = XNEW (sem_bb_t);
      sem_bb->bb = bb;
      sem_bb->func = f;
      sem_bb->stmt_count = gimple_count;
      sem_bb->edge_count = EDGE_COUNT (bb->preds) + EDGE_COUNT (bb->succs);
      sem_bb->hashcode = iterative_hash_object (gcode_hash, bb_hash (sem_bb));

      f->bb_sorted[bb_count++] = sem_bb;
    }

  return true;
}

/* Declaration comparer- global declarations are comparer for a pointer equality,
   local one are stored in the function dictionary. */

static bool
check_declaration (tree t1, tree t2, func_dict_t *d, tree func1, tree func2)
{
  decl_pair_t **slot;
  bool r;
  decl_pair_t *decl_pair, *slot_decl_pair;

  decl_pair = XNEW (decl_pair_t);
  decl_pair->source = t1;
  decl_pair->target = t2;

  if (!auto_var_in_fn_p (t1, func1) || !auto_var_in_fn_p (t2, func2))
    return t1 == t2; /* global variable declaration */

  slot = d->decl_hash.find_slot (decl_pair, INSERT);

  slot_decl_pair = (decl_pair_t *) *slot;

  if (slot_decl_pair)
    {
      r = decl_pair->target == slot_decl_pair->target;
      free (decl_pair);

      return r;
    }
  else
    *slot = decl_pair;

  return true;
}

/* Edge comparer- we just build edge bidictionary for each visited edge. */

static bool
check_edges (edge e1, edge e2, func_dict_t *d)
{
  edge_pair_t **slot;
  bool r;
  edge_pair_t *edge_pair, *slot_edge_pair;

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

/* Comparer responsible for a comparison of either SSA names or
   a declaration (variable or parameter). */

static bool
function_check_names (func_dict_t *d, tree t1, tree t2, tree func1, tree func2)
{
  tree b1, b2;

  if (!func_dict_ssa_look_up (d, t1, t2))
    return false;

  b1 = SSA_NAME_VAR (t1);
  b2 = SSA_NAME_VAR (t2);

  if (b1 == NULL && b2 == NULL)
    return true;

  if (b1 == NULL || b2 == NULL || TREE_CODE (b1) != TREE_CODE (b2))
    return false;

  switch (TREE_CODE (b1))
    {
    case VAR_DECL:
    case PARM_DECL:
      return check_declaration (b1, b2, d, func1, func2);
      break;
    default:
      return false;
    }
}

/* Each handled componend is decomposed for arguments that are
   recursivelly compared. */

static bool
compare_handled_component (tree t1, tree t2, func_dict_t *d,
  tree func1, tree func2)
{
  tree base1, base2, x1, x2, y1, y2;
  HOST_WIDE_INT offset1, offset2;
  
  base1 = get_addr_base_and_unit_offset (t1, &offset1);
  base2 = get_addr_base_and_unit_offset (t2, &offset2);

  if (base1 && base2)
    {
      if (offset1 != offset2)
        return false;

      t1 = base1;
      t2 = base2;
    }
  
  if (TREE_CODE (t1) != TREE_CODE (t2))
    return false;

  switch (TREE_CODE (t1))
    {
    case ARRAY_REF:
    case ARRAY_RANGE_REF:
    case COMPONENT_REF:
    case MEM_REF:
    {
      x1 = TREE_OPERAND (t1, 0);
      x2 = TREE_OPERAND (t2, 0);
      y1 = TREE_OPERAND (t1, 1);
      y2 = TREE_OPERAND (t2, 1);

      return compare_handled_component (x1, x2, d, func1, func2) &&
        compare_handled_component (y1, y2, d, func1, func2);
    }
    case ADDR_EXPR:
    {
      x1 = TREE_OPERAND (t1, 0);
      x2 = TREE_OPERAND (t2, 0);
      return compare_handled_component (x1, x2, d, func1, func2);
    }
    case INTEGER_CST:
      return operand_equal_p (t1, t2, OEP_ONLY_CONST);
    case SSA_NAME:
      return function_check_names (d, t1, t2, func1, func2);
    case FUNCTION_DECL:
    case FIELD_DECL:
      return t1 == t2;
    case VAR_DECL:
    case LABEL_DECL:
      return check_declaration (t1, t2, d, func1, func2);
    default:
      return false;
    }
}

/* Operand comparer could handle a tree operand. */

static bool
check_var_operand (tree t1, tree t2, func_dict_t *d, tree func1, tree func2)
{
  enum tree_code tc1, tc2;

  if (t1 == NULL && t2 == NULL)
    return true;

  if (t1 == NULL || t2 == NULL)
    return false;

  tc1 = TREE_CODE (t1);
  tc2 = TREE_CODE (t2);

  if (tc1 != tc2)
    return false;

  switch (tc1)
    {
    case CONSTRUCTOR:
      return true;
    case VAR_DECL:
    case LABEL_DECL:
      return check_declaration (t1, t2, d, func1, func2);
    case SSA_NAME:
      return function_check_names (d, t1, t2, func1, func2); 
    default:
      break;
    }

  if ((handled_component_p (t1) && handled_component_p (t1))
    || tc1 == ADDR_EXPR || tc1 == MEM_REF || tc1 == REALPART_EXPR
      || tc1 == IMAGPART_EXPR)
    return compare_handled_component (t1, t2, d, func1, func2);
  else /* COMPLEX_CST compared correctly here */
    return operand_equal_p (t1, t2, OEP_ONLY_CONST);
}

/* Function comparer- all function arguments including a return type
   are checked and we support only calls pointing to the save
   function. */

static bool
check_ssa_call (gimple s1, gimple s2, func_dict_t *d, tree func1, tree func2)
{
  unsigned i;
  tree t1, t2;

  if (gimple_call_num_args (s1) != gimple_call_num_args (s2))
    return false;

  if (!gimple_call_same_target_p (s1, s2))
    return false;

  /* argument checking */
  for (i = 0; i < gimple_call_num_args (s1); ++i)
    { 
      t1 = gimple_call_arg (s1, i);
      t2 = gimple_call_arg (s2, i);

      if (!check_var_operand (t1, t2, d, func1, func2))
        return false;
    }

  /* return value checking */
  t1 = gimple_get_lhs (s1);
  t2 = gimple_get_lhs (s2);

  if (t1 == NULL_TREE && t2 == NULL_TREE)
    return true;
  else
    return check_var_operand (t1, t2, d, func1, func2);
}

/* Assignment comparer- all parts of expression are checked. */

static bool
check_ssa_assign (gimple s1, gimple s2, func_dict_t *d, tree func1, tree func2)
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

    if (!check_var_operand (arg1, arg2, d, func1, func2))
      return false;
  }

  return true;
}

/* Condition comparer- conditions with swapped arguments are not
   supported yet.  */

static bool
check_ssa_cond (gimple s1, gimple s2, func_dict_t *d, tree func1, tree func2)
{
  tree t1, t2;
  enum tree_code code1, code2;

  code1 = gimple_expr_code (s1);
  code2 = gimple_expr_code (s2);

  if (code1 != code2)
    return false;

  t1 = gimple_cond_lhs (s1);
  t2 = gimple_cond_lhs (s2);

  if (!check_var_operand (t1, t2, d, func1, func2))
    return false;

  t1 = gimple_cond_rhs (s1);
  t2 = gimple_cond_rhs (s2);

  return check_var_operand (t1, t2, d, func1, func2);
}

/* Label comparer. */

static bool
check_ssa_label (gimple g1, gimple g2, func_dict_t *d, tree func1, tree func2)
{
  tree label1, label2;

  label1 = gimple_label_label (g1);
  label2 = gimple_label_label (g2);

  return check_var_operand (label1, label2, d, func1, func2);
}

/* Switch comparer. */

static bool
check_ssa_switch (gimple g1, gimple g2, func_dict_t *d, tree func1, tree func2)
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

  if (!check_var_operand (t1, t2, d, func1, func2))
    return false;

  for (i = 0; i < lsize1; i++)
  {
    low1 = CASE_LOW (gimple_switch_label (g1, i));
    low2 = CASE_LOW (gimple_switch_label (g2, i));

    if ((low1 != NULL) ^ (low2 != NULL) ||
      (low1 && low2 && TREE_INT_CST_LOW (low1) != TREE_INT_CST_LOW (low2)))
        return false;

    high1 = CASE_HIGH (gimple_switch_label (g1, i));
    high2 = CASE_HIGH (gimple_switch_label (g2, i));

    if ((high1 != NULL) ^ (high2 != NULL) ||
      (high1 && high2 && TREE_INT_CST_LOW (high1) != TREE_INT_CST_LOW (high2)))
        return false;
  }

  return true;
}

/* Return comparer. */

static bool
check_ssa_return (gimple g1, gimple g2, func_dict_t *d, tree func1, tree func2)
{
  tree t1, t2;

  t1 = gimple_return_retval (g1);
  t2 = gimple_return_retval (g2);

  /* void return type */
  if (t1 == NULL && t2 == NULL)
    return true;
  else
    return check_var_operand (t1, t2, d, func1, func2);
}

/* Goto comparer */

static bool
check_ssa_goto (gimple g1, gimple g2, func_dict_t *d, tree func1, tree func2)
{
  tree dest1, dest2;

  dest1 = gimple_goto_dest (g1);
  dest2 = gimple_goto_dest (g2);

  if (TREE_CODE (dest1) != TREE_CODE (dest2) || TREE_CODE (dest1) != SSA_NAME)
    return false;

  return check_var_operand (dest1, dest2, d, func1, func2);
}

/* Deep basic block comparison function. All statements are iterated,
   type of the statement is chosen and a corresponding comparer is called. */

static bool
compare_bb (sem_bb_t *bb1, sem_bb_t *bb2, func_dict_t *d,
  tree func1, tree func2)
{
  gimple_stmt_iterator gsi1, gsi2;
  gimple s1, s2;

  if (bb1->stmt_count != bb2->stmt_count || bb1->edge_count != bb2->edge_count)
    return false;

  gsi2 = gsi_start_bb (bb2->bb);
  for (gsi1 = gsi_start_bb (bb1->bb); !gsi_end_p (gsi1); gsi_next (&gsi1))
  {
    s1 = gsi_stmt (gsi1);
    s2 = gsi_stmt (gsi2);

    if (gimple_code (s1) != gimple_code (s2))
      return false;

    switch (gimple_code (s1))
      {
      case GIMPLE_CALL:
        if (!check_ssa_call (s1, s2, d, func1, func2))
          return false;
        break;
      case GIMPLE_ASSIGN:
        if (!check_ssa_assign (s1, s2, d, func1, func2))
          return false;
        break;
      case GIMPLE_COND:
        if (!check_ssa_cond (s1, s2, d, func1, func2))
          return false;
        break;
      case GIMPLE_SWITCH:
        if (!check_ssa_switch (s1, s2, d, func1, func2))
          return false;
        break;
      case GIMPLE_DEBUG:
        break;
      case GIMPLE_LABEL:
        if (!check_ssa_label (s1, s2, d, func1, func2))
          return false;
        break;
      case GIMPLE_RETURN:
        if (!check_ssa_return (s1, s2, d, func1, func2))
          return false;
        break;
      case GIMPLE_GOTO:
        if (!check_ssa_goto (s1, s2, d, func1, func2))
          return false;
        break;
      case GIMPLE_RESX:
      case GIMPLE_ASM:
        return false;
      default:
        return false;
      }

    gsi_next (&gsi2);
  }

  return true;
}

/* Phi comparer. */

static bool
compare_phi_nodes (basic_block bb1, basic_block bb2, func_dict_t *d,
  tree func1, tree func2)
{
  gimple_stmt_iterator si1, si2;
  gimple phi1, phi2;
  unsigned size1, size2, i;
  tree t1, t2;
  edge e1, e2;

  si2 = gsi_start_phis (bb2);
  for (si1 = gsi_start_phis (bb1); !gsi_end_p (si1); gsi_next (&si1))
  {
    phi1 = gsi_stmt (si1);
    phi2 = gsi_stmt (si2);

    size1 = gimple_phi_num_args (phi1);
    size2 = gimple_phi_num_args (phi2);

    if (size1 != size2)
      return false;

    for (i = 0; i < size1; ++i)
    {
      t1 = gimple_phi_arg (phi1, i)->def;
      t2 = gimple_phi_arg (phi2, i)->def;

      if (!check_var_operand (t1, t2, d, func1, func2))
          return false;

      e1 = gimple_phi_arg_edge (phi1, i);
      e2 = gimple_phi_arg_edge (phi2, i);

      if (!check_edges (e1, e2, d))
        return false;
    }

    if (gsi_end_p (si2))
      return false;

    gsi_next (&si2);
  }

  return true;
}

/* Basic block bidictionary testing function. */

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

/* The main handler called for all a deep function comparison. */

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

  if (f1->arg_count != f2->arg_count || f1->bb_count != f2->bb_count ||
    f1->edge_count != f2->edge_count || f1->cfg_checksum != f2->cfg_checksum)
      return false;

  /* Result type checking */
  if (f1->result_type != f2->result_type)
    return false;

  /* Checking types of arguments */
  for (i = 0; i < f1->arg_count; ++i)
    if (!types_compatible_p (f1->arg_types[i], f2->arg_types[i]))
      return false;

  /* Checking function arguments */
  decl1 = DECL_ATTRIBUTES (f1->node->symbol.decl);
  decl2 = DECL_ATTRIBUTES (f2->node->symbol.decl);

  while (decl1)
    {
      if (decl2 == NULL)
        return false;

      if (get_attribute_name (decl1) != get_attribute_name (decl2))
        return false;

      decl1 = TREE_CHAIN (decl1);
      decl2 = TREE_CHAIN (decl2);
    }

  if (decl1 != decl2)
    return false;

  func_dict_init (&func_dict, f1->ssa_names_size, f2->ssa_names_size);

  /* Checking all basic blocks */
  for (i = 0; i < f1->bb_count; ++i)
    if(!compare_bb (f1->bb_sorted[i], f2->bb_sorted[i], &func_dict,
      f1->func_decl, f2->func_decl))
      {
        result = false;
        goto free_func_dict;
      }

  /* Basic block edges check */
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
              goto free_bb_dict;
            }

          if (!bb_dict_test (bb_dict, e1->dest->index, e2->dest->index))
            {
              result = false;
              goto free_bb_dict;
            }

          if (e1->flags != e2->flags)
            {
              result = false;
              goto free_bb_dict;
            }

          if (!check_edges (e1, e2, &func_dict))
            {
              result = false;
              goto free_bb_dict;
            }

          ei_next (&ei2);
        }
      } 

  /* Basic block PHI nodes comparison */
  for (i = 0; i < f1->bb_count; ++i)
    if (!compare_phi_nodes (f1->bb_sorted[i]->bb, f2->bb_sorted[i]->bb,
        &func_dict, f1->func_decl, f2->func_decl))
      result = false;

  free_bb_dict:
    free (bb_dict);

  free_func_dict:
    func_dict_free (&func_dict);

  return result;
}

/* Two semantically equal function are merged. */

/*
static void
merge_functions (sem_func_t *original, sem_func_t *alias)
{
  cgraph_release_function_body (alias->node);
  cgraph_reset_node (alias->node);
  cgraph_create_function_alias (alias->func_decl, original->func_decl);
}
*/

/* IPA semantic equality pass entry point. */

static unsigned int
semantic_equality (void)
{
  bool result, detected;
  sem_func_t *f, *f1;
  struct cgraph_node *node;
  unsigned int nnodes = 0;
  sem_func_t **slot;
  bool merged = false;
  hash_table <sem_func_var_hash> sem_function_hash;

  sem_functions = XNEWVEC (sem_func_t *, cgraph_n_nodes);
  sem_function_hash.create (nnodes);

  FOR_EACH_DEFINED_FUNCTION (node)
    {
      f = XNEW (sem_func_t);
      f->next = NULL;

      detected = visit_function (node, f);
      if (detected)
        {
          sem_functions[sem_function_count++] = f;

          slot = sem_function_hash.find_slot (f, INSERT);
          f1 = *slot;

          while(f1)
          {
            merged = false;
            result = compare_functions (f1, f);

            /* fprintf (stderr, " (%s)\n", result ? "EQUAL" : "different"); */

            if (result)
              {
#ifdef IPA_SEM_EQUALITY_DEBUG
                fprintf (stderr, "IPA_SEM_EQ HIT:%s:%s\n",
                  cgraph_node_name (f->node), cgraph_node_name (f1->node));

                dump_function_to_file (f1->func_decl, stderr, TDF_DETAILS);
                dump_function_to_file (f->func_decl, stderr, TDF_DETAILS);
#endif        
                /* Experimental merging of two functions */
                /* merge_functions (f, f1); */

                merged = true;
                break;
              }

            f1 = f1->next;
          }

          if (merged)
            free (f);
          else
            {
              f1 = (sem_func_t *) *slot;
              f->next = f1;
              *slot = f;
            }
        }
      else
        free (f);
    }

  free (sem_functions);

  sem_function_hash.dispose();

  return 0; 
}

static bool
gate_sem_equality (void)
{
  return flag_ipa_sem_equality;
}

struct simple_ipa_opt_pass pass_ipa_sem_equality =
{
 {
  SIMPLE_IPA_PASS,
  "sem-equality",         /* name */
  OPTGROUP_IPA,           /* optinfo_flags */
  gate_sem_equality,      /* gate */
  semantic_equality,      /* execute */
  NULL,                   /* sub */
  NULL,                   /* next */
  0,                      /* static_pass_number */
  TV_IPA_SEM_EQUALITY,    /* tv_id */
  0,                      /* properties_required */
  0,                      /* properties_provided */
  0,                      /* properties_destroyed */
  0,                      /* todo_flags_start */
  0                       /* todo_flags_finish */
 }
};
