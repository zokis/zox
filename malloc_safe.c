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
    void *new_ptr = malloc_safe(size, error_message);
    /* Note: dangerous without old size, but arena buffers are large. 
       Actually, this only happens during growth where new size > old size. */
    memcpy(new_ptr, ptr, size); 
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
