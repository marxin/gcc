/* Reorder functions based on profile.
   Copyright (C) 2019 Free Software Foundation, Inc.

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "tree-pass.h"
#include "cgraph.h"
#include "alloc-pool.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "alloc-pool.h"
#include "ipa-prop.h"
#include "ipa-fnsummary.h"
#include "fibonacci_heap.h"
#include <limits>

using namespace std;

namespace {

#define C3_CLUSTER_THRESHOLD 1024

struct cluster_edge;

/* Cluster is set of functions considered in C3 algorithm.  */

struct cluster
{
  cluster (cgraph_node *node, int size, sreal time):
    m_functions (), m_callers (), m_size (size), m_time (time)
  {
    m_functions.safe_push (node);
  }

  vec<cgraph_node *> m_functions;
  hash_map <cluster *, cluster_edge *> m_callers;
  int m_size;
  sreal m_time;
};

/* Cluster edge is an oriented edge in between two clusters.  */

struct cluster_edge
{
  cluster_edge (cluster *caller, cluster *callee, uint64_t count):
    m_caller (caller), m_callee (callee), m_count (count), m_heap_node (NULL)
  {}


  uint64_t inverted_count ()
  {
    return numeric_limits<uint64_t>::max () - m_count;
  }

  cluster *m_caller;
  cluster *m_callee;
  uint64_t m_count;
  fibonacci_node<uint64_t, cluster_edge> *m_heap_node;
};

/* Sort functions based of first execution of the function.  */

static void
sort_functions_by_first_run (void)
{
  cgraph_node *node;

  FOR_EACH_DEFINED_FUNCTION (node)
    if (node->tp_first_run && !node->alias)
      node->text_sorted_order = node->tp_first_run;
}

/* Compare clusters by density after that are established.  */

static int
cluster_cmp (const void *a_p, const void *b_p)
{
  const cluster *a = *(cluster * const *)a_p;
  const cluster *b = *(cluster * const *)b_p;

  unsigned fncounta = a->m_functions.length ();
  unsigned fncountb = b->m_functions.length ();
  if (fncounta <= 1 || fncountb <= 1)
    return fncountb - fncounta;

  sreal r = b->m_time * a->m_size - a->m_time * b->m_size;
  return (r < 0) ? -1 : ((r > 0) ? 1 : 0);
}

/* Visit callgraph edge CS until we reach a real cgraph_node (not a clone).
   Record such edge to EDGES or traverse recursively.  */

static void
visit_all_edges_for_caller (auto_vec<cluster_edge *> *edges,
			    cgraph_node *node, cgraph_edge *cs)
{
  if (cs->inline_failed)
    {
      gcov_type count;
      profile_count pcount = cs->count.ipa ();
      /* A real call edge.  */
      if (!cs->callee->alias
	  && cs->callee->definition
	  && pcount.initialized_p ()
	  && (count = pcount.to_gcov_type ()) > 0)
	{
	  cluster *caller = (cluster *)node->aux;
	  cluster *callee = (cluster *)cs->callee->aux;
	  cluster_edge **cedge = callee->m_callers.get (caller);

	  if (dump_file && (dump_flags & TDF_DETAILS))
	    fprintf (dump_file, "Adding edge:%s->%s:%" PRIu64 "\n",
		     node->dump_name (), cs->callee->dump_name (), count);

	  if (cedge != NULL)
	    (*cedge)->m_count += count;
	  else
	    {
	      cluster_edge *cedge = new cluster_edge (caller, callee, count);
	      edges->safe_push (cedge);
	      callee->m_callers.put (caller, cedge);
	    }
	}
    }
  else
    {
      cgraph_node *clone = cs->callee;
      for (cgraph_edge *cs = clone->callees; cs; cs = cs->next_callee)
	visit_all_edges_for_caller (edges, node, cs);
    }
}

/* Sort functions based on call chain clustering, which is an algorithm
   mentioned in the following article:
   https://research.fb.com/wp-content/uploads/2017/01/cgo2017-hfsort-final1.pdf
   .  */

