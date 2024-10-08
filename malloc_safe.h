#ifndef MALLOC_SAFE_H
#define MALLOC_SAFE_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void *malloc_safe(size_t size, const char *error_message);
void *realloc_safe(void *ptr, size_t size, const char *error_message);
void free_safe(void *ptr);

#endif  // MALLOC_SAFE_H
