#ifndef ZOX_ALLOC_H
#define ZOX_ALLOC_H

#ifndef ZOX_ALLOC_STATS
#define ZOX_ALLOC_STATS 0
#endif

#include <stddef.h>
#include <stdio.h>

typedef enum {
  ZOX_ALLOC_NUMBER,
  ZOX_ALLOC_STRING,
  ZOX_ALLOC_LIST,
  ZOX_ALLOC_DICT,
  ZOX_ALLOC_TYPE,
  ZOX_ALLOC_STRUCT,
  ZOX_ALLOC_FUNCTION,
  ZOX_ALLOC_ENV,
  ZOX_ALLOC_KIND_COUNT
} ZoxAllocKind;

typedef enum {
  ZOX_BUF_STRING,
  ZOX_BUF_DICT_KEY,
  ZOX_BUF_LIST_ITEMS,
  ZOX_BUF_DICT_ENTRIES,
  ZOX_BUF_ENV_ENTRIES,
  ZOX_BUF_TYPE_FIELDS,
  ZOX_BUF_STRUCT_VALUES,
  ZOX_BUF_SCOPE_NAME,
  ZOX_BUF_AST,
  ZOX_BUF_TEMP,
  ZOX_BUF_IO,
  ZOX_BUF_MISC,
  ZOX_BUF_KIND_COUNT
} ZoxBufKind;

typedef struct {
  size_t alloc;  /* total zox_alloc_obj calls */
  size_t reuse;  /* served from free list */
  size_t arena;  /* served by arena bump */
  size_t heap;   /* served by malloc (overflow or no-arena) */
  size_t freed;  /* total zox_free_obj calls */
  size_t pooled; /* returned to free list */
  size_t depth;  /* current free list depth */
} ZoxAllocStats;

void  zox_arena_init(size_t bytes);
void  zox_arena_destroy(void);
void *zox_arena_get_base(void);
size_t zox_arena_get_offset(void);
void   zox_arena_set_offset(size_t offset);
int    zox_arena_owns(void *ptr);
void   zox_alloc_force_heap(int force);
void *zox_alloc_obj(ZoxAllocKind kind, size_t size, const char *label);
void  zox_free_obj(ZoxAllocKind kind, void *ptr);
void *zox_alloc_buf(ZoxBufKind kind, size_t size, const char *label);
void *zox_calloc_buf(ZoxBufKind kind, size_t count, size_t size, const char *label);
void *zox_realloc_buf(ZoxBufKind kind, void *ptr, size_t size, const char *label);
char *zox_strdup_buf(ZoxBufKind kind, const char *str);
void  zox_free_buf(ZoxBufKind kind, void *ptr);
void  zox_alloc_set_cleanup_fn(ZoxAllocKind kind, void (*cleanup_fn)(void *ptr));
void  zox_alloc_cleanup(void);
void  zox_alloc_report(FILE *out);

#endif  // ZOX_ALLOC_H
