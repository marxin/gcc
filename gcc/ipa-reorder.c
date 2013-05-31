/* IPA function reordering pass.
   Copyright (C) 2010 Free Software Foundation, Inc.
   Contributed by Jan Hubicka  <jh@suse.cz>

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
#include "cgraph.h"
#include "tree-pass.h"
#include "timevar.h"
#include "gimple.h"
#include "ggc.h"
#include "flags.h"
#include "pointer-set.h"
#include "target.h"
#include "tree-iterator.h"
#include "fibheap.h"

struct priority
{
  gcov_type priority;
  struct partition *src_partition;
  struct partition *dest_partition;
  fibnode_t node;
};

typedef struct priority *priority_ptr;

struct partition
{
  int index;
  vec<cgraph_node_ptr, va_heap, vl_embed> *nodes;
  vec<priority_ptr, va_heap, vl_embed> *priorities;
  vec<priority_ptr, va_heap, vl_embed> *in_priorities;
};

typedef struct partition *partition_ptr;

static bool
cgraph_node_will_be_output_p (struct cgraph_node *node)
{
  return (node->symbol.analyzed && !node->global.inlined_to);
}

static void
account_callees (struct partition *part,
               vec<partition_ptr, va_heap, vl_embed> *partitions,
               struct cgraph_node *node)
{
  struct cgraph_edge *edge;
  struct priority *p;

  for (edge = node->callees; edge; edge = edge->next_callee)
    if (edge->inline_failed)
      {
      if (!edge->callee->aux)
        {
          partition_ptr dest = (*partitions)[edge->callee->uid];
          if (dest && dest != part)
            {
              p = XCNEW (struct priority);
              edge->callee->aux = p;
              p->src_partition = part;
              p->dest_partition = dest;
              vec_safe_push (part->priorities, p);
              vec_safe_push (dest->in_priorities, p);
            }
          else
            continue;
        }
      else
        p = (struct priority *)edge->callee->aux;
      if (edge->count)
        p->priority += edge->count + 1;
      else
        {
          int mul = 1;
          switch (edge->caller->frequency)
            {
            case NODE_FREQUENCY_UNLIKELY_EXECUTED: mul = 0; break;
            case NODE_FREQUENCY_EXECUTED_ONCE: mul = 1; break;
            case NODE_FREQUENCY_NORMAL: mul = 10; break;
            case NODE_FREQUENCY_HOT: mul = 1000; break;
            }
          p->priority += edge->frequency * mul + 1;
        }
      }
    else
      account_callees (part, partitions, edge->callee);

  /* Account taking address of a function as possible call with a low priority.  */
  /* TODO
  for (i = 0; ipa_ref_list_reference_iterate (&node->symbol.ref_list, i, ref); i++)
    if (ref->refered_type == IPA_REF_CGRAPH)
      {
      struct cgraph_node *node2 = ipa_ref_node (ref);
      if (node2->aux)
        {
          partition_ptr dest = (*partitions)[node2->uid];
          if (dest && dest != part)
            {
              p = XCNEW (struct priority);
              node2->aux = p;
              p->src_partition = part;
              p->dest_partition = dest;
              vec_safe_push (part->priorities, p);
              vec_safe_push (dest->in_priorities, p);
            }
          else
            continue;
        }
      else
        p = (struct priority *)node2->aux;
      p->priority++;
      }
  */
}

static void
clear_callees (struct cgraph_node *node)
{
  struct cgraph_edge *edge;

  for (edge = node->callees; edge; edge = edge->next_callee)
    if (edge->inline_failed)
      edge->callee->aux = NULL;
    else
      clear_callees (edge->callee);

  /* TODO */
  /*
  for (i = 0; ipa_ref_list_reference_iterate (&node->symbol.ref_list, i, ref); i++)
    if (ref->refered_type == IPA_REF_CGRAPH)
      ipa_ref_node (ref)->aux = NULL;
  */
}

/* Compare two priority vector entries via indexes of destination
   partitions.  */

static int
priority_cmp (const void *pa, const void *pb)
{
  const struct priority *a = *(const struct priority * const *) pa;
  const struct priority *b = *(const struct priority * const *) pb;
  /* Priorities pointing to dead partitions are removed lazily,
     so just avoid referencing dead memory.  */
  if (!a->priority)
    return ! b->priority ? 0 : -1;
  if (!b->priority)
    return 1;
  return a->dest_partition->index - b->dest_partition->index;
}

