#include "zox_alloc.h"

#include <string.h>
#include <stdlib.h>
#include "malloc_safe.h"

/* max free list depth per kind when no arena is configured */
#define POOL_CAP 128

typedef struct PoolNode {
  struct PoolNode *next;
} PoolNode;

static PoolNode      *free_lists[ZOX_ALLOC_KIND_COUNT];
static void         (*cleanup_fns[ZOX_ALLOC_KIND_COUNT])(void *ptr);
static ZoxAllocStats  stats[ZOX_ALLOC_KIND_COUNT];

static unsigned char *arena_base   = NULL;
static size_t         arena_size   = 0;
static size_t         arena_offset = 0;
static int            force_heap   = 0;

#define ARENA_OWNS(ptr) (arena_base && (unsigned char *)(ptr) >= arena_base && (unsigned char *)(ptr) < arena_base + arena_size)

void zox_arena_init(size_t bytes) {
  if (arena_base) free(arena_base);
  arena_base   = (unsigned char *)malloc_safe(bytes, "ZoxArena");
  arena_size   = bytes;
  arena_offset = 0;
}

void zox_arena_destroy(void) {
  if (arena_base) free(arena_base);
  arena_base   = NULL;
  arena_size   = 0;
  arena_offset = 0;
}

void *zox_arena_get_base(void) { return (void *)arena_base; }
size_t zox_arena_get_offset(void) { return arena_offset; }

void zox_arena_set_offset(size_t offset) {
  if (offset > arena_offset) return;
  /* Arena objects are never pooled, ensuring safe O(1) reset. */
  arena_offset = offset;
}

int zox_arena_owns(void *ptr) {
  return ARENA_OWNS(ptr);
}

void zox_alloc_force_heap(int force) {
  force_heap = force;
}

static void *arena_bump(size_t size) {
  if (!arena_base) return NULL;
  size_t aligned = (arena_offset + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
  if (aligned + size > arena_size) return NULL;
  void *ptr = arena_base + aligned;
  arena_offset = aligned + size;
  return ptr;
}

void *zox_alloc_obj(ZoxAllocKind kind, size_t size, const char *label) {
  if (kind >= ZOX_ALLOC_KIND_COUNT) return calloc_safe(1, size, label);

  /* Try pool first (heap objects only) */
  PoolNode *node = free_lists[kind];
  if (node) {
    free_lists[kind] = node->next;
    stats[kind].reuse++;
    stats[kind].depth--;
    return node;
  }

  if (kind == ZOX_ALLOC_ENV || force_heap) {
    stats[kind].heap++;
    return calloc_safe(1, size, label);
  }

  stats[kind].alloc++;
  void *ptr = arena_bump(size);
  if (ptr) {
    memset(ptr, 0, size);
    stats[kind].arena++;
    return ptr;
  }
  
  stats[kind].heap++;
  return calloc_safe(1, size, label);
}

void zox_free_obj(ZoxAllocKind kind, void *ptr) {
  if (!ptr) return;
  if (kind >= ZOX_ALLOC_KIND_COUNT) {
    if (!ARENA_OWNS(ptr)) free_safe(ptr);
    return;
  }
  stats[kind].freed++;

  if (ARENA_OWNS(ptr)) return;

  if (stats[kind].depth < POOL_CAP) {
    stats[kind].pooled++;
    stats[kind].depth++;
    PoolNode *node = (PoolNode *)ptr;
    node->next = free_lists[kind];
    free_lists[kind] = node;
  } else {
    if (cleanup_fns[kind]) cleanup_fns[kind](ptr);
    free_safe(ptr);
  }
}

void zox_alloc_set_cleanup_fn(ZoxAllocKind kind, void (*cleanup_fn)(void *ptr)) {
  if (kind < ZOX_ALLOC_KIND_COUNT) cleanup_fns[kind] = cleanup_fn;
}

void zox_alloc_cleanup(void) {
  for (size_t i = 0; i < ZOX_ALLOC_KIND_COUNT; i++) {
    PoolNode *node = free_lists[i];
    while (node) {
      PoolNode *next = node->next;
      if (cleanup_fns[i]) cleanup_fns[i](node);
      free_safe(node);
      node = next;
    }
    free_lists[i] = NULL;
    stats[i].depth = 0;
  }
}

static const char *kind_names[ZOX_ALLOC_KIND_COUNT] = {
  "NumberVal", "StringVal", "ListVal", "DictVal",
  "TypeVal", "StructVal", "FunctionVal", "Environment",
};

void zox_alloc_report(FILE *out) {
  fprintf(out, "pool  %-12s  %8s  %8s  %8s  %8s  %8s  %8s  %6s\n",
          "kind", "alloc", "reuse", "arena", "heap", "freed", "pooled", "depth");
  for (size_t i = 0; i < ZOX_ALLOC_KIND_COUNT; i++) {
    fprintf(out, "pool  %-12s  %8zu  %8zu  %8zu  %8zu  %8zu  %8zu  %6zu\n",
            kind_names[i],
            stats[i].alloc, stats[i].reuse, stats[i].arena,
            stats[i].heap,  stats[i].freed,  stats[i].pooled,
            stats[i].depth);
  }
  if (arena_base) {
    fprintf(out, "arena used=%zukB / total=%zukB\n",
            arena_offset / 1024, arena_size / 1024);
  }
}
