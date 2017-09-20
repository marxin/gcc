/* Tree switch conversion for GNU compiler.
   Copyright (C) 2017 Free Software Foundation, Inc.

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

#ifndef TREE_SWITCH_CONVERSION_H
#define TREE_SWITCH_CONVERSION_H

namespace tree_switch_conversion {

/* Type of cluster.  */

enum cluster_type
{
  SIMPLE_CASE,
  JUMP_TABLE,
  BIT_TEST
};

#define PRINT_CASE(f,c) print_dec (c, f, TYPE_SIGN (TREE_TYPE (c)))

/* Base class for switch clustering.   */

struct cluster
{
  /* Destructor.  */
  virtual ~cluster ()
  {}

  /* Return type.  */
  virtual cluster_type get_type () = 0;

  /* Get low value covered by a cluster.  */
  virtual tree get_low () = 0;

  /* Get high value covered by a cluster.  */
  virtual tree get_high () = 0;

  /* Dump content of a cluster.  */
  virtual void debug () = 0;

  virtual void dump (FILE *f) = 0;

  virtual void emit (tree, tree, tree, basic_block)
  {}

  /* Return range of a cluster.  */
  static unsigned HOST_WIDE_INT get_range (tree low, tree high)
  {
    tree unsigned_type = unsigned_type_for (TREE_TYPE (low));
    tree r = fold_build2 (MINUS_EXPR, unsigned_type, high, low);

    return tree_to_uhwi (r) + 1;
  }

  /* Case label.  */
  tree m_case_label_expr;

  /* Basic block of the case.  */
  basic_block m_case_bb;

  /* Probability of taking this cluster.  */
  profile_probability m_prob;

  /* Probability of reaching subtree rooted at this node.  */
  profile_probability m_subtree_prob;  
};

struct simple_cluster: public cluster
{
  /* Constructor.  */
  simple_cluster(tree low, tree high, tree case_label_expr, basic_block case_bb,
		 profile_probability prob):
    m_low (low), m_high (high)
  {
    // TODO: add base ctor 
    m_case_label_expr = case_label_expr;
    m_case_bb = case_bb;
    m_prob = prob;
    m_subtree_prob = prob;
  }

  virtual ~simple_cluster ()
  {}

  virtual cluster_type
  get_type ()
  {
    return SIMPLE_CASE;
  }

  virtual tree
  get_low ()
  {
    return m_low;
  }

  virtual tree
  get_high ()
  {
    return m_high;
  }

  virtual void
  debug ()
  {
    dump (stderr);
  }

  virtual void
  dump (FILE *f)
  {
    PRINT_CASE (f, get_low ());
    if (get_low () != get_high ())
      {
	fprintf (f, "-");
	PRINT_CASE (f, get_high ());
      }
    fprintf (f, " ");
  }

  /* Low value of the case.  */
  tree m_low;

  /* High value of the case.  */
  tree m_high;
};

struct group_cluster: public cluster
{
  /* Destructor.  */
  virtual ~group_cluster ();

  virtual tree
  get_low ()
  {
    return m_cases[0]->get_low ();
  }

  virtual tree
  get_high ()
  {
    return m_cases[m_cases.length () - 1]->get_high ();
  }

  virtual void
  debug ()
  {
    dump (stderr);
  }

  virtual void
  dump (FILE *f)
  {
    fprintf (f, "%s(%d):", get_type () == JUMP_TABLE ? "JT" : "BT",
	     m_cases.length ());
    PRINT_CASE (f, get_low ());
    fprintf (f, "-");
    PRINT_CASE (f, get_high ());
    fprintf (f, " ");
  }


  /* List of simple clusters handled by the group.  */
  vec<simple_cluster *> m_cases;
};

struct jump_table_cluster: public group_cluster 
{
  /* Constructor.  */
  jump_table_cluster (vec<cluster *> &clusters, unsigned start, unsigned end);

