#ifndef GCC_MEM_STATS_H
#define GCC_MEM_STATS_H

#include "hash-map-traits.h"
#include "inchash.h"

template<typename Key, typename Value,
	 typename Traits = default_hashmap_traits>
class hash_map;

struct mem_location
{
  const char *m_filename;
  const char *m_function;
  int m_line;

  mem_location () {}

  mem_location (const char *filename, const char *function, int line):
    m_filename (filename), m_function (function), m_line (line) {}

  hashval_t hash ()
  {
    return 123;
  }

  int equal (mem_location &other)
  {
    return m_filename == other.m_filename && m_function == other.m_function
      && m_line == other.m_line;
  }

  const char *get_trimmed_filename ()
    {
      const char *s1 = m_filename;
      const char *s2;

      while ((s2 = strstr (s1, "gcc/")))
	s1 = s2 + 4;

      return s1;
    }
};

struct mem_usage
{
  mem_usage (): m_allocated (0), m_times (0), m_peak (0) {}

  size_t m_allocated;
  size_t m_times;
  size_t m_peak;
};

template <class T>
struct mem_usage_pair
{
  mem_usage_pair (T *usage_, size_t allocated_): usage (usage_),
  allocated (allocated_) {}

  T *usage;
  size_t allocated;
};

template <class T>
class mem_alloc_description
{
public:
  struct mem_alloc_hashmap_traits: default_hashmap_traits
  {
    static hashval_t
    hash (const mem_location *l)
    {
	inchash::hash hstate;

	hstate.add_ptr ((const void *)l->m_filename);
	hstate.add_ptr (l->m_function);
	hstate.add_int (l->m_line);

	return hstate.end ();
    }

    static bool
    equal_keys (const mem_location *l1, const mem_location *l2)
    {
      return l1->m_filename == l2->m_filename && l1->m_function == l2->m_function
	&& l1->m_line == l2->m_line;
    }
  };


  typedef hash_map <mem_location *, T *, mem_alloc_hashmap_traits>
    mem_map_t;
  typedef hash_map <const void *, mem_usage_pair<T> *, default_hashmap_traits> reverse_mem_map_t;

  mem_alloc_description ();
  T *get_descriptor (const char *name, int line, const char *function);
  T *register_overhead (size_t size, const char *name, int line,
			const char *function, const void *ptr);
  void release_overhead (void *ptr);
  void dump ();
  T get_total ();

  mem_location m_location;
  mem_map_t *m_map;
  reverse_mem_map_t *m_reverse_map;
};

#include "hash-map.h"

template <class T>
inline T* 
mem_alloc_description<T>::get_descriptor (const char *filename, int line, const char *function)
{  
  mem_location *l = new mem_location (filename, function, line);
  T *usage = NULL;

  T **slot = m_map->get (l);
  if (slot)
    {
      // TODO
      // delete l;
      usage = *slot;
    }
  else
    {
      usage = new T ();
      m_map->put (l, usage);
    }

  return usage;
}

template <class T>
inline T* 
mem_alloc_description<T>::register_overhead (size_t size, const char *filename, int line, const char *function, const void *ptr)
{
  T *usage = get_descriptor (filename, line, function);

  usage->m_allocated += size;  
  usage->m_times++;

  if (usage->m_peak < usage->m_allocated)
    usage->m_peak = usage->m_allocated;

  if (!m_reverse_map->get (ptr))
    m_reverse_map->put (ptr, new mem_usage_pair<T> (usage, size));

  return usage;
}

template <class T>
inline void
mem_alloc_description<T>::release_overhead (void *ptr)
{
//  mem_usage_pair<T> **slot = mem_ptr_map.get (ptr);
//  mem_usage_pair<T> *usage_pair = *slot;
//  usage_pair->usage->m_allocated -= usage_pair->allocated;
}

template <class T>
inline void
mem_alloc_description<T>::dump ()
{
  fprintf (stderr, "XXX\n");
}

template <class T>
inline T 
mem_alloc_description<T>::get_total ()
{
  T u;

  for (typename mem_map_t::iterator it = m_map->begin(); it != m_map->end (); ++it)
    {
      u.m_times += (*it).second->m_times;
      u.m_allocated += (*it).second->m_allocated;
    }

  return u;
}

template <class T>
inline 
mem_alloc_description<T>::mem_alloc_description()
{
  m_map = new mem_map_t (13, false, false);
  m_reverse_map = new reverse_mem_map_t (13, false, false);
}

#endif // GCC_MEM_STATS_H
