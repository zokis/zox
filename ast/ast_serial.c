/* Binary AST cache serialization (.zoxc). */
#include "../ast.h"
#include "../malloc_safe.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_u8(FILE *f, uint8_t v)   { fwrite(&v, 1, 1, f); }
static void write_u32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void write_u64(FILE *f, uint64_t v) { fwrite(&v, 8, 1, f); }
static void write_f64(FILE *f, double v)   { fwrite(&v, 8, 1, f); }

static void write_str(FILE *f, const char *s) {
  if (!s) { write_u32(f, 0); return; }
  uint32_t len = (uint32_t)strlen(s);
  write_u32(f, len);
  fwrite(s, 1, len, f);
}

static uint8_t  read_u8(FILE *f)  { uint8_t v;  size_t _r = fread(&v, 1, 1, f); (void)_r; return v; }
static uint32_t read_u32(FILE *f) { uint32_t v; size_t _r = fread(&v, 4, 1, f); (void)_r; return v; }
static uint64_t read_u64(FILE *f) { uint64_t v; size_t _r = fread(&v, 8, 1, f); (void)_r; return v; }
static double   read_f64(FILE *f) { double v;   size_t _r = fread(&v, 8, 1, f); (void)_r; return v; }

static char *read_str(FILE *f) {
  uint32_t len = read_u32(f);
  if (len == 0) return NULL;
  char *s = malloc_safe(len + 1, "read_str");
  { size_t _r = fread(s, 1, len, f); (void)_r; }
  s[len] = '\0';
  return s;
}

static void serialize_node(FILE *f, Stmt *node);

static void serialize_expr(FILE *f, Expr *e) {
  if (!e) { write_u8(f, 255); return; }
  serialize_node(f, (Stmt *)e);
}

static void serialize_body(FILE *f, Stmt **body, size_t count) {
  write_u32(f, (uint32_t)count);
  for (size_t i = 0; i < count; i++) serialize_node(f, body[i]);
}