  virtual cluster_type
  get_type ()
  {
    return JUMP_TABLE;
  }

  virtual void emit (tree index_expr, tree index_type,
		     tree default_label_expr, basic_block default_bb);

  /* Find jump tables of given CLUSTERS, where all members of the vector
     are of type simple_cluster.  New clusters are appended to OUTPUT vector.
     */
  static vec<cluster *> find_jump_tables (vec<cluster *> &clusters);

  /* Return true when cluster starting at START and ending at END (inclusive)
     can build a jump-table.  */
  static bool can_be_handled (const vec<cluster *> &clusters, unsigned start,
			      unsigned end);

  static bool is_beneficial (const vec<cluster *> &clusters, unsigned start,
			     unsigned end);

  /* Return the smallest number of different values for which it is best
     to use a jump-table instead of a tree of conditional branches.  */
  static inline unsigned int case_values_threshold (void);
};

struct bit_test_cluster: public group_cluster
{
  /* Constructor.  */
  bit_test_cluster (vec<cluster *> &clusters, unsigned start, unsigned end);

  virtual cluster_type
  get_type ()
  {
    return BIT_TEST;
  }

  virtual void emit (tree index_expr, tree index_type,
		     tree default_label_expr, basic_block default_bb);

  static vec<cluster *> find_bit_tests (vec<cluster *> &clusters);

  /* Return true when cluster starting at START and ending at END (inclusive)
     can build a bit test.  */
  static bool can_be_handled (const vec<cluster *> &clusters, unsigned start,
			      unsigned end);

  static bool is_beneficial (const vec<cluster *> &clusters, unsigned start,
			     unsigned end);

/* Split the basic block at the statement pointed to by GSIP, and insert
   a branch to the target basic block of E_TRUE conditional on tree
   expression COND.

   It is assumed that there is already an edge from the to-be-split
   basic block to E_TRUE->dest block.  This edge is removed, and the
   profile information on the edge is re-used for the new conditional
   jump.
   
   The CFG is updated.  The dominator tree will not be valid after
   this transformation, but the immediate dominators are updated if
   UPDATE_DOMINATORS is true.
   
   Returns the newly created basic block.  */
  static basic_block
  hoist_edge_and_branch_if_true (gimple_stmt_iterator *gsip,
				 tree cond, basic_block case_bb);

};

/* Helper struct to find minimal clusters.  */

struct min_cluster_item
{
  /* Constructor.  */
  min_cluster_item (unsigned count, unsigned start, unsigned non_jt_cases):
    m_count (count), m_start (start), m_non_jt_cases (non_jt_cases)
  {}

  /* Count of clusters.  */
  unsigned m_count;

  /* Index where is cluster boundary.  */
  unsigned m_start;

  /* Total number of cases that will not be in a jump table.  */
  unsigned m_non_jt_cases;
};

struct case_tree_node
{
  /* Empty Constructor.  */
  case_tree_node ();

  case_tree_node		*left;	/* Left son in binary tree.  */
  case_tree_node		*right;	/* Right son in binary tree;
				   also node chain.  */
  case_tree_node		*parent; /* Parent of node in binary tree.  */
  cluster		*c;
};

inline
case_tree_node::case_tree_node ():
  left (NULL), right (NULL), parent (NULL), c (NULL)
{
}

unsigned int
jump_table_cluster::case_values_threshold (void)
{
  unsigned int threshold = PARAM_VALUE (PARAM_CASE_VALUES_THRESHOLD);

  if (threshold == 0)
    threshold = targetm.case_values_threshold ();

  return threshold;
}

/* A case_bit_test represents a set of case nodes that may be
   selected from using a bit-wise comparison.  HI and LO hold
   the integer to be tested against, TARGET_EDGE contains the
   edge to the basic block to jump to upon success and BITS
   counts the number of case nodes handled by this test,
   typically the number of bits set in HI:LO.  The LABEL field
   is used to quickly identify all cases in this set without
   looking at label_to_block for every case label.  */

