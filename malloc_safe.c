#include "malloc_safe.h"
#include "zox_alloc.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void *malloc_safe(size_t size, const char *error_message) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "Memory allocation error: %s\n", error_message);
    exit(1);
  }
  return ptr;
}

void *calloc_safe(size_t num, size_t size, const char *error_message) {
  void *ptr = calloc(num, size);
  if (ptr == NULL) {
    fprintf(stderr, "Memory allocation error: %s\n", error_message);
    exit(1);
  }
  return ptr;
}

void *realloc_safe(void *ptr, size_t size, const char *error_message) {
  if (ptr && zox_arena_owns(ptr)) {
    /* Manual realloc for arena memory: move to heap. */
    void *new_ptr = malloc_safe(size, error_message);
    
    /* Safe copy: we don't know the exact old size, but it cannot exceed 
       the current arena offset or the new requested size. */
    size_t arena_offset = zox_arena_get_offset();
    unsigned char *base = (unsigned char *)zox_arena_get_base();
    size_t ptr_offset = (unsigned char *)ptr - base;
    size_t max_copy = arena_offset - ptr_offset;
    size_t copy_size = (size < max_copy) ? size : max_copy;

    memcpy(new_ptr, ptr, copy_size); 
    return new_ptr;
  }
  void *new_ptr = realloc(ptr, size);
  if (new_ptr == NULL) {
    fprintf(stderr, "Memory reallocation error: %s\n", error_message);
    exit(1);
  }
  return new_ptr;
}

void free_safe(void *ptr) {
  if (ptr && !zox_arena_owns(ptr)) {
    free(ptr);
  }
}