static void serialize_node(FILE *f, Stmt *node) {
  if (!node) { write_u8(f, 255); return; }
  write_u8(f, (uint8_t)node->kind);

  switch (node->kind) {
  case ProgramAst: {
    Program *p = (Program *)node;
    serialize_body(f, p->body, p->body_count);
    break;
  }
  case NumericLiteralAst:
    write_f64(f, ((NumericLiteral *)node)->value);
    break;
  case StringLiteralAst:
    write_str(f, ((StringLiteral *)node)->value);
    break;
  case BooleanLiteralAst:
    write_u8(f, (uint8_t)((BooleanLiteral *)node)->value);
    break;
  case NilAst:
    break;
  case IdentifierAst:
    write_str(f, ((Identifier *)node)->symbol);
    break;
  case UnaryExprAst: {
    UnaryExpr *u = (UnaryExpr *)node;
    write_str(f, u->operator);
    serialize_expr(f, u->expr);
    break;
  }
  case BinaryExprAst: {
    BinaryExpr *b = (BinaryExpr *)node;
    serialize_expr(f, b->left);
    serialize_expr(f, b->right);
    write_str(f, b->operator);
    break;
  }
  case VarDeclarationAst: {
    VarDeclaration *v = (VarDeclaration *)node;
    write_str(f, v->varname);
    serialize_expr(f, v->value);
    break;
  }
  case AssignVarAst: {
    AssignVar *a = (AssignVar *)node;
    write_str(f, a->varname);
    serialize_expr(f, a->value);
    break;
  }
  case AssignListVarAst: {
    AssignListVar *a = (AssignListVar *)node;
    write_str(f, a->varname);
    serialize_expr(f, a->index);
    serialize_expr(f, a->value);
    break;
  }
  case AssignDictVarAst: {
    AssignDictVar *a = (AssignDictVar *)node;
    write_str(f, a->varname);
    serialize_expr(f, a->key);
    serialize_expr(f, a->value);
    break;
  }
  case AssignListExprAst: {
    AssignListExpr *a = (AssignListExpr *)node;
    serialize_expr(f, a->target);
    serialize_expr(f, a->index);
    serialize_expr(f, a->value);
    break;
  }
  case AssignDictExprAst: {
    AssignDictExpr *a = (AssignDictExpr *)node;
    serialize_expr(f, a->target);
    serialize_expr(f, a->key);
    serialize_expr(f, a->value);
    break;
  }
  case IfAst: {
    IfExpr *e = (IfExpr *)node;
    serialize_expr(f, e->condition);
    serialize_body(f, e->body, e->body_count);
    serialize_body(f, e->else_body, e->else_body_count);
    serialize_expr(f, (Expr *)e->else_if);
    break;
  }
  case WhileAst: {
    WhileExpr *w = (WhileExpr *)node;
    serialize_expr(f, w->condition);
    serialize_body(f, w->body, w->body_count);
    break;
  }
  case ForAst: {
    ForExpr *fo = (ForExpr *)node;
    serialize_expr(f, fo->initialization);
    serialize_expr(f, fo->condition);
    serialize_expr(f, fo->increment);
    serialize_body(f, fo->body, fo->body_count);
    break;
  }
  case FuncDefAst: {
    FuncDef *fd = (FuncDef *)node;
    write_str(f, fd->name);
    write_u32(f, (uint32_t)fd->param_count);
    for (size_t i = 0; i < fd->param_count; i++) write_str(f, fd->params[i]);
    serialize_body(f, fd->body, fd->body_count);
    break;
  }
  case CallExprAst: {
    CallExpr *c = (CallExpr *)node;
    serialize_expr(f, c->callee);
    write_u32(f, (uint32_t)c->arg_count);
    for (size_t i = 0; i < c->arg_count; i++) serialize_expr(f, c->arguments[i]);
    break;
  }
  case ListLiteralAst: {
    ListLiteral *l = (ListLiteral *)node;
    write_u32(f, (uint32_t)l->element_count);
    for (size_t i = 0; i < l->element_count; i++) serialize_expr(f, l->elements[i]);
    break;
  }
  case DictLiteralAst: {
    DictLiteral *d = (DictLiteral *)node;
    write_u32(f, (uint32_t)d->element_count);
    for (size_t i = 0; i < d->element_count; i++) {
      serialize_expr(f, d->keys[i]);
      serialize_expr(f, d->values[i]);
    }
    break;
  }
  case ListIndexAst: {
    ListIndex *l = (ListIndex *)node;
    serialize_expr(f, l->list);
    serialize_expr(f, l->start);
    serialize_expr(f, l->end);
    write_u8(f, (uint8_t)l->is_slice);
    break;
  }
  case DictKeyAst: {
    DictKey *d = (DictKey *)node;
    serialize_expr(f, d->dict);
    serialize_expr(f, d->key);
    break;
  }
  case ImportAst: {
    ImportStmt *imp = (ImportStmt *)node;
    write_str(f, imp->module_name);
    write_u32(f, (uint32_t)imp->import_count);
    for (size_t i = 0; i < imp->import_count; i++) {
      write_str(f, imp->imports[i]->name);
      write_str(f, imp->imports[i]->alias);
    }
    break;
  }
  case BreakAst:
  case ContinueAst:
    break;
  case ReturnAst:
    serialize_expr(f, ((ReturnStmt *)node)->value);
    break;
  case ArenaBlockAst: {
    ArenaBlockExpr *a = (ArenaBlockExpr *)node;
    serialize_body(f, a->body, a->body_count);
    break;
  }
  default:
    break;
  }
}

static Stmt *deserialize_node(FILE *f);

static Expr *deserialize_expr(FILE *f) {
  return (Expr *)deserialize_node(f);
}

static Stmt **deserialize_body(FILE *f, size_t *count_out) {
  uint32_t count = read_u32(f);
  *count_out = count;
  if (count == 0) return NULL;
  Stmt **body = malloc_safe(sizeof(Stmt *) * count, "deserialize_body");
  for (uint32_t i = 0; i < count; i++) body[i] = deserialize_node(f);
  return body;
}

