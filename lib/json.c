/* External Zox module: JSON parse/stringify.
   Compile: gcc -shared -fPIC -O2 -o json.so json.c -I..
   Use: ~> "./lib/json.so" { parse, stringify }; */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../zox_module.h"

typedef struct {
  const char *src;
  size_t      pos;
} JsonParser;

static void skip_ws(JsonParser *p) {
  while (p->src[p->pos] && isspace((unsigned char)p->src[p->pos]))
    p->pos++;
}

static RuntimeVal *json_parse_value(JsonParser *p);

static RuntimeVal *json_parse_string(JsonParser *p) {
  p->pos++;
  size_t cap = 64, len = 0;
  char *buf = malloc(cap);
  while (p->src[p->pos] && p->src[p->pos] != '"') {
    if (len + 4 >= cap) { cap *= 2; buf = realloc(buf, cap); }
    if (p->src[p->pos] == '\\') {
      p->pos++;
      switch (p->src[p->pos]) {
        case '"':  buf[len++] = '"';  break;
        case '\\': buf[len++] = '\\'; break;
        case '/':  buf[len++] = '/';  break;
        case 'n':  buf[len++] = '\n'; break;
        case 'r':  buf[len++] = '\r'; break;
        case 't':  buf[len++] = '\t'; break;
        case 'b':  buf[len++] = '\b'; break;
        case 'f':  buf[len++] = '\f'; break;
        default:   buf[len++] = p->src[p->pos]; break;
      }
    } else {
      buf[len++] = p->src[p->pos];
    }
    p->pos++;
  }
  buf[len] = '\0';
  p->pos++;
  RuntimeVal *v = (RuntimeVal *)MK_STRING(buf);
  free(buf);
  return v;
}

static RuntimeVal *json_parse_number(JsonParser *p) {
  char tmp[64];
  size_t i = 0;
  if (p->src[p->pos] == '-') tmp[i++] = p->src[p->pos++];
  while (isdigit((unsigned char)p->src[p->pos])) tmp[i++] = p->src[p->pos++];
  if (p->src[p->pos] == '.') {
    tmp[i++] = p->src[p->pos++];
    while (isdigit((unsigned char)p->src[p->pos])) tmp[i++] = p->src[p->pos++];
  }
  if (p->src[p->pos] == 'e' || p->src[p->pos] == 'E') {
    tmp[i++] = p->src[p->pos++];
    if (p->src[p->pos] == '+' || p->src[p->pos] == '-') tmp[i++] = p->src[p->pos++];
    while (isdigit((unsigned char)p->src[p->pos])) tmp[i++] = p->src[p->pos++];
  }
  tmp[i] = '\0';
  return (RuntimeVal *)MK_NUMBER(atof(tmp));
}

static RuntimeVal *json_parse_array(JsonParser *p) {
  p->pos++;
  ListVal *list = MK_LIST(4);
  skip_ws(p);
  if (p->src[p->pos] == ']') { p->pos++; return (RuntimeVal *)list; }
  while (1) {
    skip_ws(p);
    RuntimeVal *item = json_parse_value(p);
    list_append_val(list, item);
    release(item);
    skip_ws(p);
    if (p->src[p->pos] == ',') { p->pos++; continue; }
    if (p->src[p->pos] == ']') { p->pos++; break; }
    break;
  }
  return (RuntimeVal *)list;
}

static RuntimeVal *json_parse_object(JsonParser *p) {
  p->pos++;
  DictVal *dict = MK_DICT(4);
  skip_ws(p);
  if (p->src[p->pos] == '}') { p->pos++; return (RuntimeVal *)dict; }
  while (1) {
    skip_ws(p);
    if (p->src[p->pos] != '"') break;
    RuntimeVal *key_val = json_parse_string(p);
    const char *key = ((StringVal *)key_val)->value;
    skip_ws(p);
    if (p->src[p->pos] == ':') p->pos++;
    skip_ws(p);
    RuntimeVal *val = json_parse_value(p);
    dict_set_val(dict, key, val);
    release(val);
    release(key_val);
    skip_ws(p);
    if (p->src[p->pos] == ',') { p->pos++; continue; }
    if (p->src[p->pos] == '}') { p->pos++; break; }
    break;
  }
  return (RuntimeVal *)dict;
}

