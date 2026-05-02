/* Recursive AST node release. */
#include "../ast.h"

#include <stddef.h>
#include <stdlib.h>

#include "../malloc_safe.h"

extern NilLiteral     *get_preallocated_nil(void);
extern BooleanLiteral *get_preallocated_true(void);
extern BooleanLiteral *get_preallocated_false(void);
extern NumericLiteral *get_preallocated_numerics(void);

static void free_node(Stmt *node) {
  if (!node) return;

  NilLiteral     *nil_lit  = get_preallocated_nil();
  BooleanLiteral *true_lit = get_preallocated_true();
  BooleanLiteral *false_lit = get_preallocated_false();
  NumericLiteral *num_lits = get_preallocated_numerics();

  if ((void *)node == (void *)nil_lit   ||
      (void *)node == (void *)true_lit  ||
      (void *)node == (void *)false_lit ||
      ((void *)node >= (void *)&num_lits[0] &&
       (void *)node <= (void *)&num_lits[255])) {
    return;
  }

  switch (node->kind) {
  case ProgramAst: {
    Program *p = (Program *)node;
    for (size_t i = 0; i < p->body_count; i++) free_node(p->body[i]);
    free_safe(p->body);
    break;
  }
  case NumericLiteralAst:
    break;
  case BooleanLiteralAst:
  case NilAst:
    break;
  case StringLiteralAst: {
    StringLiteral *s = (StringLiteral *)node;
    free_safe(s->value);
    break;
  }
  case IdentifierAst: {
    Identifier *id = (Identifier *)node;
    free_safe(id->symbol);
    break;
  }
  case UnaryExprAst: {
    UnaryExpr *u = (UnaryExpr *)node;
    free_safe((void *)u->operator);
    free_node((Stmt *)u->expr);
    break;
  }
  case BinaryExprAst: {
    BinaryExpr *b = (BinaryExpr *)node;
    free_node((Stmt *)b->left);
    free_node((Stmt *)b->right);
    free_safe((void *)b->operator);
    break;
  }
  case VarDeclarationAst: {
    VarDeclaration *v = (VarDeclaration *)node;
    free_safe(v->varname);
    free_node((Stmt *)v->value);
    break;
  }
  case AssignVarAst: {
    AssignVar *a = (AssignVar *)node;
    free_safe(a->varname);
    free_node((Stmt *)a->value);
    break;
  }
  case AssignListVarAst: {
    AssignListVar *a = (AssignListVar *)node;
    free_safe(a->varname);
    free_node((Stmt *)a->value);
    free_node((Stmt *)a->index);
    break;
  }
  case AssignDictVarAst: {
    AssignDictVar *a = (AssignDictVar *)node;
    free_safe(a->varname);
    free_node((Stmt *)a->value);
    free_node((Stmt *)a->key);
    break;
  }
  case IfAst: {
    IfExpr *e = (IfExpr *)node;
    free_node((Stmt *)e->condition);
    for (size_t i = 0; i < e->body_count; i++)      free_node(e->body[i]);
    for (size_t i = 0; i < e->else_body_count; i++) free_node(e->else_body[i]);
    free_safe(e->body);
    free_safe(e->else_body);
    if (e->else_if) free_node((Stmt *)e->else_if);
    break;
  }
  case WhileAst: {
    WhileExpr *w = (WhileExpr *)node;
    free_node((Stmt *)w->condition);
    for (size_t i = 0; i < w->body_count; i++) free_node(w->body[i]);
    free_safe(w->body);
    break;
  }
  case ForAst: {
    ForExpr *f = (ForExpr *)node;
    free_node((Stmt *)f->initialization);
    free_node((Stmt *)f->condition);
    free_node((Stmt *)f->increment);
    for (size_t i = 0; i < f->body_count; i++) free_node(f->body[i]);
    free_safe(f->body);
    break;
  }
  case FuncDefAst: {
    FuncDef *f = (FuncDef *)node;
    free_safe(f->name);
    for (size_t i = 0; i < f->param_count; i++) free_safe(f->params[i]);
    free_safe(f->params);
    for (size_t i = 0; i < f->body_count; i++) free_node(f->body[i]);
    free_safe(f->body);
    break;
  }
  case CallExprAst: {
    CallExpr *c = (CallExpr *)node;
    free_node((Stmt *)c->callee);
    for (size_t i = 0; i < c->arg_count; i++) free_node((Stmt *)c->arguments[i]);
    free_safe(c->arguments);
    break;
  }
  case ListLiteralAst: {
    ListLiteral *l = (ListLiteral *)node;
    for (size_t i = 0; i < l->element_count; i++) free_node((Stmt *)l->elements[i]);
    free_safe(l->elements);
    break;
  }
  case DictLiteralAst: {
    DictLiteral *d = (DictLiteral *)node;
    for (size_t i = 0; i < d->element_count; i++) {
      free_node((Stmt *)d->keys[i]);
      free_node((Stmt *)d->values[i]);
    }
    free_safe(d->keys);
    free_safe(d->values);
    break;
  }
  case ListIndexAst: {
    ListIndex *l = (ListIndex *)node;
    free_node((Stmt *)l->list);
    free_node((Stmt *)l->start);
    if (l->end) free_node((Stmt *)l->end);
    break;
  }
  case DictKeyAst: {
    DictKey *d = (DictKey *)node;
    free_node((Stmt *)d->dict);
    free_node((Stmt *)d->key);
    break;
  }
  case AssignListExprAst: {
    AssignListExpr *a = (AssignListExpr *)node;
    free_node((Stmt *)a->target);
    free_node((Stmt *)a->index);
    free_node((Stmt *)a->value);
    break;
  }
  case AssignDictExprAst: {
    AssignDictExpr *a = (AssignDictExpr *)node;
    free_node((Stmt *)a->target);
    free_node((Stmt *)a->key);
    free_node((Stmt *)a->value);
    break;
  }
  case ImportAst: {
    ImportStmt *imp = (ImportStmt *)node;
    free_safe(imp->module_name);
    for (size_t i = 0; i < imp->import_count; i++) {
      free_safe(imp->imports[i]->name);
      free_safe(imp->imports[i]->alias);
      free_safe(imp->imports[i]);
    }
    free_safe(imp->imports);
    break;
  }
  case BreakAst:
  case ContinueAst:
    break;
  case ReturnAst: {
    ReturnStmt *r = (ReturnStmt *)node;
    if (r->value) free_node((Stmt *)r->value);
    break;
  }
  case ArenaBlockAst: {
    ArenaBlockExpr *a = (ArenaBlockExpr *)node;
    for (size_t i = 0; i < a->body_count; i++) free_node(a->body[i]);
    free_safe(a->body);
    break;
  }
  default:
    break;
  }
  free_safe(node);
}

void free_expr(Expr *expr)       { free_node((Stmt *)expr); }
void free_stmt(Stmt *stmt)       { free_node(stmt); }
void free_program(Program *prog) { free_node((Stmt *)prog); }
