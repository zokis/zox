#include "malloc_safe.h"

void *malloc_safe(size_t size, const char *error_message) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Memory allocation error: %s\n", error_message);
        exit(1);
    }
    return ptr;
}

void *realloc_safe(void *ptr, size_t size, const char *error_message) {
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL) {
        fprintf(stderr, "Memory reallocation error: %s\n", error_message);
        exit(1);
    }
    return new_ptr;
}

void free_safe(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}