static RuntimeVal *json_parse_value(JsonParser *p) {
  skip_ws(p);
  char c = p->src[p->pos];
  if (c == '"')  return json_parse_string(p);
  if (c == '[')  return json_parse_array(p);
  if (c == '{')  return json_parse_object(p);
  if (c == '-' || isdigit((unsigned char)c)) return json_parse_number(p);
  if (strncmp(p->src + p->pos, "true",  4) == 0) { p->pos += 4; return (RuntimeVal *)MK_BOOL(1); }
  if (strncmp(p->src + p->pos, "false", 5) == 0) { p->pos += 5; return (RuntimeVal *)MK_BOOL(0); }
  if (strncmp(p->src + p->pos, "null",  4) == 0) { p->pos += 4; return (RuntimeVal *)MK_NIL(); }
  return (RuntimeVal *)MK_NIL();
}

typedef struct { char *buf; size_t len; size_t cap; } Buf;

static void buf_push(Buf *b, const char *s) {
  size_t sl = strlen(s);
  while (b->len + sl + 1 > b->cap) { b->cap = b->cap * 2 + 64; b->buf = realloc(b->buf, b->cap); }
  memcpy(b->buf + b->len, s, sl);
  b->len += sl;
  b->buf[b->len] = '\0';
}

static void buf_push_char(Buf *b, char c) { char tmp[2] = {c, 0}; buf_push(b, tmp); }

static void stringify_val(RuntimeVal *val, Buf *b);

static void stringify_string(const char *s, Buf *b) {
  buf_push_char(b, '"');
  for (; *s; s++) {
    switch (*s) {
      case '"':  buf_push(b, "\\\""); break;
      case '\\': buf_push(b, "\\\\"); break;
      case '\n': buf_push(b, "\\n");  break;
      case '\r': buf_push(b, "\\r");  break;
      case '\t': buf_push(b, "\\t");  break;
      default:   buf_push_char(b, *s); break;
    }
  }
  buf_push_char(b, '"');
}

static void stringify_val(RuntimeVal *val, Buf *b) {
  switch (val->type) {
    case NIL_T:     buf_push(b, "null"); break;
    case BOOLEAN_T: buf_push(b, ((BooleanVal *)val)->value ? "true" : "false"); break;
    case NUMBER_T: {
      char tmp[64];
      snprintf(tmp, sizeof(tmp), "%g", ((NumberVal *)val)->value);
      buf_push(b, tmp);
      break;
    }
    case STRING_T:
      stringify_string(((StringVal *)val)->value, b);
      break;
    case LIST_T: {
      ListVal *list = (ListVal *)val;
      buf_push_char(b, '[');
      for (size_t i = 0; i < list->size; i++) {
        if (i > 0) buf_push_char(b, ',');
        stringify_val(list->items[i], b);
      }
      buf_push_char(b, ']');
      break;
    }
    case DICT_T: {
      DictVal *dict = (DictVal *)val;
      buf_push_char(b, '{');
      int first = 1;
      for (size_t i = 0; i < dict->capacity; i++) {
        for (Entry *e = dict->entries[i]; e != NULL; e = e->next) {
          if (!first) buf_push_char(b, ',');
          stringify_string(e->key, b);
          buf_push_char(b, ':');
          stringify_val(e->value, b);
          first = 0;
        }
      }
      buf_push_char(b, '}');
      break;
    }
    default:
      buf_push(b, "null");
      break;
  }
}

static RuntimeVal *json_parse(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 1 || args[0]->type != STRING_T) {
    fprintf(stderr, "json.parse: expects one string argument\n");
    return (RuntimeVal *)MK_NIL();
  }
  JsonParser p = { ((StringVal *)args[0])->value, 0 };
  return json_parse_value(&p);
}

static RuntimeVal *json_stringify(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 1) {
    fprintf(stderr, "json.stringify: expects one argument\n");
    return (RuntimeVal *)MK_NIL();
  }
  Buf b = { malloc(64), 0, 64 };
  b.buf[0] = '\0';
  stringify_val(args[0], &b);
  RuntimeVal *result = (RuntimeVal *)MK_STRING(b.buf);
  free(b.buf);
  return result;
}

ZOX_MODULE_INIT {
  char *single[] = {"value"};
  declare_owned(env, "parse",
    (RuntimeVal *)MK_FUNCTION(single, 1, NULL, 0, NULL, json_parse));
  declare_owned(env, "stringify",
    (RuntimeVal *)MK_FUNCTION(single, 1, NULL, 0, NULL, json_stringify));
}
