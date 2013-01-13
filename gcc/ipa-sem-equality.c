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
#include "coverage.h"

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
  unsigned stmt_count;
  unsigned edge_count;
  hashval_t hashcode;
} sem_bb_t;

typedef struct ssa_pair
{
  tree_ssa_name *source;
  tree_ssa_name *target;
} ssa_pair_t;

typedef struct vardecl_pair
{
  tree source;
  tree target;
  vardecl_pair *next;
} vardecl_pair_t;

typedef struct ssa_dict
{
  int *source;
  int *target;
  vardecl_pair_t *vardecl_list;
} ssa_dict_t;

static sem_func_t **sem_functions;
static unsigned int sem_function_count;
static htab_t sem_function_hash;

static
hashval_t iterative_hash_tree (tree type, hashval_t hash)
{
  uintptr_t pointer = (uintptr_t)type;
  hashval_t *h = (hashval_t *)&pointer;

  for (unsigned i = 0; i < sizeof(uintptr_t) / sizeof(hashval_t); ++i)
    hash = iterative_hash_object (h[i], hash);

  return hash;
}

/* SSA dictionary functions */
static void
ssa_dict_init (ssa_dict_t *d, unsigned ssa_names_size1, unsigned ssa_names_size2) 
{
  d->source = XCNEWVEC (int, ssa_names_size1);
  d->target = XCNEWVEC (int, ssa_names_size2);

  memset (d->source, -1, ssa_names_size1 * sizeof (int));
  memset (d->target, -1, ssa_names_size2 * sizeof (int));

  d->vardecl_list = NULL;
}

static void
ssa_dict_free (ssa_dict_t *d)
{
  vardecl_pair_t *p;

  while (d->vardecl_list)
  {
    p = d->vardecl_list;
    d->vardecl_list = d->vardecl_list->next;

    free (p);
  }

  free (d->source);
  free (d->target);
}

static bool
ssa_dict_look_up (ssa_dict_t *d, tree ssa1, tree ssa2)
{
  unsigned i1, i2;

  i1 = SSA_NAME_VERSION (ssa1);
  i2 = SSA_NAME_VERSION (ssa2);

  if (d->source[i1] == -1)
    d->source[i1] = i2;
  else if (d->source[i1] != (int)i2)
    return false;

  if(d->target[i2] == -1)
    d->target[i2] = i1;
  else if (d->target[i2] != (int)i1)
    return false;

  return true;
}

/* Htab calculation function for semantic function struct. */
static hashval_t
func_hash (const void *func)
{
  unsigned int i;

  const sem_func_t *f = (const sem_func_t *)func;

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

/* Semantic function equality comparer. */
static int
func_equal (const void *func1, const void *func2)
{
  const sem_func_t *f1 = (const sem_func_t *)func1;
  const sem_func_t *f2 = (const sem_func_t *)func2;

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

  hashval_t hash = bb->stmt_count;
  hash = iterative_hash_object (bb->edge_count, hash);

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

    for(j = 0; j < f->bb_count; ++j)
      printf ("      bb[%u]: %u, hashcode: %u\n", j, f->bb_sizes[j], f->bb_sorted[j]->hashcode);
  }
}

static void 
visit_function (struct cgraph_node *node, sem_func_t *f)
{
  tree fndecl, fnargs, parm, result;
  unsigned int param_num, gimple_count, bb_count;
  struct function *my_function;
  gimple_stmt_iterator gsi;
  basic_block bb;
  sem_bb_t *sem_bb;

  fndecl = node->symbol.decl;    
  my_function = DECL_STRUCT_FUNCTION (fndecl);

  /* TODO: add alert */
  if (!my_function) 
    return;

  f->ssa_names_size = SSANAMES (my_function)->length ();
  f->node = node;
  f->func_decl = fndecl;
  fnargs = DECL_ARGUMENTS (fndecl);

  /* iterating all function arguments */
  param_num = 0;
  for (parm = fnargs; parm; parm = DECL_CHAIN (parm))
    param_num++;

  f->arg_count = param_num;
  f->arg_types = XCNEWVEC (tree, param_num);

  param_num = 0;
  for (parm = fnargs; parm; parm = DECL_CHAIN (parm))
    f->arg_types[param_num++] = gimple_register_canonical_type (DECL_ARG_TYPE (parm));

  /* Function result type */
  result = DECL_RESULT(fndecl);

  if (result)
    f->result_type = gimple_register_canonical_type (TREE_TYPE (DECL_RESULT(fndecl)));

  /* basic block iteration */
  f->bb_count = n_basic_blocks_for_function (my_function) - 2;

  f->edge_count = n_edges_for_function (my_function);
  f->bb_sizes = XCNEWVEC (unsigned int, f->bb_count);

  f->bb_sorted = XCNEWVEC (sem_bb_t *, f->bb_count);
  f->cfg_checksum = coverage_compute_cfg_checksum_fn (my_function);

  bb_count = 0;
  FOR_EACH_BB_FN (bb, my_function)
  {
    gimple_count = 0;

    for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
      gimple_count++;

    f->bb_sizes[bb_count] = gimple_count;

    /* Inserting basic block to hash table */
    sem_bb = XCNEW (sem_bb_t);
    sem_bb->bb = bb;
    sem_bb->func = f;
    sem_bb->stmt_count = gimple_count;
    sem_bb->edge_count = EDGE_COUNT (bb->preds) + EDGE_COUNT (bb->succs);
    sem_bb->hashcode = bb_hash (sem_bb);

    f->bb_sorted[bb_count++] = sem_bb;
  }
}

