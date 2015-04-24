#ifndef GCC_MEM_STATS_H
#define GCC_MEM_STATS_H

#include "hash-map-traits.h"
#include "inchash.h"
#include "mem-stats-traits.h"

template<typename Key, typename Value,
	 typename Traits = default_hashmap_traits>
class hash_map;

struct mem_location
{
  const char *m_filename;
  const char *m_function;
  int m_line;
  mem_alloc_origin m_origin;

  mem_location () {}

  mem_location (const char *filename, const char *function, int line,
		mem_alloc_origin origin):
    m_filename (filename), m_function (function), m_line (line), m_origin
    (origin) {}

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

  static const char *get_origin_name (mem_alloc_origin origin)
  {
    switch (origin)
      {
      case HASH_TABLE:
	return "hash table";
      case HASH_MAP:
	return "hash map";
      case HASH_SET:
	return "hash set";
      case VEC:
	return "vec";
      case BITMAP:
	return "bitmap";
      default:
	gcc_unreachable ();
      }
  }
};

struct mem_usage
{
  mem_usage (): m_allocated (0), m_times (0), m_peak (0) {}

  size_t m_allocated;
  size_t m_times;
  size_t m_peak;

  inline void register_overhead (size_t size)
    {
      m_allocated += size;  
      m_times++;

      if (m_peak < m_allocated)
	m_peak = m_allocated;
    }
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
  T *register_descriptor (const void *ptr, mem_alloc_origin origin, const char *name, int line, const char *function);
  T *register_overhead (size_t size, mem_alloc_origin origin, const char *name, int line,
			const char *function, const void *ptr);
  T *register_instance_overhead (size_t size, const void *ptr);
  void release_overhead (void *ptr);
  void release_overhead_for_instance (void *ptr, size_t size);
  void dump ();
  T get_total ();

  mem_location m_location;
  mem_map_t *m_map;
  reverse_mem_map_t *m_reverse_map;
};

#include "hash-map.h"

template <class T>
inline T* 
mem_alloc_description<T>::register_descriptor (const void *ptr, mem_alloc_origin origin, const char *filename, int line, const char *function)
{  
  mem_location *l = new mem_location (filename, function, line, origin);
  T *usage = NULL;

  T **slot = m_map->get (l);
  if (slot)
    {
      delete l;
      usage = *slot;
    }
  else
    {
      usage = new T ();
      m_map->put (l, usage);
    }

  if (!m_reverse_map->get (ptr))
    m_reverse_map->put (ptr, new mem_usage_pair<T> (usage, 0));

  return usage;
}


template <class T>
inline T* 
mem_alloc_description<T>::register_instance_overhead (size_t size, const void *ptr)
{
  mem_usage *usage = (*m_reverse_map->get (ptr))->usage;
  usage->register_overhead (size);

  return usage;
}

template <class T>
inline T* 
mem_alloc_description<T>::register_overhead (size_t size, mem_alloc_origin origin, const char *filename, int line, const char *function, const void *ptr)
{
  T *usage = register_descriptor (ptr, origin, filename, line, function);
  usage->register_overhead (size);

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
mem_alloc_description<T>::release_overhead_for_instance (void *ptr, size_t size)
{
  mem_usage_pair<T> **slot = m_reverse_map->get (ptr);
  mem_usage_pair<T> *usage_pair = *slot;


  // TODO
  if (!(size <= usage_pair->usage->m_allocated))
    {
    fprintf (stderr, "XXX: fix release: %lu/%lu\n",
	     size, usage_pair->usage->m_allocated);
    return;
    }

  usage_pair->usage->m_allocated -= size;
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