/* Compare two priority vector entries via indexes of destination
   partitions.  */

static int
in_priority_cmp (const void *pa, const void *pb)
{
  const struct priority *a = *(const struct priority * const *) pa;
  const struct priority *b = *(const struct priority * const *) pb;
  /* Priorities pointing to dead partitions are removed lazily,
     so just avoid referencing dead memory.  */
  if (!a->priority)
    return ! b->priority ? 0 : -1;
  if (!b->priority)
    return 1;
  return a->src_partition->index - b->src_partition->index;
}

static void
delete_priority (fibheap_t heap, struct priority *p)
{
  p->priority = 0;
  p->src_partition = p->dest_partition = NULL;
  if (p->node)
    fibheap_delete_node (heap, p->node);
  p->node = NULL;
}

static void
merge_partitions (vec<partition_ptr, va_heap, vl_embed> *partitions,
                fibheap_t heap,
                struct partition *part1,
                struct partition *part2)
{
  vec<priority_ptr, va_heap, vl_embed> *priorities = NULL;
  vec<priority_ptr, va_heap, vl_embed> *in_priorities = NULL;
  unsigned int i1 = 0, i2 = 0;
  struct cgraph_node *node;
  unsigned i;

  /* We keep priority lists ordered by indexes of destination partitions
     to allow effective merging.  */
  qsort (part1->priorities->address (),
       vec_safe_length (part1->priorities),
       sizeof (priority_ptr),
       priority_cmp);
  qsort (part2->priorities->address (),
       vec_safe_length (part2->priorities),
       sizeof (priority_ptr),
       priority_cmp);

  /* Merge priority lists, prune out references of partition to itself.
     Assume priority lists are ordered by indexes of destination partitions
     and keep them so.  */
  while (1)
    {
      struct priority *p1, *p2;

      while (i1 < vec_safe_length (part1->priorities)
           && !(*part1->priorities)[i1]->priority)
      i1++;
      while (i2 < vec_safe_length (part2->priorities)
           && !(*part2->priorities)[i2]->priority)
      i2++;

      if (i1 == vec_safe_length (part1->priorities)
        && i2 == vec_safe_length (part2->priorities))
      break;

      if (i1 < vec_safe_length (part1->priorities)
        && (i2 == vec_safe_length (part2->priorities)
            || ((*part1->priorities)[i1]->dest_partition->index
                < (*part2->priorities)[i2]->dest_partition->index)))
      {
        p1 = (*part1->priorities)[i1];
        if (p1->dest_partition != part2)
          vec_safe_push (priorities, p1);
        else
          delete_priority (heap, p1);
        i1++;
      }
      else if (i2 < vec_safe_length (part2->priorities)
             && (i1 == vec_safe_length (part1->priorities)
                 || ((*part1->priorities)[i1]->dest_partition->index
                     > (*part2->priorities)[i2]->dest_partition->index)))
      {
        p2 = (*part2->priorities)[i2];
        if (p2->dest_partition != part1)
          {
            vec_safe_push (priorities, p2);
            p2->src_partition = part1;
          }
        else
          delete_priority (heap, p2);
        i2++;
      }
      else
      {
        p1 = (*part1->priorities)[i1];
        p2 = (*part2->priorities)[i2];
        p1->priority += p2->priority;
        fibheap_replace_key (heap, p1->node, INT_MAX - p1->priority);
        delete_priority (heap, p2);
        vec_safe_push (priorities, p1);
        i1++;
        i2++;
      }
    }
  vec_free (part1->priorities);
  part1->priorities = priorities;