struct case_bit_test
{
  wide_int mask;
  basic_block target_bb;
  tree label;
  int bits;

/* Comparison function for qsort to order bit tests by decreasing
   probability of execution.  */

  static int cmp (const void *p1, const void *p2);
};

struct switch_decision_tree
{
  /* Reset the aux field of all outgoing edges of basic block BB.  */
  static inline void reset_out_edges_aux (basic_block bb);

  /* Compute the number of case labels that correspond to each outgoing edge of
     STMT.  Record this information in the aux field of the edge.  */
  static void compute_cases_per_edge (gswitch *stmt);

  static bool analyze_switch_statement (gswitch *swtch);

  /* Attempt to expand gimple switch STMT to a decision tree.  */
  static bool try_switch_expansion (gswitch *stmt, vec<cluster *> &clusters,
				    const vec<basic_block> &case_bbs);

  static void record_phi_operand_mapping (const vec<basic_block> bbs,
					  basic_block switch_bb,
					  hash_map <tree, tree> *map);

  static void fix_phi_operands_for_edges (vec<basic_block> case_bbs,
					  hash_map<tree, tree> *phi_mapping);

  /* Generate a decision tree, switching on INDEX_EXPR and jumping to
     one of the labels in CASE_LIST or to the DEFAULT_LABEL.
     DEFAULT_PROB is the estimated probability that it jumps to
     DEFAULT_LABEL.

     We generate a binary decision tree to select the appropriate target
     code.  */
  static void emit (basic_block bb, gswitch *s, tree index_expr,
		    tree index_type,
		    case_tree_node *case_list, basic_block default_bb,
		    profile_probability default_prob);

  /* Take an ordered list of case nodes
     and transform them into a near optimal binary tree,
     on the assumption that any target code selection value is as
     likely as any other.

     The transformation is performed by splitting the ordered
     list into two equal sections plus a pivot.  The parts are
     then attached to the pivot as left and right branches.  Each
     branch is then transformed recursively.  */

  static void balance_case_nodes (case_tree_node **head,
				  case_tree_node *parent);

  /* Dump ROOT, a list or tree of case nodes, to file.  */

  static void dump_case_nodes (FILE *f, case_tree_node *root, int indent_step,
			       int indent_level);

  /* Emit step-by-step code to select a case for the value of INDEX.
     The thus generated decision tree follows the form of the
     case-node binary tree NODE, whose nodes represent test conditions.
     INDEX_TYPE is the type of the index of the switch.  */

  static basic_block emit_case_nodes (basic_block bb, tree index,
				      case_tree_node *node,
				      basic_block default_bb,
				      profile_probability default_prob,
				      tree index_type);

  /* Add an unconditional jump to CASE_BB that happens in basic block BB.  */
  static void emit_jump (basic_block bb, basic_block case_bb);

  static basic_block emit_cmp_and_jump_insns (basic_block bb, tree op0,
					      tree op1, tree_code comparison,
					      basic_block label_bb,
					      profile_probability prob);
};

/*
     Switch initialization conversion

The following pass changes simple initializations of scalars in a switch
statement into initializations from a static array.  Obviously, the values
must be constant and known at compile time and a default branch must be
provided.  For example, the following code:

        int a,b;

        switch (argc)
	{
         case 1:
         case 2:
                a_1 = 8;
                b_1 = 6;
                break;
         case 3:
                a_2 = 9;
                b_2 = 5;
                break;
         case 12:
                a_3 = 10;
                b_3 = 4;
                break;
         default:
                a_4 = 16;
                b_4 = 1;
		break;
        }
	a_5 = PHI <a_1, a_2, a_3, a_4>
	b_5 = PHI <b_1, b_2, b_3, b_4>


is changed into:

        static const int = CSWTCH01[] = {6, 6, 5, 1, 1, 1, 1, 1, 1, 1, 1, 4};
        static const int = CSWTCH02[] = {8, 8, 9, 16, 16, 16, 16, 16, 16, 16,
                                 16, 16, 10};

        if (((unsigned) argc) - 1 < 11)
          {
	    a_6 = CSWTCH02[argc - 1];
            b_6 = CSWTCH01[argc - 1];
	  }
	else
	  {
	    a_7 = 16;
	    b_7 = 1;
          }
	a_5 = PHI <a_6, a_7>
	b_b = PHI <b_6, b_7>

There are further constraints.  Specifically, the range of values across all
case labels must not be bigger than SWITCH_CONVERSION_BRANCH_RATIO (default
eight) times the number of the actual switch branches.

This transformation was contributed by Martin Jambor, see this e-mail:
   http://gcc.gnu.org/ml/gcc-patches/2008-07/msg00011.html  */

