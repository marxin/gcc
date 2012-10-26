#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "tree-flow.h"
#include "tree-pass.h"
#include "langhooks.h"
#include "gimple.h"
#include "cgraph.h"
#include "cfgloop.h"
#include "tree-ssa-sccvn.h"

/* Forward struct declaration */
typedef struct sem_bb sem_bb_t;
typedef struct sem_func sem_func_t;

/* Function struct for sematic equality pass */
typedef struct sem_func
{
  struct cgraph_node *node;
  tree func_decl;
  enum tree_code result_type;
  enum tree_code *arg_types;
  unsigned int arg_count;
  unsigned int bb_count;
  unsigned int *bb_sizes;
  hashval_t hashcode;
  sem_bb_t **bb_sorted;
  sem_func_t *next;
} sem_func_t;

/* Basic block struct for sematic equality pass */
typedef struct sem_bb
{
  basic_block bb;
  sem_func_t *func;
  htab_t variables;
  unsigned int count;
  hashval_t hashcode;
} sem_bb_t;

typedef struct ssa_pair
{
  tree_ssa_name *source;
  tree_ssa_name *target;
} ssa_pair_t;

typedef struct ssa_dict
{
  htab_t source;
  htab_t target;
} ssa_dict_t;

static sem_func_t **sem_functions;
static unsigned int sem_function_count;
static htab_t sem_function_hash;

/* Htab calculation function for semantic function struct. */

static hashval_t
func_hash (const void *func)
{
  unsigned int i;

  const sem_func_t *f = (const sem_func_t *)func;

  hashval_t hash = 0;
  
  hash = iterative_hash_object (f->arg_count, hash);
  hash = iterative_hash_object (f->bb_count, hash);

  for(i = 0; i < f->arg_count; ++i)
    hash = iterative_hash_object (f->arg_types[i], hash);

  for(i = 0; i < f->bb_count; ++i)
    hash = iterative_hash_object (f->bb_sizes[i], hash);

  return hash;
}

/* Semantic function equality comparer. */

static int
func_equal (const void *func1, const void *func2)
{
  const sem_func_t *f1 = (const sem_func_t *)func1;
  const sem_func_t *f2 = (const sem_func_t *)func2;

  /* TODO */
  return f1->hashcode == f2->hashcode; 
}

/* Semantic function htab memory release function. */

static void
func_free (void *func)
{
  sem_func_t *f = (sem_func_t *)func;

  free(f->arg_types);
  free(f->bb_sizes);
  free(f);
}

static hashval_t
bb_hash (const void *basic_block)
{
  const sem_bb_t *bb = (const sem_bb_t *)basic_block;

  hashval_t hash = bb->count;

  return hash;
}

static void
generate_summary (void)
{
  unsigned int i, j;

  printf ("analysed functions (%u):\n", sem_function_count);

  for(i = 0; i < sem_function_count; ++i)
  {
    sem_func_t *f = sem_functions[i];

    if(f == NULL)
      continue;

    printf ("  function: %s\n", cgraph_node_name (f->node));
    printf ("  hash: %d\n", f->hashcode);
    printf ("    res: %u\n", f->result_type);

    for(j = 0; j < f->arg_count; ++j)
      printf ("    arg[%u]: %u\n", j, f->arg_types[j]);

    for(j = 0; j < f->bb_count; ++j)
      printf ("      bb[%u]: %u, hashcode: %u\n", j, f->bb_sizes[j], f->bb_sorted[j]->hashcode);
  }
}

