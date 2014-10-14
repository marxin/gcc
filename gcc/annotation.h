/* Annotations handling code.
   Copyright (C) 2014 Free Software Foundation, Inc.
   Contributed by Martin Liska

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

#ifndef GCC_ANNOTATION_H
#define GCC_ANNOTATION_H

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "varasm.h"
#include "calls.h"
#include "print-tree.h"
#include "tree-inline.h"
#include "langhooks.h"
#include "hashtab.h"
#include "toplev.h"
#include "flags.h"
#include "debug.h"
#include "target.h"
#include "cgraph.h"
#include "hash-map.h"

#define ANNOTATION_DELETED_VALUE -1
#define ANNOTATION_EMPTY_VALUE 0

struct annotation_hashmap_traits: default_hashmap_traits
{
  static inline
  hashval_t hash (const int v)
  {
    return (hashval_t)v;
  }

  template<typename T>
  static inline
  bool is_deleted (T &e)
  {
    return e.m_key == ANNOTATION_DELETED_VALUE;
  }

  template<typename T>
  static inline
  bool is_empty (T &e)
  {
    return e.m_key == ANNOTATION_EMPTY_VALUE;
  }

  template<typename T>
  static inline
  void mark_deleted (T &e)
  {
    e.m_key = ANNOTATION_DELETED_VALUE;
  }

  template<typename T>
  static inline
  void mark_empty (T &e)
  {
    e.m_key = ANNOTATION_EMPTY_VALUE;
  }
};

template <class T>
class cgraph_annotation
{
public:
  cgraph_annotation (symbol_table *symtab)
  {
    cgraph_node *node;

    FOR_EACH_FUNCTION (node)
    {
      gcc_assert (node->annotation_uid > 0);
      m_reverse_map.put (node, node->annotation_uid);
    }

    m_map = new  hash_map<int, T*, annotation_hashmap_traits>();

    m_symtab_insertion_hook =
      symtab->add_cgraph_insertion_hook
      (cgraph_annotation::symtab_insertion, this);

    m_symtab_removal_hook =
      symtab->add_cgraph_removal_hook
      (cgraph_annotation::symtab_removal, this);
    m_symtab_duplication_hook =
      symtab->add_cgraph_duplication_hook
      (cgraph_annotation::symtab_duplication, this);

  }

  ~cgraph_annotation ()
  {
    m_symtab->remove_cgraph_insertion_hook (m_symtab_insertion_hook);
    m_symtab->remove_cgraph_removal_hook (m_symtab_removal_hook);
    m_symtab->remove_cgraph_duplication_hook (m_symtab_duplication_hook);

    m_map->traverse <void *, cgraph_annotation::release> (NULL);
  }

  template<typename Arg, bool (*f)(const T &, Arg)>
  void traverse (Arg a) const
  {
    m_map->traverse <f> (a);
  }

  template <void (*f) (const cgraph_node *, T *)>
  inline void add_insertion_hook (void)
  {
    m_insertion_hooks.safe_push (f);
  }

  template <void (*f) (const cgraph_node *, T *)>
  inline void add_removal_hook (void)
  {
    m_removal_hooks.safe_push (f);
  }

  template <void (*f) (const cgraph_node *, const cgraph_node *, T *, T *)>
  inline void add_duplication_hook (void)
  {
    m_duplication_hooks.safe_push (f);
  }


  inline T* get_or_add (int uid)
  {
    T **v = m_map->get (uid);
    if (!v)
      {
	T *new_value = new T();
	m_map->put (uid, new_value);

	v = &new_value;
      }

    return *v;
  }

  inline T *get_or_add (cgraph_node *node)
  {
    return get_or_add (node->annotation_uid);
  }

  inline void call_insertion_hooks (cgraph_node *node)
  {
    for (unsigned int i = 0; i < m_insertion_hooks.length (); i++)
      m_insertion_hooks[i] (node, get_or_add (node));
  }

  inline void call_removal_hooks (cgraph_node *node, T *v)
  {
    for (unsigned int i = 0; i < m_removal_hooks.length (); i++)
      m_removal_hooks[i] (node, v);
  }

  inline void call_duplication_hooks (cgraph_node *node, cgraph_node *node2, T *v)
  {
    for (unsigned int i = 0; i < m_duplication_hooks.length (); i++)
      m_duplication_hooks[i] (node, node2, v, get_or_add (node2));
  }

  static void symtab_insertion (cgraph_node *node, void *data)
  {
    cgraph_annotation *annotation = (cgraph_annotation <T> *) (data);
    annotation->call_insertion_hooks (node);
  }

  static void symtab_removal (cgraph_node *node, void *data)
  {
    cgraph_annotation *annotation = (cgraph_annotation <T> *) (data);
    int *annotation_uid_ptr = annotation->m_reverse_map.get (node);

    if (!annotation_uid_ptr)
      return;

    int annotation_uid = *annotation_uid_ptr;

    T **v = annotation->m_map->get (annotation_uid);

    if (v)
      annotation->call_removal_hooks (node, *v);

    annotation->m_reverse_map.remove (node);

    if (annotation->m_map->get (annotation_uid))
      annotation->m_map->remove (annotation_uid);

  }

  static void symtab_duplication (cgraph_node *node, cgraph_node *node2,
				  void *data)
  {
    cgraph_annotation *annotation = (cgraph_annotation <T> *) (data);
    T **v = annotation->m_map->get (node->annotation_uid);

    gcc_assert (node2->annotation_uid > 0);
    annotation->m_reverse_map.put (node2, node2->annotation_uid);

    if (v)
      {
	T *data = *v;
	T *duplicate = new T();
	annotation->m_map->put (node2->annotation_uid, duplicate);
	annotation->call_duplication_hooks (node, node2, data);
      }

  }

  hash_map <int, T *, annotation_hashmap_traits> *m_map;
  hash_map <cgraph_node *, int> m_reverse_map;

private:
  inline void remove (int uid)
  {
    T *v = m_map->get (uid);

    if (v)
      m_map->erase (uid);
  }

  static bool release (int const &, T * const &v, void *)
  {
    delete (v);
    return true;
  }

  auto_vec <void (*) (cgraph_node *, T *)> m_insertion_hooks;
  auto_vec <void (*) (cgraph_node *, T *)> m_removal_hooks;
  auto_vec <void (*) (const cgraph_node *, const cgraph_node *, T *, T *)>
  m_duplication_hooks;

  cgraph_node_hook_list *m_symtab_insertion_hook;
  cgraph_node_hook_list *m_symtab_removal_hook;
  cgraph_2node_hook_list *m_symtab_duplication_hook;

  symbol_table *m_symtab;
};

#endif  /* GCC_ANNOTATION_H  */