/* The main structure of the pass.  */
struct switch_conversion
{
  /* The following function is invoked on every switch statement (the current one
     is given in SWTCH) and runs the individual phases of switch conversion on it
     one after another until one fails or the conversion is completed.
     Returns NULL on success, or a pointer to a string with the reason why the
     conversion failed.  */
  const char *expand (gswitch *swtch);

  void collect (gswitch *swtch);

  /* Checks whether the range given by individual case statements of the SWTCH
     switch statement isn't too big and whether the number of branches actually
     satisfies the size of the new array.  */
  bool check_range ();

  /* Checks whether all but the FINAL_BB basic blocks are empty.  */
  bool check_all_empty_except_final ();

  /* This function checks whether all required values in phi nodes in final_bb
     are constants.  Required values are those that correspond to a basic block
     which is a part of the examined switch statement.  It returns true if the
     phi nodes are OK, otherwise false.  */
  bool check_final_bb ();

  /* The following function allocates default_values, target_{in,out}_names and
     constructors arrays.  The last one is also populated with pointers to
     vectors that will become constructors of new arrays.  */
  void create_temp_arrays ();

  /* Free the arrays created by create_temp_arrays().  The vectors that are
     created by that function are not freed here, however, because they have
     already become constructors and must be preserved.  */
  void free_temp_arrays ();

  /* Populate the array of default values in the order of phi nodes.
     DEFAULT_CASE is the CASE_LABEL_EXPR for the default switch branch
     if the range is non-contiguous or the default case has standard
     structure, otherwise it is the first non-default case instead.  */
  void gather_default_values (tree default_case);

  /* The following function populates the vectors in the constructors array with
     future contents of the static arrays.  The vectors are populated in the
     order of phi nodes.  SWTCH is the switch statement being converted.  */
  void build_constructors ();

  /* If all values in the constructor vector are the same, return the value.
     Otherwise return NULL_TREE.  Not supposed to be called for empty
     vectors.  */
  tree constructor_contains_same_values_p (vec<constructor_elt, va_gc> *vec);

  /* Return type which should be used for array elements, either TYPE's
     main variant or, for integral types, some smaller integral type
     that can still hold all the constants.  */
  tree array_value_type (tree type, int num);

  /* Create an appropriate array type and declaration and assemble a static array
     variable.  Also create a load statement that initializes the variable in
     question with a value from the static array.  SWTCH is the switch statement
     being converted, NUM is the index to arrays of constructors, default values
     and target SSA names for this particular array.  ARR_INDEX_TYPE is the type
     of the index of the new array, PHI is the phi node of the final BB that
     corresponds to the value that will be loaded from the created array.  TIDX
     is an ssa name of a temporary variable holding the index for loads from the
     new array.  */
  void build_one_array (int num, tree arr_index_type,
			gphi *phi, tree tidx);

  /* Builds and initializes static arrays initialized with values gathered from
     the SWTCH switch statement.  Also creates statements that load values from
     them.  */
  void build_arrays ();

  /* Generates and appropriately inserts loads of default values at the position
     given by BSI.  Returns the last inserted statement.  */
  gassign *gen_def_assigns (gimple_stmt_iterator *gsi);

