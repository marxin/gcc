#ifndef GCC_MEM_STATS_H
#define GCC_MEM_STATS_H

#include "hash-map-traits.h"
#include "inchash.h"
#include "mem-stats-traits.h"
#include "vec.h"

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
    inchash::hash hash;

    hash.add_ptr (m_filename);
    hash.add_ptr (m_function);
    hash.add_int (m_line);

    return hash.end ();
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
    return mem_alloc_origin_names[(unsigned) origin];
  }
};

struct mem_usage
{
  mem_usage (): m_allocated (0), m_times (0), m_peak (0) {}
  mem_usage (size_t allocated, size_t times, size_t peak):
    m_allocated (allocated), m_times (times), m_peak (peak) {} 

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

  mem_usage operator+ (const mem_usage &second)
  {
    return mem_usage (m_allocated + second.m_allocated,
		      m_times + second.m_times,
		      m_peak + second.m_peak);
  }

  inline bool operator< (const mem_usage &second) const
  {
    return (m_allocated == second.m_allocated ?
	    (m_peak == second.m_peak ? m_times < second.m_times
	     : m_peak < second.m_peak ) : m_allocated < second.m_allocated);
  }

  static int compare (const void *first, const void *second)
  {
    typedef std::pair<mem_location *, mem_usage *> mem_pair_t;

    const mem_pair_t f = *(const mem_pair_t *)first;
    const mem_pair_t s = *(const mem_pair_t *)second;

    return (*f.second) < (*s.second);
  }

  inline void dump (mem_location *loc) const
  {
    char s[4096];
    sprintf (s, "%s:%i (%s)", loc->get_trimmed_filename (),
	     loc->m_line, loc->m_function);

    s[48] = '\0';

    fprintf (stderr, "%-48s %10li%10li%10li\n", s, (long)m_allocated, (long)m_peak, (long)m_times);
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
  typedef std::pair <mem_location *, T *> mem_list_t;

  mem_alloc_description ();
  bool contains_descriptor_for_instance (const void *ptr);
  T *register_descriptor (const void *ptr, mem_alloc_origin origin, const char *name, int line, const char *function);
  T *register_overhead (size_t size, mem_alloc_origin origin, const char *name, int line,
			const char *function, const void *ptr);
  T *register_instance_overhead (size_t size, const void *ptr);
  void release_overhead_for_instance (void *ptr, size_t size);
  void dump ();
  T get_total ();
  mem_list_t *get_list (mem_alloc_origin origin, unsigned *length);

  mem_location m_location;
  mem_map_t *m_map;
  reverse_mem_map_t *m_reverse_map;
};

#include "hash-map.h"


template <class T>
inline bool 
mem_alloc_description<T>::contains_descriptor_for_instance (const void *ptr)
{
  return m_reverse_map->get (ptr);
}

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
  mem_usage_pair <T> **slot = m_reverse_map->get (ptr);
  if (!slot)
    {
      fprintf (stderr, "problem tu je..\n");
      return NULL;
    }

  T *usage = (*slot)->usage;
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
mem_alloc_description<T>::release_overhead_for_instance (void *ptr, size_t size)
{
  mem_usage_pair<T> **slot = m_reverse_map->get (ptr);

  if (!slot)
    {
      fprintf (stderr, "tady je taky problem\n");
      return;
    }

  mem_usage_pair<T> *usage_pair = *slot;
  
  gcc_assert (size <= usage_pair->usage->m_allocated);

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


template <class T>
inline 
typename mem_alloc_description<T>::mem_list_t *
mem_alloc_description<T>::get_list (mem_alloc_origin origin, unsigned *length)
{
  /* vec data structure is not used because all vectors generate memory
     allocation info a it would create a cycle.  */
  size_t element_size = sizeof (mem_list_t);
  mem_list_t *list = XCNEWVEC (mem_list_t, m_map->elements ());
  unsigned i = 0;

  for (typename mem_map_t::iterator it = m_map->begin(); it != m_map->end (); ++it)
    if ((*it).first->m_origin == origin)
      list[i++] = std::pair<mem_location*, T*> (*it);

  qsort (list, i, element_size, T::compare);
  *length = i;

  return list;
}

#endif // GCC_MEM_STATS_H