  qsort (part1->in_priorities->address (),
       vec_safe_length (part1->in_priorities),
       sizeof (priority_ptr),
       in_priority_cmp);
  qsort (part2->in_priorities->address (),
       vec_safe_length (part2->in_priorities),
       sizeof (priority_ptr),
       in_priority_cmp);
  i1 = 0;
  i2 = 0;
  while (1)
    {
      struct priority *p1, *p2;
      while (i1 < vec_safe_length (part1->in_priorities)
           && !(*part1->in_priorities)[i1]->priority)
      i1++;
      while (i2 < vec_safe_length (part2->in_priorities)
           && !(*part2->in_priorities)[i2]->priority)
      i2++;

      if (i1 == vec_safe_length (part1->in_priorities)
        && i2 == vec_safe_length (part2->in_priorities))
      break;

      if (i1 < vec_safe_length (part1->in_priorities)
        && (i2 == vec_safe_length (part2->in_priorities)
            || ((*part1->in_priorities)[i1]->src_partition->index
                < (*part2->in_priorities)[i2]->src_partition->index)))
      {
        p1 = (*part1->in_priorities)[i1];
        if (p1->src_partition != part2)
          vec_safe_push (in_priorities, p1);
        else
          delete_priority (heap, p1);
        i1++;
      }
      else if (i2 < vec_safe_length (part2->in_priorities)
             && (i1 == vec_safe_length (part1->in_priorities)
                 || ((*part1->in_priorities)[i1]->src_partition->index
                     > (*part2->in_priorities)[i2]->src_partition->index)))
      {
        p2 = (*part2->in_priorities)[i2];
        if (p2->src_partition != part1)
          {
            vec_safe_push (in_priorities, p2);
            p2->dest_partition = part1;
          }
        else
          delete_priority (heap, p2);
        i2++;
      }
      else
      {
        p1 = (*part1->in_priorities)[i1];
        p2 = (*part2->in_priorities)[i2];
        p1->priority += p2->priority;
        fibheap_replace_key (heap, p1->node, INT_MAX - p1->priority);
        delete_priority (heap, p2);
        vec_safe_push (in_priorities, p1);
        i1++;
        i2++;
      }
    }
  vec_free (part1->in_priorities);
  part1->in_priorities = in_priorities;

  for (i = 0; vec_safe_iterate (part2->nodes, i, &node); i++)
    vec_safe_push (part1->nodes, node);

  (*partitions)[part2->index] = NULL;
  vec_free (part2->priorities);
  vec_free (part2->in_priorities);
  vec_free (part2->nodes);
  free (part2);
}

static void
dump_partition (struct partition *part)
{
  int i;
  struct cgraph_node *node;
  struct priority *p;
  fprintf (dump_file, "  Partition %i:", part->index);
  for (i = 0; vec_safe_iterate (part->nodes, i, &node); i++)
    fprintf (dump_file, "  %s/%i", cgraph_node_name (node), node->uid);
  fprintf (dump_file, "\n    priorities:");
  for (i = 0; vec_safe_iterate (part->priorities, i, &p); i++)
    if (p->priority)
      {
      gcc_assert (p->src_partition == part);
      gcc_assert (p->dest_partition != part);
      fprintf (dump_file, "  %i:%i", p->dest_partition->index,
               (int)p->priority);
      }
  fprintf (dump_file, "\n");
}