static bool
ssa_check_names (ssa_dict_t *d, tree t1, tree t2)
{
  if (!ssa_dict_look_up (d, t1, t2))
    return false;

  return useless_type_conversion_p (TREE_TYPE (t1), TREE_TYPE (t2))
    && useless_type_conversion_p (TREE_TYPE (t1), TREE_TYPE (t2));
}

static bool
check_vardecl (tree t1, tree t2, ssa_dict_t *d, tree func1, tree func2)
{
  vardecl_pair_t *vardecl_pair, *vardecl_next_pair;

  if (auto_var_in_fn_p (t1, func1) && auto_var_in_fn_p (t2, func2))
  {
    vardecl_pair = d->vardecl_list;

    while (vardecl_pair)
    {
      if (vardecl_pair->source == t1 || vardecl_pair->target == t2)
        return vardecl_pair->source == t1 && vardecl_pair->target == t2;

      vardecl_pair = vardecl_pair->next;
    }

    vardecl_next_pair = XCNEW (vardecl_pair_t);
    vardecl_next_pair->source = t1;
    vardecl_next_pair->target = t2;
    vardecl_next_pair->next = NULL;

    if (vardecl_pair)
      vardecl_pair->next = vardecl_next_pair;
    else
      d->vardecl_list = vardecl_next_pair;

    return true;
  }
  else
    return t1 == t2;
}

static bool
compare_handled_component (tree t1, tree t2, ssa_dict_t *d, tree func1, tree func2)
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

  switch (TREE_CODE (t1))
  {
    case ARRAY_REF:
    case COMPONENT_REF:
    case MEM_REF:
    {
      x1 = TREE_OPERAND (t1, 0);
      x2 = TREE_OPERAND (t2, 0);
      y1 = TREE_OPERAND (t1, 1);
      y2 = TREE_OPERAND (t2, 1);

      return compare_handled_component (x1, x2, d, func1, func2) && compare_handled_component (y1, y2, d, func1, func2);
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
      return ssa_check_names (d, t1, t2);
    case FUNCTION_DECL:
    case FIELD_DECL:
      return t1 == t2;
    case VAR_DECL:
      return check_vardecl (t1, t2, d, func1, func2);
    default:
      return false;
  }
}

static bool
check_var_operand (tree t1, tree t2, ssa_dict_t *d, tree func1, tree func2)
{
  enum tree_code tc1, tc2;

  if (t1 == NULL || t2 == NULL)
    return false;

  tc1 = TREE_CODE (t1);
  tc2 = TREE_CODE (t2);

  if (tc1 != tc2)
    return false;

  switch (tc1)
  {
    case CONSTRUCTOR: /* TODO: handle in a proper way */
      return true;
    case VAR_DECL:
      return check_vardecl (t1, t2, d, func1, func2);
    case SSA_NAME:
      return ssa_check_names (d, t1, t2); 
    default:
      break;
  }

  if ((handled_component_p (t1) && handled_component_p (t1)) || tc1 == ADDR_EXPR || tc2 == MEM_REF)
    return compare_handled_component (t1, t2, d, func1, func2);
  else 
    return operand_equal_p (t1, t2, OEP_ONLY_CONST);
}

static bool
check_ssa_call (gimple s1, gimple s2, ssa_dict_t *d, tree func1, tree func2)
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
      return ssa_check_names (d, t1, t2);

    if (!operand_equal_p (t1, t2, OEP_ONLY_CONST))
        return false;
  }

  /* return value checking */
  t1 = gimple_get_lhs (s1);
  t2 = gimple_get_lhs (s2);

  if (t1 == NULL_TREE && t2 == NULL_TREE)
    return true;
  else if(t1 == NULL_TREE || t2 == NULL_TREE)
    return false;
  else
    return check_var_operand (t1, t2, d, func1, func2);
}