  /* Deletes the unused bbs and edges that now contain the switch statement and
     its empty branch bbs.  BBD is the now dead BB containing the original switch
     statement, FINAL is the last BB of the converted switch statement (in terms
     of succession).  */
  void prune_bbs (basic_block bbd, basic_block final, basic_block default_bb);

  /* Add values to phi nodes in final_bb for the two new edges.  E1F is the edge
     from the basic block loading values from an array and E2F from the basic
     block loading default values.  BBF is the last switch basic block (see the
     bbf description in the comment below).  */
  void fix_phi_nodes (edge e1f, edge e2f, basic_block bbf);

  /* Creates a check whether the switch expression value actually falls into the
     range given by all the cases.  If it does not, the temporaries are loaded
     with default values instead.  SWTCH is the switch statement being converted.

     bb0 is the bb with the switch statement, however, we'll end it with a
     condition instead.

     bb1 is the bb to be used when the range check went ok.  It is derived from
     the switch BB

     bb2 is the bb taken when the expression evaluated outside of the range
     covered by the created arrays.  It is populated by loads of default
     values.

     bbF is a fall through for both bb1 and bb2 and contains exactly what
     originally followed the switch statement.

     bbD contains the switch statement (in the end).  It is unreachable but we
     still need to strip off its edges.  */
  void gen_inbound_check ();

  /* Switch statement for which switch conversion takes place.  */
  gswitch *m_switch;

  /* The expression used to decide the switch branch.  */
  tree index_expr;

  /* The following integer constants store the minimum and maximum value
     covered by the case labels.  */
  tree range_min;
  tree range_max;

  /* The difference between the above two numbers.  Stored here because it
     is used in all the conversion heuristics, as well as for some of the
     transformation, and it is expensive to re-compute it all the time.  */
  tree range_size;

  /* Basic block that contains the actual GIMPLE_SWITCH.  */
  basic_block switch_bb;

  /* Basic block that is the target of the default case.  */
  basic_block default_bb;

  /* The single successor block of all branches out of the GIMPLE_SWITCH,
     if such a block exists.  Otherwise NULL.  */
  basic_block final_bb;

  /* The probability of the default edge in the replaced switch.  */
  profile_probability default_prob;

  /* The count of the default edge in the replaced switch.  */
  profile_count default_count;

  /* Combined count of all other (non-default) edges in the replaced switch.  */
  profile_count other_count;

  /* Number of phi nodes in the final bb (that we'll be replacing).  */
  int phi_count;

  /* Array of default values, in the same order as phi nodes.  */
  tree *default_values;

  /* Constructors of new static arrays.  */
  vec<constructor_elt, va_gc> **constructors;

  /* Array of ssa names that are initialized with a value from a new static
     array.  */
  tree *target_inbound_names;

  /* Array of ssa names that are initialized with the default value if the
     switch expression is out of range.  */
  tree *target_outbound_names;

  /* VOP SSA_NAME.  */
  tree target_vop;

  /* The first load statement that loads a temporary from a new static array.
   */
  gimple *arr_ref_first;

  /* The last load statement that loads a temporary from a new static array.  */
  gimple *arr_ref_last;

  /* String reason why the case wasn't a good candidate that is written to the
     dump file, if there is one.  */
  const char *reason;

  /* True if default case is not used for any value between range_min and
     range_max inclusive.  */
  bool contiguous_range;

  /* True if default case does not have the required shape for other case
     labels.  */
  bool default_case_nonstandard;

  /* Count is number of non-default edges.  */
  unsigned int count;
};

void
switch_decision_tree::reset_out_edges_aux (basic_block bb)
{
  edge e;
  edge_iterator ei;
  FOR_EACH_EDGE (e, ei, bb->succs)
    e->aux = (void *) 0;
}


} // tree_switch_conversion namespace

#endif // TREE_SWITCH_CONVERSION_H