static unsigned int
ipa_func_reorder (void)
{
  struct cgraph_node *node;
  vec<partition_ptr, va_heap, vl_embed> *partitions;
  vec_alloc (partitions, cgraph_max_uid);
  int i;
  struct partition *part, *first_part = NULL;
  int freq;
  fibheap_t heap;

  if (!cgraph_max_uid)
    return 0;

  heap = fibheap_new ();
  partitions->quick_grow_cleared (cgraph_max_uid);

  FOR_EACH_DEFINED_FUNCTION (node)
    if (cgraph_node_will_be_output_p (node))
      {
      part = XCNEW (struct partition);
      part->index = node->uid;
      vec_safe_push (part->nodes, node);
      (*partitions)[node->uid] = part;
      }

  if (dump_file)
    fprintf (dump_file, "\n\nCreating partitions\n");
  FOR_EACH_DEFINED_FUNCTION (node)
    if (cgraph_node_will_be_output_p (node))
      {
      part = (*partitions)[node->uid];
      account_callees (part, partitions, node);
      clear_callees (node);
      if (dump_file)
        dump_partition (part);
      }
  FOR_EACH_DEFINED_FUNCTION (node)
    if (cgraph_node_will_be_output_p (node))
      {
      struct priority *p;
      part = (*partitions)[node->uid];

      for (i = 0; vec_safe_iterate (part->priorities, i, &p); i++)
        p->node = fibheap_insert (heap, INT_MAX - p->priority, p);
      if (dump_file)
        dump_partition (part);
      }

  if (dump_file)
    fprintf (dump_file, "\n\nMerging by priorities\n");
  while (!fibheap_empty (heap))
    {
      struct priority *p = (struct priority *)fibheap_extract_min (heap);
      struct partition *part = p->src_partition;
      p->node = NULL;
      if (dump_file)
      {
        fprintf (dump_file,
                 "Concatenating partitions %i and %i, priority %i\n",
                 p->src_partition->index,
                 p->dest_partition->index,
                 (int)p->priority);
        if (dump_file)
          dump_partition (p->src_partition);
        if (dump_file)
          dump_partition (p->dest_partition);
      }
      merge_partitions (partitions, heap, p->src_partition, p->dest_partition);
      if (dump_file)
      dump_partition (part);
    }

  /* We ran out of calls to merge.  Try to arrange remaining partitions
     approximately in execution order: static constructors first, followed
     by partition containing function main ()
     followed by hot sections of the program.  */
  if (dump_file)
    fprintf (dump_file, "\n\nLooking for static constructors\n");
  for (i = 0; vec_safe_iterate (partitions, i, &part); i++)
    if (part && part != first_part
      && DECL_STATIC_CONSTRUCTOR ((*part->nodes)[0]->symbol.decl))
      {
      if (dump_file)
        dump_partition (part);
      if (!first_part)
       first_part = part;
      else
       merge_partitions (partitions, heap, first_part, part);
      }
  if (dump_file)
    fprintf (dump_file, "\n\nLooking for main\n");
  for (i = 0; vec_safe_iterate (partitions, i, &part); i++)
    if (part && part != first_part
      && MAIN_NAME_P (DECL_NAME ((*part->nodes)[0]->symbol.decl)))
      {
      if (dump_file)
        dump_partition (part);
      if (!first_part)
       first_part = part;
      else
       merge_partitions (partitions, heap, first_part, part);
      }
  if (dump_file)
    fprintf (dump_file, "\n\nMerging by frequency\n");
  for (freq = NODE_FREQUENCY_HOT; freq >= NODE_FREQUENCY_UNLIKELY_EXECUTED;
       freq--)
    {
      for (i = 0; vec_safe_iterate (partitions, i, &part); i++)
      if (part && part != first_part
          && (*part->nodes)[0]->frequency == freq)
        {
          if (dump_file)
            dump_partition (part);
          if (!first_part)
           first_part = part;
          else
           merge_partitions (partitions, heap, first_part, part);
        }
    }
  for (i = 0; vec_safe_iterate (partitions, i, &part); i++)
    gcc_assert (!part || part == first_part);

  fibheap_delete (heap);
  if (!first_part)
    return 0;
  if (dump_file)
    {
      fprintf (dump_file, "\n\nFinal order\n");
      dump_partition (first_part);
    }

  /* Store the resulting order and kill the single remaining partition.  */
  for (i = 0; vec_safe_iterate (first_part->nodes, i, &node); i++)
    node->order = i;
  vec_free (first_part->priorities);
  vec_free (first_part->nodes);
  free (first_part);
  return 0;
}

static bool
gate_ipa_func_reorder (void)
{
  return flag_reorder_functions && flag_toplevel_reorder;
}

struct ipa_opt_pass_d pass_ipa_func_reorder =
{
 {
  IPA_PASS,
  "reorder",                          /* name */
  OPTGROUP_NONE,                      /* optinfo_flags */
  gate_ipa_func_reorder,              /* gate */
  ipa_func_reorder,                   /* execute */
  NULL,                                       /* sub */
  NULL,                                       /* next */
  0,                                  /* static_pass_number */
  TV_CGRAPHOPT,                               /* tv_id */
  0,                                  /* properties_required */
  0,                                  /* properties_provided */
  0,                                  /* properties_destroyed */
  0,                                  /* todo_flags_start */
  0                                     /* todo_flags_finish */
 },
 NULL,                                        /* generate_summary */
 NULL,                                        /* write_summary */
 NULL,                                        /* read_summary */
 NULL,                                        /* write_optimization_summary */
 NULL,                                        /* read_optimization_summary */
 NULL,                                        /* stmt_fixup */
 0,                                   /* TODOs */
 NULL,                                        /* function_transform */
 NULL                                 /* variable_transform */
};
