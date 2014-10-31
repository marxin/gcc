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
  /* Default construction takes SYMTAB as an argument.  */
  cgraph_annotation (symbol_table *symtab): m_symtab (symtab)
  {
    cgraph_node *node;

    FOR_EACH_FUNCTION (node)
    {
      gcc_assert (node->annotation_uid > 0);
    }

    m_map = new hash_map<int, T*, annotation_hashmap_traits>();

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

  /* Destructor.  */
  virtual ~cgraph_annotation ()
  {
    m_symtab->remove_cgraph_insertion_hook (m_symtab_insertion_hook);
    m_symtab->remove_cgraph_removal_hook (m_symtab_removal_hook);
    m_symtab->remove_cgraph_duplication_hook (m_symtab_duplication_hook);

    m_map->traverse <void *, cgraph_annotation::release> (NULL);
  }

  /* Traverses all annotations with a function F called with
     ARG as argument.  */
  template<typename Arg, bool (*f)(const T &, Arg)>
  void traverse (Arg a) const
  {
    m_map->traverse <f> (a);
  }

  /* Basic implementation of insertion hook.  */
  virtual void insertion_hook (const cgraph_node *, T *) {}

  /* Basic implementation of removal hook.  */
  virtual void removal_hook (const cgraph_node *, T *) {}

  /* Basic implementation of duplication hook.  */
  virtual void duplication_hook (const cgraph_node *,
				 const cgraph_node *, T *, T *) {}

  /* Getter for annotation callgraph ID.  */
  inline T* operator[] (int uid)
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

  /* Getter for annotation callgraph node pointer.  */
  inline T * operator[] (cgraph_node *node)
  {
    return operator[] (node->annotation_uid);
  }

  /* Symbol insertion hook that is registered to symbol table.  */
  static void symtab_insertion (cgraph_node *node, void *data)
  {
    cgraph_annotation *annotation = (cgraph_annotation <T> *) (data);
    annotation->insertion_hook (node, (*annotation)[node]);
  }

  /* Symbol removal hook that is registered to symbol table.  */
  static void symtab_removal (cgraph_node *node, void *data)
  {
    gcc_assert (node->annotation_uid);
    cgraph_annotation *annotation = (cgraph_annotation <T> *) (data);

    int annotation_uid = node->annotation_uid;
    T **v = annotation->m_map->get (annotation_uid);

    if (v)
      {
	annotation->removal_hook (node, *v);
	delete (*v);
      }

    if (annotation->m_map->get (annotation_uid))
      annotation->m_map->remove (annotation_uid);
  }

  /* Symbol duplication hook that is registered to symbol table.  */
  static void symtab_duplication (cgraph_node *node, cgraph_node *node2,
				  void *data)
  {
    cgraph_annotation *annotation = (cgraph_annotation <T> *) (data);
    T **v = annotation->m_map->get (node->annotation_uid);

    gcc_assert (node2->annotation_uid > 0);

    if (v)
      {
	T *data = *v;
	T *duplicate = new T();
	annotation->m_map->put (node2->annotation_uid, duplicate);
	annotation->duplication_hook (node, node2, data, (*annotation)[node2]);
      }
  }

private:
  /* Remove annotation for annotation UID.  */
  inline void remove (int uid)
  {
    T *v = m_map->get (uid);

    if (v)
      m_map->erase (uid);
  }

  /* Annotation class release function called by traverse method.  */
  static bool release (int const &, T * const &v, void *)
  {
    delete (v);
    return true;
  }

  /* Main annotation store, where annotation ID is used as key.  */
  hash_map <int, T *, annotation_hashmap_traits> *m_map;

  /* Internal annotation insertion hook pointer.  */
  cgraph_node_hook_list *m_symtab_insertion_hook;
  /* Internal annotation removal hook pointer.  */
  cgraph_node_hook_list *m_symtab_removal_hook;
  /* Internal annotation duplication hook pointer.  */
  cgraph_2node_hook_list *m_symtab_duplication_hook;

  /* Symbol table the annotation is registered to.  */
  symbol_table *m_symtab;
};

#endif  /* GCC_ANNOTATION_H  */