static void 
visit_function (struct cgraph_node *node, sem_func_t *f)
{
  tree fndecl, fnargs, parm, result;
  unsigned int param_num, gimple_count, bb_count, edge_count;
  edge e;
  edge_iterator ei;
  struct function *my_function;
  gimple_stmt_iterator gsi;
  basic_block bb;
  sem_bb_t *sem_bb;
  gimple g;

  fndecl = node->symbol.decl;    
  my_function = DECL_STRUCT_FUNCTION (fndecl);

  // TODO
  if (!my_function)
  {
    return;
  }

  f->node = node;
  f->func_decl = fndecl;
  fnargs = DECL_ARGUMENTS (fndecl);

  /* iterating all function arguments */
  param_num = 0;
  for (parm = fnargs; parm; parm = DECL_CHAIN (parm))
    param_num++;

  f->arg_count = param_num;
  f->arg_types = XCNEWVEC (enum tree_code, param_num);

  param_num = 0;
  for (parm = fnargs; parm; parm = DECL_CHAIN (parm))
    f->arg_types[param_num++] = TREE_CODE(DECL_ARG_TYPE(parm));

  /* Function result type */
  result = DECL_RESULT(fndecl);

  if (result)
    f->result_type = TREE_CODE (TREE_TYPE (DECL_RESULT(fndecl)));

  bb_count = 0;
  FOR_EACH_BB_FN (bb, my_function)
  {
    bb_count++;
  }

 /* basic block iteration */
  f->bb_count = bb_count;
  f->bb_sizes = XCNEWVEC (unsigned int, f->bb_count);

  f->bb_sorted = XCNEWVEC (sem_bb_t *, f->bb_count);
  
  // TODO: remove
  bb_count = 0;
  FOR_EACH_BB_FN (bb, my_function)
  {
    gimple_count = 0;

    for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
    {
      edge_count = 0;
      g = gsi_stmt(gsi);

      /*
       * DEBUG OUTPUT
      printf( "\t%s\n", gimple_code_name[gimple_code(g)]);
      debug_gimple_stmt(gsi_stmt(gsi));
      */

      /* iterating all edges */
      FOR_EACH_EDGE (e, ei, bb->succs)
      {
        edge_count++;
      }

      gimple_count++;
    }

    f->bb_sizes[bb_count] = gimple_count;

    /* Inserting basic block to hash table */

    sem_bb = XCNEW (sem_bb_t);
    sem_bb->bb = bb;
    sem_bb->func = f;
    sem_bb->count = gimple_count;
    sem_bb->hashcode = bb_hash (sem_bb);

    f->bb_sorted[bb_count] = sem_bb;
    bb_count++;
  }
}

static void
ssa_free (void *ssa_name)
{
  free(ssa_name);
}

static hashval_t
ssa_hash (const void *ssa_name)
{
  const ssa_pair_t *p = (const ssa_pair_t *)ssa_name;

  return (size_t)p->source;
}

static int ssa_equal (const void *ssa_name1, const void *ssa_name2)
{
  const ssa_pair_t *p1 = (const ssa_pair_t *)ssa_name1;
  const ssa_pair_t *p2 = (const ssa_pair_t *)ssa_name2;

  return p1->source == p2->source;
}

static bool
ssa_check_htable (htab_t ssa_htable, tree_ssa_name *n1, tree_ssa_name *n2)
{
  void **slot;

  ssa_pair_t *pair = XCNEW (ssa_pair_t);
  pair->source = n1;
  pair->target = n2;

  ssa_pair_t *hit = (ssa_pair_t *)htab_find (ssa_htable, pair);

  if(hit)
  {
    tree_ssa_name *hit_target = hit->target;
    free (pair);
    return hit_target == n2;
  }
  else
  {
    slot = htab_find_slot (ssa_htable, pair, INSERT);
    *slot = pair;

    return true;
  }
}

static bool
ssa_check_names (ssa_dict_t ssa_dict, tree t1, tree t2)
{
  bool result = ssa_check_htable (ssa_dict.source, &t1->ssa_name, &t2->ssa_name);

  if(!result)
    return false; /* source ssa name already coresponds to a different one */
  else
  {
    if (!ssa_check_htable (ssa_dict.source, &t2->ssa_name, &t1->ssa_name))
      return false;

    return useless_type_conversion_p (TREE_TYPE (t1), TREE_TYPE (t2))
      && useless_type_conversion_p (TREE_TYPE (t1), TREE_TYPE (t2));
  }
}

static bool
check_ssa_or_const (tree t1, tree t2, ssa_dict_t ssa_dict)
{
  if (t1 == NULL || t2 == NULL)
    return false;

  if (TREE_CODE (t1) == SSA_NAME && TREE_CODE (t2) == SSA_NAME)
    return ssa_check_names (ssa_dict, t1, t2); 
  else
    return operand_equal_p (t1, t2, OEP_ONLY_CONST);
}