static bool
check_ssa_assign (gimple s1, gimple s2, ssa_dict_t *d, tree func1, tree func2)
{
  tree lhs1, lhs2;
  tree rhs1, rhs2;
  enum tree_code code1, code2;
  enum gimple_rhs_class class1;

  code1 = gimple_expr_code (s1);
  code2 = gimple_expr_code (s2);

  if (code1 != code2)
    return false;

  code1 = gimple_assign_rhs_code (s1);
  code2 = gimple_assign_rhs_code (s2);

  class1 = gimple_assign_rhs_class (s1);

  if (code1 != code2) 
    return false;

  switch (class1)
  {
    case GIMPLE_BINARY_RHS:
    {
      rhs1 = gimple_assign_rhs2 (s1);
      rhs2 = gimple_assign_rhs2 (s2);

      if (!check_var_operand (rhs1, rhs2, d, func1, func2))
        return false;
    }
    case GIMPLE_SINGLE_RHS:
    case GIMPLE_UNARY_RHS:
    {
      rhs1 = gimple_assign_rhs1 (s1);
      rhs2 = gimple_assign_rhs1 (s2);

      if (!check_var_operand (rhs1, rhs2, d, func1, func2))
        return false;

      break;
    }
    default:
      return false;
  }

  lhs1 = gimple_get_lhs (s1);
  lhs2 = gimple_get_lhs (s2);

  return check_var_operand (lhs1, lhs2, d, func1, func2);
}

static bool
check_ssa_cond (gimple s1, gimple s2, ssa_dict_t *d, tree func1, tree func2)
{
  tree t1, t2;
  enum tree_code code1, code2;

  code1 = gimple_expr_code (s1);
  code2 = gimple_expr_code (s2);

  return code1 == code2;

  t1 = gimple_cond_lhs (s1);
  t2 = gimple_cond_lhs (s2);

  if (!check_var_operand (t1, t2, d, func1, func2))
    return false;

  t1 = gimple_cond_rhs (s1);
  t2 = gimple_cond_rhs (s2);

  if (!check_var_operand (t1, t2, d, func1, func2))
    return false;
}

static bool
check_ssa_label (gimple g1, gimple g2)
{
  // TODO: do a complex check
  return !(FORCED_LABEL (gimple_label_label (g1)) || FORCED_LABEL (gimple_label_label (g2)));
}

static bool
check_ssa_switch (gimple g1, gimple g2, ssa_dict_t *d, tree func1, tree func2)
{
  unsigned lsize1, lsize2, i;
  tree t1, t2;

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
    t1 = CASE_LABEL (gimple_switch_label (g1, i));
    t2 = CASE_LABEL (gimple_switch_label (g2, i));
  }

  return true;
}

static bool
check_ssa_return (gimple g1, gimple g2, ssa_dict_t *d, tree func1, tree func2)
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

static bool
compare_bb (sem_bb_t *bb1, sem_bb_t *bb2, ssa_dict_t *d, tree func1, tree func2)
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

      case GIMPLE_RESX:
      case GIMPLE_DEBUG:
      case GIMPLE_GOTO:
        return false;

      case GIMPLE_LABEL:
        if (!check_ssa_label (s1, s2))
          return false;
        break;
      case GIMPLE_RETURN:
        if (!check_ssa_return (s1, s2, d, func1, func2))
          return false;
        break;

      default:
        return false;
    }

    gsi_next (&gsi2);
  }

  return true;
}

static bool
compare_phi_nodes (basic_block bb1, basic_block bb2, ssa_dict_t *d, tree func1, tree func2)
{
  gimple_stmt_iterator si1, si2;
  gimple phi1, phi2;
  unsigned size1, size2, i;
  tree t1, t2;

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
    }

    if (gsi_end_p (si2))
      return false;

    gsi_next (&si2);
  }

  return true;
}

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

  if (f1->arg_count != f2->arg_count || f1->bb_count != f2->bb_count ||
    f1->edge_count != f2->edge_count || f1->cfg_checksum != f2->cfg_checksum)
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

  ssa_dict_init (&ssa_dict, f1->ssa_names_size, f2->ssa_names_size);

  /* Checking all basic blocks */
  for (i = 0; i < f1->bb_count; ++i)
    if(!compare_bb (f1->bb_sorted[i], f2->bb_sorted[i], &ssa_dict, f1->func_decl, f2->func_decl))
      return false;

  /* Basic block edges check */
  for (i = 0; i < f1->bb_count; ++i)
  {
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

  /* Basic block PHI nodes comparison */
  for (i = 0; i < f1->bb_count; ++i)
    if (!compare_phi_nodes (f1->bb_sorted[i]->bb, f2->bb_sorted[i]->bb, &ssa_dict, f1->func_decl, f2->func_decl))
      return false;

  ssa_dict_free (&ssa_dict);

  return true;
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

  generate_summary ();

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