static void
sort_functions_by_c3 (void)
{
  cgraph_node *node;
  auto_vec<cluster *> clusters;

  /* Create a cluster for each function.  */
  FOR_EACH_DEFINED_FUNCTION (node)
    if (!node->alias
	&& !node->inlined_to)
      {
	if (dump_file && (dump_flags & TDF_DETAILS))
	  fprintf (dump_file, "Adding node:%s\n", node->dump_name ());

	ipa_size_summary *size_summary = ipa_size_summaries->get (node);
	ipa_fn_summary *fn_summary = ipa_fn_summaries->get (node);
	cluster *c = new cluster (node, size_summary->size, fn_summary->time);
	node->aux = c;
	clusters.safe_push (c);
      }

  auto_vec<cluster_edge *> edges;

  /* Insert edges between clusters that have a profile.  */
  for (unsigned i = 0; i < clusters.length (); i++)
    {
      cgraph_node *node = clusters[i]->m_functions[0];
      for (cgraph_edge *cs = node->callees; cs; cs = cs->next_callee)
	visit_all_edges_for_caller (&edges, node, cs);
    }

  /* Now insert all created edges into a heap.  */
  fibonacci_heap <uint64_t, cluster_edge> heap (0);

  for (unsigned i = 0; i < clusters.length (); i++)
    {
      cluster *c = clusters[i];
      for (hash_map<cluster *, cluster_edge *>::iterator it
	   = c->m_callers.begin (); it != c->m_callers.end (); ++it)
	{
	  cluster_edge *cedge = (*it).second;
	  cedge->m_heap_node = heap.insert (cedge->inverted_count (), cedge);
	}
    }

  while (!heap.empty ())
    {
      cluster_edge *cedge = heap.extract_min ();
      cluster *caller = cedge->m_caller;
      cluster *callee = cedge->m_callee;
      cedge->m_heap_node = NULL;

      if (dump_file && (dump_flags & TDF_DETAILS))
	{
	  fprintf (dump_file, "Processing cluster edge: %p->%p, count: %"
		   PRIu64 "\n", (void *)caller, (void *)callee, cedge->m_count);
	  fprintf (dump_file, "  source functions (%u): ", caller->m_size);
	  for (unsigned j = 0; j < caller->m_functions.length (); j++)
	    fprintf (dump_file, "%s ", caller->m_functions[j]->dump_name ());
	  fprintf (dump_file, "\n  target functions (%u): ", callee->m_size);
	  for (unsigned j = 0; j < callee->m_functions.length (); j++)
	    fprintf (dump_file, "%s ", callee->m_functions[j]->dump_name ());
	  fprintf (dump_file, "\n");
	}

      if (caller == callee)
	continue;
      if (caller->m_size + callee->m_size <= C3_CLUSTER_THRESHOLD)
	{
	  if (dump_file && (dump_flags & TDF_DETAILS))
	    fprintf (dump_file, "  (clusters merged)\n");

	  caller->m_size += callee->m_size;
	  caller->m_time += callee->m_time;

	  /* Append all cgraph_nodes from callee to caller.  */
	  for (unsigned i = 0; i < callee->m_functions.length (); i++)
	    caller->m_functions.safe_push (callee->m_functions[i]);

	  callee->m_functions.truncate (0);

	  /* Iterate all cluster_edges of callee and add them to the caller.  */
	  for (hash_map<cluster *, cluster_edge *>::iterator it
	       = callee->m_callers.begin (); it != callee->m_callers.end ();
	       ++it)
	    {
	      (*it).second->m_callee = caller;
	      cluster_edge **ce = caller->m_callers.get ((*it).first);

	      if (ce != NULL)
		{
		  (*ce)->m_count += (*it).second->m_count;
		  if ((*ce)->m_heap_node != NULL)
		    heap.decrease_key ((*ce)->m_heap_node,
				       (*ce)->inverted_count ());
		}
	      else
		caller->m_callers.put ((*it).first, (*it).second);
	    }
	}
      else if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, "  (clusters too big to be merged)\n");
    }

  /* Sort the candidate clusters.  */
  clusters.qsort (cluster_cmp);

  /* Dump clusters.  */
  if (dump_file)
    {
      for (unsigned i = 0; i < clusters.length (); i++)
	{
	  cluster *c = clusters[i];
	  if (c->m_functions.length () == 0)
	    continue;

	  fprintf (dump_file, "\nCluster %d with functions: %d, size: %d,"
		   " density: %f\n", i, c->m_functions.length (), c->m_size,
		   (c->m_time / c->m_size).to_double ());
	  fprintf (dump_file, "  functions: ");
	  for (unsigned j = 0; j < c->m_functions.length (); j++)
	    fprintf (dump_file, "%s ", c->m_functions[j]->dump_name ());
	  fprintf (dump_file, "\n");
	}
      fprintf (dump_file, "\n");
    }

  /* Assign .text.sorted.* section names.  */
  int counter = 1;
  for (unsigned i = 0; i < clusters.length (); i++)
    {
      cluster *c = clusters[i];
      if (c->m_functions.length () <= 1)
	continue;

      for (unsigned j = 0; j < c->m_functions.length (); j++)
	{
	  cgraph_node *node = c->m_functions[j];

	  if (dump_file)
	    fprintf (dump_file, "setting: %d for %s with size:%d\n",
		     counter, node->dump_asm_name (),
		     ipa_size_summaries->get (node)->size);
	  node->text_sorted_order = counter++;
	}
    }

  /* Release memory.  */
  FOR_EACH_DEFINED_FUNCTION (node)
    if (!node->alias)
      node->aux = NULL;

  for (unsigned i = 0; i < clusters.length (); i++)
    delete clusters[i];

  for (unsigned i = 0; i < edges.length (); i++)
    delete edges[i];
}

/* The main function for function sorting.  */

static unsigned int
ipa_reorder (void)
{
  switch (flag_reorder_functions_algorithm)
    {
    case REORDER_FUNCTIONS_ALGORITHM_CALL_CHAIN_CLUSTERING:
      sort_functions_by_c3 ();
      break;
    case REORDER_FUNCTIONS_ALGORITHM_FIRST_RUN:
      sort_functions_by_first_run ();
      break;
    default:
      gcc_unreachable ();
    }

  return 0;
}

const pass_data pass_data_ipa_reorder =
{
  IPA_PASS, /* type */
  "reorder", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_IPA_REORDER, /* tv_id */
  0, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

class pass_ipa_reorder : public ipa_opt_pass_d
{
public:
  pass_ipa_reorder (gcc::context *ctxt)
    : ipa_opt_pass_d (pass_data_ipa_reorder, ctxt,
		      NULL, /* generate_summary */
		      NULL, /* write_summary */
		      NULL, /* read_summary */
		      NULL, /* write_optimization_summary */
		      NULL, /* read_optimization_summary */
		      NULL, /* stmt_fixup */
		      0, /* function_transform_todo_flags_start */
		      NULL, /* function_transform */
		      NULL) /* variable_transform */
  {}

  /* opt_pass methods: */
  virtual bool gate (function *);
  virtual unsigned int execute (function *) { return ipa_reorder (); }

}; // class pass_ipa_reorder

bool
pass_ipa_reorder::gate (function *)
{
  return flag_profile_reorder_functions && flag_profile_use && flag_wpa;
}

} // anon namespace

ipa_opt_pass_d *
make_pass_ipa_reorder (gcc::context *ctxt)
{
  return new pass_ipa_reorder (ctxt);
}