static bool
check_ssa_call (gimple s1, gimple s2, ssa_dict_t ssa_dict)
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

    if (TREE_CODE (t1) == SSA_NAME && TREE_CODE (t2) == SSA_NAME)
      return ssa_check_names (ssa_dict, t1, t2);

    if (!operand_equal_p (t1, t2, OEP_ONLY_CONST))
        return false;
  }

  /* return value checking */
  t1 = gimple_get_lhs (s1);
  t2 = gimple_get_lhs (s2);

  if (t1 == NULL_TREE || t2 == NULL_TREE)
    return true;
  else if(t1 == NULL_TREE || t2 == NULL_TREE)
    return false;
  else
    return check_ssa_or_const (t1, t2, ssa_dict);
}

static bool
check_ssa_assign (gimple s1, gimple s2, ssa_dict_t ssa_dict)
{
  tree lhs1, lhs2;
  tree rhs1, rhs2;
  enum gimple_rhs_class class1, class2;
  enum tree_code code1, code2;

  class1 = gimple_assign_rhs_class (s1);
  class2 = gimple_assign_rhs_class (s2);

  if (class1 != class2)
    return false;

  code1 = gimple_expr_code (s1);
  code2 = gimple_expr_code (s2);

  if (code1 != code2)
    return false;

  switch (class1)
  {
    case GIMPLE_BINARY_RHS:
    {
      rhs1 = gimple_assign_rhs2 (s1);
      rhs2 = gimple_assign_rhs2 (s2);

      if (!check_ssa_or_const (rhs1, rhs2, ssa_dict))
        return false;
    }
    case GIMPLE_SINGLE_RHS:
    case GIMPLE_UNARY_RHS:
    {
      rhs1 = gimple_assign_rhs1 (s1);
      rhs2 = gimple_assign_rhs1 (s2);

      if (!check_ssa_or_const (rhs1, rhs2, ssa_dict))
        return false;

      break;
    }
    default:
      return false;
  }

  lhs1 = gimple_get_lhs (s1);
  lhs2 = gimple_get_lhs (s2);

  if (gimple_vdef (s1))
  { 
    if (vn_valueize (gimple_vdef (s1)) != vn_valueize (gimple_vdef (s2)))
      return false;

    if (TREE_CODE (lhs1) != SSA_NAME && TREE_CODE (lhs2) != SSA_NAME)
      return true;

    return false;
  }

  if (TREE_CODE (lhs1) == SSA_NAME && TREE_CODE (lhs2) == SSA_NAME)
    return ssa_check_names (ssa_dict, lhs1, lhs2);
  else
    return false;
}

static bool
check_ssa_cond (gimple s1, gimple s2, ssa_dict_t ssa_dict)
{
  tree t1, t2;
  enum tree_code code1, code2;

  t1 = gimple_cond_lhs (s1);
  t2 = gimple_cond_lhs (s2);

  if (!check_ssa_or_const (t1, t2, ssa_dict))
    return false;

  t1 = gimple_cond_rhs (s1);
  t2 = gimple_cond_rhs (s2);

  if (!check_ssa_or_const (t1, t2, ssa_dict))
    return false;

  code1 = gimple_expr_code (s1);
  code2 = gimple_expr_code (s2);

  return code1 == code2;
}

static bool
check_ssa_label (gimple g1, gimple g2)
{
  /* TODO
  tree t1, t2;

  t1 = gimple_label_label (g1);
  t2 = gimple_label_label (g2);
  */

  return true;
}

static bool
check_ssa_return (gimple g1, gimple g2, ssa_dict_t ssa_dict)
{
  tree t1, t2;

  t1 = gimple_return_retval (g1);
  t2 = gimple_return_retval (g2);

  return check_ssa_or_const (t1, t2, ssa_dict);
}

static bool
compare_bb (sem_bb_t *bb1, sem_bb_t *bb2, ssa_dict_t ssa_dict)
{
  gimple_stmt_iterator gsi1, gsi2;
  gimple s1, s2;

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
        if (!check_ssa_call (s1, s2, ssa_dict))
          return false;
        break;
      case GIMPLE_ASSIGN:
        if (!check_ssa_assign (s1, s2, ssa_dict))
          return false;
        break;

      case GIMPLE_COND:
        if (!check_ssa_cond (s1, s2, ssa_dict))
          return false;
        break;

      case GIMPLE_GOTO:
        break;

      case GIMPLE_LABEL:
        if (!check_ssa_label (s1, s2))
          return false;
        break;
      case GIMPLE_RETURN:
        if (!check_ssa_return (s1, s2, ssa_dict))
          return false;
        break;

      default:
        return false;
    }

    gsi_next (&gsi2);
  }

  return true;
}

