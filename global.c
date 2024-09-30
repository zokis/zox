#include "global.h"

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "malloc_safe.h"
#include "values.h"

extern ExecutionContext global_context;

unsigned short int compare_runtimeval(RuntimeVal *a, RuntimeVal *b);

unsigned short int compare_lists(ListVal *a, ListVal *b) {
  if (a->size != b->size) {
    return 0;
  }
  for (size_t i = 0; i < a->size; i++) {
    if (!compare_runtimeval(a->items[i], b->items[i])) {
      return 0;
    }
  }
  return 1;
}

unsigned short int compare_dicts(DictVal *a, DictVal *b) {
  if (a->size != b->size) {
    return 0;
  }
  for (size_t i = 0; i < a->size; i++) {
    if (strcmp(a->entries[i]->key, b->entries[i]->key) != 0) {
      return 0;
    }
    if (!compare_runtimeval(a->entries[i]->value, b->entries[i]->value)) {
      return 0;
    }
  }
  return 1;
}

unsigned short int compare_runtimeval(RuntimeVal *a, RuntimeVal *b) {
  if (a->type != b->type) {
    return 0;
  }
  switch (a->type) {
    case NUMBER_T:
      return ((NumberVal *)a)->value == ((NumberVal *)b)->value;
    case BOOLEAN_T:
      return ((BooleanVal *)a)->value == ((BooleanVal *)b)->value;
    case STRING_T:
      return strcmp(((StringVal *)a)->value, ((StringVal *)b)->value) == 0;
    case LIST_T:
      return compare_lists((ListVal *)a, (ListVal *)b);
    case DICT_T:
      return compare_dicts((DictVal *)a, (DictVal *)b);
  }
  return 0;
}

unsigned short int contains(ListVal *list, RuntimeVal *value) {
  for (size_t i = 0; i < list->size; i++) {
    if (compare_runtimeval(list->items[i], value)) {
      return 1;
    }
  }
  return 0;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
  char *bufptr = NULL;
  char *p = bufptr;
  size_t size;
  int c;

  if (lineptr == NULL) {
    return -1;
  }
  if (stream == NULL) {
    return -1;
  }
  if (n == NULL) {
    return -1;
  }
  bufptr = *lineptr;
  size = *n;

  c = fgetc(stream);
  if (c == EOF) {
    return -1;
  }
  if (bufptr == NULL) {
    bufptr = malloc_safe(128, "getline buffer");
    if (bufptr == NULL) {
      return -1;
    }
    size = 128;
  }
  p = bufptr;
  while (c != EOF) {
    if ((p - bufptr) > (size - 1)) {
      size = size + 128;
      bufptr = realloc_safe(bufptr, size, "getline buffer");
      if (bufptr == NULL) {
        return -1;
      }
    }
    *p++ = c;
    if (c == '\n') {
      break;
    }
    c = fgetc(stream);
  }

  *p++ = '\0';
  *lineptr = bufptr;
  *n = size;

  return p - bufptr - 1;
}

void error(const char *message) {
  fprintf(stderr, "Error: %s\n", message);
  if (global_context.is_repl) {
    longjmp(global_context.error_jmp, 1);
  } else {
    exit(1);
  }
}

void parser_error(const char *message, Token *token, TokenType type) {
  fprintf(
      stderr,
      "Parser Error:\n%s\nToken: %s - Expecting: %d\nLine: %d; Column: %d\n",
      message, token->value, type, token->line, token->column);
  if (global_context.is_repl) {
    longjmp(global_context.error_jmp, 1);
  } else {
    exit(1);
  }
}

char *read_file(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    perror("Could not open file");
    return NULL;
  }
  size_t buffer_size = 1024;
  size_t content_size = 1;
  char *buffer = malloc_safe(buffer_size, "read_file buffer");
  if (!buffer) {
    perror("Could not allocate buffer");
    fclose(file);
    return NULL;
  }

  char *current_position = buffer;
  size_t bytes_read;

  while ((bytes_read = fread(current_position, 1, buffer_size - content_size,
                             file)) > 0) {
    content_size += bytes_read;
    current_position += bytes_read;
    if (content_size + 1 >= buffer_size) {
      buffer_size *= 2;
      char *new_buffer = realloc_safe(buffer, buffer_size, "read_file buffer");
      if (!new_buffer) {
        perror("Could not reallocate buffer");
        free_safe(buffer);
        fclose(file);
        return NULL;
      }
      current_position = new_buffer + (current_position - buffer);
      buffer = new_buffer;
    }
  }
  if (ferror(file)) {
    perror("Error reading file");
    free_safe(buffer);
    fclose(file);
    return NULL;
  }
  buffer[content_size - 1] = '\0';
  fclose(file);
  return buffer;
}