static Stmt *deserialize_node(FILE *f) {
  uint8_t kind_byte = read_u8(f);
  if (kind_byte == 255) return NULL;
  NodeType kind = (NodeType)kind_byte;

  switch (kind) {
  case ProgramAst: {
    size_t count;
    Stmt **body = deserialize_body(f, &count);
    return (Stmt *)create_program(body, count);
  }
  case NumericLiteralAst:
    return (Stmt *)create_numeric_literal(read_f64(f));
  case StringLiteralAst: {
    char *s = read_str(f);
    Stmt *n = (Stmt *)create_string_literal(s ? s : "");
    free_safe(s);
    return n;
  }
  case BooleanLiteralAst:
    return (Stmt *)create_boolean_literal(read_u8(f));
  case NilAst:
    return (Stmt *)create_nil_literal();
  case IdentifierAst: {
    char *sym = read_str(f);
    Stmt *n = (Stmt *)create_identifier(sym ? sym : "");
    free_safe(sym);
    return n;
  }
  case UnaryExprAst: {
    char *op = read_str(f);
    Expr *expr = deserialize_expr(f);
    Stmt *n = (Stmt *)create_unary_expr(op ? op : "", expr);
    free_safe(op);
    return n;
  }
  case BinaryExprAst: {
    Expr *left  = deserialize_expr(f);
    Expr *right = deserialize_expr(f);
    char *op    = read_str(f);
    Stmt *n = (Stmt *)create_binary_expr(left, right, op ? op : "");
    free_safe(op);
    return n;
  }
  case VarDeclarationAst: {
    char *name = read_str(f);
    Expr *val  = deserialize_expr(f);
    Stmt *n = (Stmt *)create_var_expr(name ? name : "", val);
    free_safe(name);
    return n;
  }
  case AssignVarAst: {
    char *name = read_str(f);
    Expr *val  = deserialize_expr(f);
    Stmt *n = (Stmt *)assign_var_expr(name ? name : "", val);
    free_safe(name);
    return n;
  }
  case AssignListVarAst: {
    char *name = read_str(f);
    Expr *idx  = deserialize_expr(f);
    Expr *val  = deserialize_expr(f);
    Stmt *n = (Stmt *)assign_list_expr(name ? name : "", idx, val);
    free_safe(name);
    return n;
  }
  case AssignDictVarAst: {
    char *name = read_str(f);
    Expr *key  = deserialize_expr(f);
    Expr *val  = deserialize_expr(f);
    Stmt *n = (Stmt *)assign_dict_expr(name ? name : "", key, val);
    free_safe(name);
    return n;
  }
  case AssignListExprAst: {
    Expr *target = deserialize_expr(f);
    Expr *idx    = deserialize_expr(f);
    Expr *val    = deserialize_expr(f);
    return (Stmt *)assign_list_expr_node(target, idx, val);
  }
  case AssignDictExprAst: {
    Expr *target = deserialize_expr(f);
    Expr *key    = deserialize_expr(f);
    Expr *val    = deserialize_expr(f);
    return (Stmt *)assign_dict_expr_node(target, key, val);
  }
  case IfAst: {
    Expr   *cond            = deserialize_expr(f);
    size_t  body_count;
    Stmt  **body            = deserialize_body(f, &body_count);
    size_t  else_body_count;
    Stmt  **else_body       = deserialize_body(f, &else_body_count);
    IfExpr *else_if         = (IfExpr *)deserialize_expr(f);
    return (Stmt *)create_if(cond, body, body_count, else_if, else_body, else_body_count);
  }
  case WhileAst: {
    Expr  *cond = deserialize_expr(f);
    size_t body_count;
    Stmt **body = deserialize_body(f, &body_count);
    return (Stmt *)create_while(cond, body, body_count);
  }
  case ForAst: {
    Expr  *init = deserialize_expr(f);
    Expr  *cond = deserialize_expr(f);
    Expr  *incr = deserialize_expr(f);
    size_t body_count;
    Stmt **body = deserialize_body(f, &body_count);
    return (Stmt *)create_for_expr(init, cond, incr, body, body_count);
  }
  case FuncDefAst: {
    char  *name       = read_str(f);
    uint32_t pc       = read_u32(f);
    char **params     = pc ? malloc_safe(sizeof(char *) * pc, "params") : NULL;
    for (uint32_t i = 0; i < pc; i++) params[i] = read_str(f);
    size_t body_count;
    Stmt **body = deserialize_body(f, &body_count);
    return (Stmt *)create_func_def(name, params, pc, body, body_count);
  }
  case CallExprAst: {
    Expr    *callee   = deserialize_expr(f);
    uint32_t ac       = read_u32(f);
    Expr   **args     = ac ? malloc_safe(sizeof(Expr *) * ac, "call args") : NULL;
    for (uint32_t i = 0; i < ac; i++) args[i] = deserialize_expr(f);
    return (Stmt *)create_call_expr(callee, args, ac);
  }
  case ListLiteralAst: {
    uint32_t ec = read_u32(f);
    Expr **elems = ec ? malloc_safe(sizeof(Expr *) * ec, "list elems") : NULL;
    for (uint32_t i = 0; i < ec; i++) elems[i] = deserialize_expr(f);
    return (Stmt *)create_list_literal(elems, ec);
  }
  case DictLiteralAst: {
    uint32_t ec   = read_u32(f);
    Expr **keys   = ec ? malloc_safe(sizeof(Expr *) * ec, "dict keys")   : NULL;
    Expr **values = ec ? malloc_safe(sizeof(Expr *) * ec, "dict values") : NULL;
    for (uint32_t i = 0; i < ec; i++) {
      keys[i]   = deserialize_expr(f);
      values[i] = deserialize_expr(f);
    }
    return (Stmt *)create_dict_literal(keys, values, ec);
  }
  case ListIndexAst: {
    Expr *list  = deserialize_expr(f);
    Expr *start = deserialize_expr(f);
    Expr *end   = deserialize_expr(f);
    int   is_sl = read_u8(f);
    return (Stmt *)create_list_index(list, start, end, is_sl);
  }
  case DictKeyAst: {
    Expr *dict = deserialize_expr(f);
    Expr *key  = deserialize_expr(f);
    return (Stmt *)create_dict_key(dict, key);
  }
  case ImportAst: {
    char *module = read_str(f);
    uint32_t ic  = read_u32(f);
    ImportStmt *imp = malloc_safe(sizeof(ImportStmt), "ImportStmt");
    imp->base.kind    = ImportAst;
    imp->module_name  = module;
    imp->import_count = ic;
    imp->imports      = ic ? malloc_safe(sizeof(ImportItem *) * ic, "imports") : NULL;
    for (uint32_t i = 0; i < ic; i++) {
      ImportItem *item = malloc_safe(sizeof(ImportItem), "ImportItem");
      item->name  = read_str(f);
      item->alias = read_str(f);
      imp->imports[i] = item;
    }
    return (Stmt *)imp;
  }
  case BreakAst:    return (Stmt *)create_break();
  case ContinueAst: return (Stmt *)create_continue();
  case ReturnAst:   return (Stmt *)create_return(deserialize_expr(f));
  case ArenaBlockAst: {
    size_t count;
    Stmt **body = deserialize_body(f, &count);
    return (Stmt *)create_arena_block(body, count);
  }
  default:          return NULL;
  }
}

#define ZOXC_MAGIC   "ZOXC"
#define ZOXC_VERSION 1

void ast_serialize(Program *program, FILE *f, uint64_t source_mtime) {
  fwrite(ZOXC_MAGIC, 1, 4, f);
  write_u8(f, ZOXC_VERSION);
  write_u64(f, source_mtime);
  serialize_node(f, (Stmt *)program);
}

/* mtime mismatch -> invalid cache */
Program *ast_deserialize(FILE *f, uint64_t source_mtime) {
  char magic[4];
  if (fread(magic, 1, 4, f) != 4) return NULL;
  if (memcmp(magic, ZOXC_MAGIC, 4) != 0) return NULL;
  uint8_t  version = read_u8(f);
  if (version != ZOXC_VERSION) return NULL;
  uint64_t cached_mtime = read_u64(f);
  if (cached_mtime != source_mtime) return NULL;
  return (Program *)deserialize_node(f);
}
