#include "global.h"

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "malloc_safe.h"
#include "values.h"

ErrorCursor      error_cursor = {0, 0, NULL};
int    zox_argc = 0;
char **zox_argv = NULL;

unsigned short int compare_runtimeval(RuntimeVal *a, RuntimeVal *b);

unsigned short int compare_lists(ListVal *a, ListVal *b) {
  if (a->size != b->size) return 0;
  for (size_t i = 0; i < a->size; i++) {
    if (!compare_runtimeval(a->items[i], b->items[i])) return 0;
  }
  return 1;
}

unsigned short int compare_dicts(DictVal *a, DictVal *b) {
  if (a->size != b->size) return 0;
  for (size_t i = 0; i < a->capacity; i++) {
    if (a->entries[i].key != NULL) {
      Entry *found = dict_find_entry(b, a->entries[i].key);
      if (found == NULL || !compare_runtimeval(a->entries[i].value, found->value)) {
        return 0;
      }
    }
  }
  return 1;
}

unsigned short int compare_runtimeval(RuntimeVal *a, RuntimeVal *b) {
  if (a->type != b->type) return 0;
  switch (a->type) {
    case NUMBER_T:  return ((NumberVal *)a)->value == ((NumberVal *)b)->value;
    case BOOLEAN_T: return ((BooleanVal *)a)->value == ((BooleanVal *)b)->value;
    case STRING_T:  return strcmp(((StringVal *)a)->value, ((StringVal *)b)->value) == 0;
    case LIST_T:    return compare_lists((ListVal *)a, (ListVal *)b);
    case DICT_T:    return compare_dicts((DictVal *)a, (DictVal *)b);
  }
  return 0;
}

ListVal *dict_to_keys(DictVal *dict) {
  ListVal *keys_list = MK_LIST(dict->size);
  for (size_t i = 0; i < dict->capacity; i++) {
    if (dict->entries[i].key != NULL) {
      keys_list->items[keys_list->size++] = (RuntimeVal *)MK_STRING(dict->entries[i].key);
    }
  }
  return keys_list;
}

unsigned short int contains(RuntimeVal *obj, RuntimeVal *value) {
  ListVal *list;
  int allocated = 0;
  if (obj->type == LIST_T) {
    list = (ListVal *)obj;
  } else if (obj->type == DICT_T) {
    list = dict_to_keys((DictVal *)obj);
    allocated = 1;
  } else {
    error("contains function only works on lists and dictionaries");
    return 0;
  }
  unsigned short int found = 0;
  for (size_t i = 0; i < list->size; i++) {
    if (compare_runtimeval(list->items[i], value)) {
      found = 1;
      break;
    }
  }
  if (allocated) release((RuntimeVal *)list);
  return found;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
  char *bufptr = NULL;
  char *p;
  size_t size;
  int c;

  if (!lineptr || !stream || !n) return -1;
  bufptr = *lineptr;
  size   = *n;
  c = fgetc(stream);
  if (c == EOF) return -1;
  if (bufptr == NULL) {
    bufptr = malloc_safe(128, "getline buffer");
    if (!bufptr) return -1;
    size = 128;
  }
  p = bufptr;
  while (c != EOF) {
    if ((p - bufptr) > (ssize_t)(size - 1)) {
      size += 128;
      bufptr = realloc_safe(bufptr, size, "getline buffer");
      if (!bufptr) return -1;
    }
    *p++ = c;
    if (c == '\n') break;
    c = fgetc(stream);
  }
  *p++ = '\0';
  *lineptr = bufptr;
  *n = size;
  return p - bufptr - 1;
}

void error(const char *message) {
  if (error_cursor.line > 0) {
    const char *file = error_cursor.file ? error_cursor.file : "<stdin>";
    fprintf(stderr, "Error at %s:%d:%d\n%s\n",
            file, error_cursor.line, error_cursor.column, message);
  } else {
    fprintf(stderr, "Error: %s\n", message);
  }
  if (global_context.is_repl) {
    longjmp(global_context.error_jmp, 1);
  } else {
    exit(1);
  }
}

void parser_error(const char *message, Token *token, TokenType type) {
  const char *file = error_cursor.file ? error_cursor.file : "<stdin>";
  fprintf(stderr, "Parser Error at %s:%d:%d\n%s\n(got '%s', expected token type %d)\n",
          file, token->line, token->column, message, token->value, type);
  if (global_context.is_repl) {
    longjmp(global_context.error_jmp, 1);
  } else {
    exit(1);
  }
}

char *read_file(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) { perror("Could not open file"); return NULL; }
  size_t buffer_size   = 1024;
  size_t content_size  = 1;
  char  *buffer        = malloc_safe(buffer_size, "read_file buffer");
  if (!buffer) { perror("Could not allocate buffer"); fclose(file); return NULL; }
  char  *current_position = buffer;
  size_t bytes_read;
  while ((bytes_read = fread(current_position, 1, buffer_size - content_size, file)) > 0) {
    content_size     += bytes_read;
    current_position += bytes_read;
    if (content_size + 1 >= buffer_size) {
      buffer_size *= 2;
      char *new_buffer = realloc_safe(buffer, buffer_size, "read_file buffer");
      if (!new_buffer) {
        perror("Could not reallocate buffer");
        free_safe(buffer); fclose(file); return NULL;
      }
      current_position = new_buffer + (current_position - buffer);
      buffer = new_buffer;
    }
  }
  if (ferror(file)) {
    perror("Error reading file");
    free_safe(buffer); fclose(file); return NULL;
  }
  buffer[content_size - 1] = '\0';
  fclose(file);
  return buffer;
}