static bool bb_dict_test (int* bb_dict, int source, int target)
{
  if (bb_dict[source] == -1)
  {
    bb_dict[source] = target;
    return true;
  }
  else
    return bb_dict[source] == target;
}

static bool
compare_functions (sem_func_t *f1, sem_func_t *f2)
{
  tree decl1, decl2;
  basic_block bb1, bb2;
  edge e1, e2;
  edge_iterator ei1, ei2;
  int *bb_dict;

  unsigned int i;
  ssa_dict_t ssa_dict;

  if (f1->arg_count != f2->arg_count || f1->bb_count != f2->bb_count)
    return false;

  /* Result type checking */
  if (f1->result_type != f2->result_type)
    return false;

  /* Checking types of arguments */
  for (i = 0; i < f1->arg_count; ++i)
    if (f1->arg_types[i] != f2->arg_types[i])
      return false;

  /* Checking function arguments */
  decl1 = DECL_ATTRIBUTES (f1->node->symbol.decl);
  decl2 = DECL_ATTRIBUTES (f2->node->symbol.decl);

  while(decl1)
  {
    if (decl2 == NULL)
      return false;

    if (get_attribute_name (decl1) != get_attribute_name (decl2))
      return false;

    decl1 = TREE_CHAIN (decl1);
    decl2 = TREE_CHAIN (decl2);
  }

  if(decl1 != decl2)
    return false;

  /* SSA names hash table */
  ssa_dict.source = htab_create (0, ssa_hash, ssa_equal, ssa_free);
  ssa_dict.target = htab_create (0, ssa_hash, ssa_equal, ssa_free);

  bool result = true;

  /* Checking all basic blocks */
  for (i = 0; i < f1->bb_count; ++i)
    if(!compare_bb (f1->bb_sorted[i], f2->bb_sorted[i], ssa_dict))
    {
      result = false;
      break;
    }

  /* Basic block edges check */
  for (i = 0; i < f1->bb_count; ++i)
  {
    /* TODO: ask */
    bb_dict = XCNEWVEC (int, f1->bb_count + 2);
    memset (bb_dict, -1, (f1->bb_count + 2) * sizeof (int));

    bb1 = f1->bb_sorted[i]->bb;
    bb2 = f2->bb_sorted[i]->bb;

    ei2 = ei_start (bb2->preds);

    for (ei1 = ei_start (bb1->preds); ei_cond (ei1, &e1); ei_next (&ei1))
    {
      ei_cond (ei2, &e2);

      if (!bb_dict_test (bb_dict, e1->src->index, e2->src->index))
        return false;

      if (!bb_dict_test (bb_dict, e1->dest->index, e2->dest->index))
        return false;

      if (e1->flags != e2->flags)
        return false;

      ei_next (&ei2);
    }

    free (bb_dict);
  } 

  /* Htab deletion */
  htab_delete (ssa_dict.source);
  htab_delete (ssa_dict.target);

  return result;
}

static unsigned int
semantic_equality (void)
{
  bool result;
  sem_func_t *f, *f1;
  struct cgraph_node *node;
  unsigned int nnodes = 0;
  void **slot;

  sem_functions = XCNEWVEC (sem_func_t *, cgraph_n_nodes);
  sem_function_hash = htab_create (nnodes, func_hash, func_equal, func_free);

  printf ("=== IPA semantic equality pass dump ===\n");

  FOR_EACH_DEFINED_FUNCTION (node)
  {
    f = XCNEW (sem_func_t);
    f->next = NULL;

    visit_function (node, f);

    sem_functions[sem_function_count++] = f;

    /* hash table insertion */
    f->hashcode = func_hash(f);

    printf ("\tfunction: '%s' with hash: %u\n", cgraph_node_name (f->node), f->hashcode);

    slot = htab_find_slot_with_hash (sem_function_hash, f, f->hashcode, INSERT);
    f1 = (sem_func_t *)*slot;

    while(f1)
    {

      /* TODO */
      printf ("\t\tcomparing with: '%s'", cgraph_node_name (f1->node));

      result = compare_functions(f1, f);

      printf (" (%s)\n", result ? "EQUAL" : "different");

      f1 = f1->next;
    
    }

    f1 = (sem_func_t *)*slot;
    f->next = f1;
    *slot = f;
  }

  /* generate_summary (); */

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
