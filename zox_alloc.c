#include "zox_alloc.h"

#include "malloc_safe.h"

/* max free list depth per kind when no arena is configured */
#define POOL_CAP 64

typedef struct PoolNode {
  struct PoolNode *next;
} PoolNode;

static PoolNode      *free_lists[ZOX_ALLOC_KIND_COUNT];
static ZoxAllocStats  stats[ZOX_ALLOC_KIND_COUNT];

static unsigned char *arena_base   = NULL;
static size_t         arena_size   = 0;
static size_t         arena_offset = 0;

void zox_arena_init(size_t bytes) {
  free_safe(arena_base);
  arena_base   = (unsigned char *)malloc_safe(bytes, "ZoxArena");
  arena_size   = bytes;
  arena_offset = 0;
}

void zox_arena_destroy(void) {
  free_safe(arena_base);
  arena_base   = NULL;
  arena_size   = 0;
  arena_offset = 0;
}

static int arena_owns(void *ptr) {
  unsigned char *p = (unsigned char *)ptr;
  return arena_base && p >= arena_base && p < arena_base + arena_size;
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
  if (kind >= ZOX_ALLOC_KIND_COUNT) return malloc_safe(size, label);
  stats[kind].alloc++;
  PoolNode *node = free_lists[kind];
  if (node) {
    free_lists[kind] = node->next;
    stats[kind].reuse++;
    stats[kind].depth--;
    return node;
  }
  void *ptr = arena_bump(size);
  if (ptr) { stats[kind].arena++; return ptr; }
  stats[kind].heap++;
  return malloc_safe(size, label);
}

void zox_free_obj(ZoxAllocKind kind, void *ptr) {
  if (!ptr) return;
  if (kind >= ZOX_ALLOC_KIND_COUNT) { free_safe(ptr); return; }
  stats[kind].freed++;
  /* arena mode: pool only arena-owned ptrs, free overflow immediately.
     no-arena mode: pool up to POOL_CAP, free the rest. */
  int pool_it = arena_base ? arena_owns(ptr) : (stats[kind].depth < POOL_CAP);
  if (pool_it) {
    stats[kind].pooled++;
    stats[kind].depth++;
    PoolNode *node = (PoolNode *)ptr;
    node->next = free_lists[kind];
    free_lists[kind] = node;
  } else {
    free_safe(ptr);
  }
}

void zox_alloc_cleanup(void) {
  for (size_t i = 0; i < ZOX_ALLOC_KIND_COUNT; i++) {
    PoolNode *node = free_lists[i];
    while (node) {
      PoolNode *next = node->next;
      if (!arena_owns(node)) free_safe(node);  /* Free heap objects */
      node = next;
    }
    free_lists[i] = NULL;
    stats[i].depth = 0;
  }
  /* Arena buffer and its contained objects are freed by zox_arena_destroy */
}

static const char *kind_names[ZOX_ALLOC_KIND_COUNT] = {
  "NumberVal", "StringVal", "ListVal", "DictVal",
  "Entry", "FunctionVal", "Environment",
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
