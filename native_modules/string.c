#include "nm_internal.h"

static RuntimeVal *str_upper(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  char *src = ((StringVal *)args[0])->value;
  size_t len = strlen(src);
  char *dst = zox_alloc_buf(ZOX_BUF_STRING, len + 1, "str_upper");
  for (size_t i = 0; i < len; i++) dst[i] = (char)toupper((unsigned char)src[i]);
  dst[len] = '\0';
  RuntimeVal *r = (RuntimeVal *)MK_STRING(dst);
  zox_free_buf(ZOX_BUF_STRING, dst);
  return r;
}

static RuntimeVal *str_lower(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  char *src = ((StringVal *)args[0])->value;
  size_t len = strlen(src);
  char *dst = zox_alloc_buf(ZOX_BUF_STRING, len + 1, "str_lower");
  for (size_t i = 0; i < len; i++) dst[i] = (char)tolower((unsigned char)src[i]);
  dst[len] = '\0';
  RuntimeVal *r = (RuntimeVal *)MK_STRING(dst);
  zox_free_buf(ZOX_BUF_STRING, dst);
  return r;
}

static RuntimeVal *str_trim(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  char *s = ((StringVal *)args[0])->value;
  while (isspace((unsigned char)*s)) s++;
  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
  char *dst = zox_alloc_buf(ZOX_BUF_STRING, len + 1, "str_trim");
  memcpy(dst, s, len);
  dst[len] = '\0';
  RuntimeVal *r = (RuntimeVal *)MK_STRING(dst);
  zox_free_buf(ZOX_BUF_STRING, dst);
  return r;
}

static RuntimeVal *str_starts_with(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  char *s   = ((StringVal *)args[0])->value;
  char *pre = ((StringVal *)args[1])->value;
  return (RuntimeVal *)MK_BOOL(strncmp(s, pre, strlen(pre)) == 0 ? 1 : 0);
}

static RuntimeVal *str_ends_with(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  char *s   = ((StringVal *)args[0])->value;
  char *suf = ((StringVal *)args[1])->value;
  size_t sl = strlen(s), pl = strlen(suf);
  if (pl > sl) return (RuntimeVal *)MK_BOOL(0);
  return (RuntimeVal *)MK_BOOL(strcmp(s + sl - pl, suf) == 0 ? 1 : 0);
}

static RuntimeVal *str_replace(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  char *src  = ((StringVal *)args[0])->value;
  char *from = ((StringVal *)args[1])->value;
  char *to   = ((StringVal *)args[2])->value;
  size_t from_len = strlen(from);
  size_t to_len   = strlen(to);
  if (from_len == 0) return (RuntimeVal *)MK_STRING(src);

  /* count matches -> exact allocation */
  size_t count = 0;
  char *p = src;
  while ((p = strstr(p, from)) != NULL) { count++; p += from_len; }

  size_t src_len = strlen(src);
  size_t new_len = src_len + count * (to_len - from_len) + 1;
  char *dst = zox_alloc_buf(ZOX_BUF_STRING, new_len, "str_replace");
  char *w = dst;
  p = src;
  char *found;
  while ((found = strstr(p, from)) != NULL) {
    size_t chunk = found - p;
    memcpy(w, p, chunk); w += chunk;
    memcpy(w, to, to_len); w += to_len;
    p = found + from_len;
  }
  strcpy(w, p);
  RuntimeVal *r = (RuntimeVal *)MK_STRING(dst);
  zox_free_buf(ZOX_BUF_STRING, dst);
  return r;
}

static RuntimeVal *str_split(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  char *src = ((StringVal *)args[0])->value;
  char *delim = ((StringVal *)args[1])->value;
  size_t delim_len = strlen(delim);
  ListVal *list = MK_LIST(4);

  if (delim_len == 0) {
    /* empty delimiter -> split into chars */
    for (size_t i = 0; src[i]; i++) {
      char buf[2] = {src[i], '\0'};
      RuntimeVal *item = (RuntimeVal *)MK_STRING(buf);
      list_append_val(list, item);
      release(item);
    }
    return (RuntimeVal *)list;
  }

  char *p = src;
  char *found;
  while ((found = strstr(p, delim)) != NULL) {
    size_t chunk = found - p;
    char *buf = zox_alloc_buf(ZOX_BUF_STRING, chunk + 1, "str_split chunk");
    memcpy(buf, p, chunk);
    buf[chunk] = '\0';
    RuntimeVal *item = (RuntimeVal *)MK_STRING(buf);
    zox_free_buf(ZOX_BUF_STRING, buf);
    list_append_val(list, item);
    release(item);
    p = found + delim_len;
  }
  RuntimeVal *last = (RuntimeVal *)MK_STRING(p);
  list_append_val(list, last);
  release(last);
  return (RuntimeVal *)list;
}

