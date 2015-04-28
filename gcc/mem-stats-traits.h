#ifndef GCC_MEM_STATS_TRAITS_H
#define GCC_MEM_STATS_TRAITS_H

enum mem_alloc_origin
{
  HASH_TABLE,
  HASH_MAP,
  HASH_SET,
  VEC,
  BITMAP,
  GGC,
  MEM_ALLOC_ORIGIN_LENGTH
};

static const char * mem_alloc_origin_names[] = { "Hash tables", "Hash maps", "Hash sets",
  "Heap vectors", "Bitmaps", "GGC memory" };

#endif // GCC_MEM_STATS_TRAITS_H
