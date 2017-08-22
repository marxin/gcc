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

} // tree_switch_conversion namespace

#endif // TREE_SWITCH_CONVERSION_H