static RuntimeVal *str_join(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  ListVal *list   = (ListVal *)args[0];
  char    *delim  = ((StringVal *)args[1])->value;
  size_t   dlen   = strlen(delim);

  size_t total = 1;
  for (size_t i = 0; i < list->size; i++) {
    if (list->items[i]->type != STRING_T) error("join() list must contain only strings");
    total += strlen(((StringVal *)list->items[i])->value);
    if (i + 1 < list->size) total += dlen;
  }
  char *dst = zox_alloc_buf(ZOX_BUF_STRING, total, "str_join");
  char *w = dst;
  for (size_t i = 0; i < list->size; i++) {
    char *s = ((StringVal *)list->items[i])->value;
    size_t slen = strlen(s);
    memcpy(w, s, slen);
    w += slen;
    if (i + 1 < list->size) {
      memcpy(w, delim, dlen);
      w += dlen;
    }
  }
  *w = '\0';
  RuntimeVal *r = (RuntimeVal *)MK_STRING(dst);
  zox_free_buf(ZOX_BUF_STRING, dst);
  return r;
}

static RuntimeVal *str_to_number(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  char *end;
  double val = strtod(((StringVal *)args[0])->value, &end);
  if (end == ((StringVal *)args[0])->value) return (RuntimeVal *)MK_NIL();
  return (RuntimeVal *)MK_NUMBER(val);
}

static RuntimeVal *str_to_string(Environment *env, RuntimeVal **args, size_t argc) {
  (void)env; (void)argc;
  RuntimeVal *val = args[0];
  char buf[64];
  switch (val->type) {
  case STRING_T:
    retain(val);
    return val;
  case NUMBER_T: {
    double n = ((NumberVal *)val)->value;
    if (n >= 0 && n <= 255 && n == (int)n) {
      snprintf(buf, sizeof(buf), "%d", (int)n);
    } else {
      snprintf(buf, sizeof(buf), "%g", n);
    }
    return (RuntimeVal *)MK_STRING(buf);
  }
  case BOOLEAN_T:
    return (RuntimeVal *)MK_STRING(((BooleanVal *)val)->value ? "true" : "false");
  case NIL_T:
    return (RuntimeVal *)MK_STRING("nil");
  default:
    error("toString() cannot convert this type");
    return (RuntimeVal *)MK_NIL();
  }
}

void init_string_module(Environment *env) {
  char *s1[] = {"s"};
  char *s2[] = {"s", "b"};
  char *s3[] = {"s", "from", "to"};

  declare_owned(env, "upper",      (RuntimeVal *)MK_NATIVE_FN(s1, 1, str_upper));
  declare_owned(env, "lower",      (RuntimeVal *)MK_NATIVE_FN(s1, 1, str_lower));
  declare_owned(env, "trim",       (RuntimeVal *)MK_NATIVE_FN(s1, 1, str_trim));
  declare_owned(env, "startsWith", (RuntimeVal *)MK_NATIVE_FN(s2, 2, str_starts_with));
  declare_owned(env, "endsWith",   (RuntimeVal *)MK_NATIVE_FN(s2, 2, str_ends_with));
  declare_owned(env, "replace",    (RuntimeVal *)MK_NATIVE_FN(s3, 3, str_replace));
  declare_owned(env, "split",      (RuntimeVal *)MK_NATIVE_FN(s2, 2, str_split));
  declare_owned(env, "join",       (RuntimeVal *)MK_NATIVE_FN(s2, 2, str_join));
  declare_owned(env, "toNumber",   (RuntimeVal *)MK_NATIVE_FN(s1, 1, str_to_number));
  declare_owned(env, "toString",   (RuntimeVal *)MK_NATIVE_FN(s1, 1, str_to_string));
}
