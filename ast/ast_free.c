/* Recursive AST node release. */
#include "../ast.h"

#include <stddef.h>
#include <stdlib.h>

#include "../malloc_safe.h"

extern NilLiteral     *get_preallocated_nil(void);
extern BooleanLiteral *get_preallocated_true(void);
extern BooleanLiteral *get_preallocated_false(void);
extern NumericLiteral *get_preallocated_numerics(void);

typedef void (*NodeFreeFn)(Stmt *node);

static void free_node(Stmt *node);

static void free_stmt_array(Stmt **nodes, size_t count) {
  for (size_t i = 0; i < count; i++) {
    free_node(nodes[i]);
  }
  free_safe(nodes);
}

static int is_preallocated_node(Stmt *node) {
  NilLiteral *nil_lit = get_preallocated_nil();
  BooleanLiteral *true_lit = get_preallocated_true();
  BooleanLiteral *false_lit = get_preallocated_false();
  NumericLiteral *num_lits = get_preallocated_numerics();

  return (void *)node == (void *)nil_lit ||
         (void *)node == (void *)true_lit ||
         (void *)node == (void *)false_lit ||
         ((void *)node >= (void *)&num_lits[0] &&
          (void *)node <= (void *)&num_lits[255]);
}

static void free_program_node(Stmt *node) {
  Program *p = (Program *)node;
  free_stmt_array(p->body, p->body_count);
}

static void free_string_literal_node(Stmt *node) {
  free_safe(((StringLiteral *)node)->value);
}

static void free_identifier_node(Stmt *node) {
  free_safe(((Identifier *)node)->symbol);
}

static void free_unary_expr_node(Stmt *node) {
  UnaryExpr *u = (UnaryExpr *)node;
  free_safe((void *)u->operator);
  free_node((Stmt *)u->expr);
}

static void free_binary_expr_node(Stmt *node) {
  BinaryExpr *b = (BinaryExpr *)node;
  free_node((Stmt *)b->left);
  free_node((Stmt *)b->right);
  free_safe((void *)b->operator);
}

static void free_var_declaration_node(Stmt *node) {
  VarDeclaration *v = (VarDeclaration *)node;
  free_safe(v->varname);
  free_node((Stmt *)v->value);
}

static void free_assign_var_node(Stmt *node) {
  AssignVar *a = (AssignVar *)node;
  free_safe(a->varname);
  free_node((Stmt *)a->value);
}

static void free_assign_list_var_node(Stmt *node) {
  AssignListVar *a = (AssignListVar *)node;
  free_safe(a->varname);
  free_node((Stmt *)a->value);
  free_node((Stmt *)a->index);
}

static void free_assign_dict_var_node(Stmt *node) {
  AssignDictVar *a = (AssignDictVar *)node;
  free_safe(a->varname);
  free_node((Stmt *)a->value);
  free_node((Stmt *)a->key);
}

static void free_if_expr_node(Stmt *node) {
  IfExpr *e = (IfExpr *)node;
  free_node((Stmt *)e->condition);
  free_stmt_array(e->body, e->body_count);
  free_stmt_array(e->else_body, e->else_body_count);
  if (e->else_if) {
    free_node((Stmt *)e->else_if);
  }
}

static void free_while_expr_node(Stmt *node) {
  WhileExpr *w = (WhileExpr *)node;
  free_node((Stmt *)w->condition);
  free_stmt_array(w->body, w->body_count);
}

static void free_for_expr_node(Stmt *node) {
  ForExpr *f = (ForExpr *)node;
  free_node((Stmt *)f->initialization);
  free_node((Stmt *)f->condition);
  free_node((Stmt *)f->increment);
  free_stmt_array(f->body, f->body_count);
}

static void free_func_def_node(Stmt *node) {
  FuncDef *f = (FuncDef *)node;
  free_safe(f->name);
  for (size_t i = 0; i < f->param_count; i++) {
    free_safe(f->params[i]);
  }
  free_safe(f->params);
  free_stmt_array(f->body, f->body_count);
}

static void free_call_expr_node(Stmt *node) {
  CallExpr *c = (CallExpr *)node;
  free_node((Stmt *)c->callee);
  for (size_t i = 0; i < c->arg_count; i++) {
    free_node((Stmt *)c->arguments[i]);
  }
  free_safe(c->arguments);
}

static void free_list_literal_node(Stmt *node) {
  ListLiteral *l = (ListLiteral *)node;
  for (size_t i = 0; i < l->element_count; i++) {
    free_node((Stmt *)l->elements[i]);
  }
  free_safe(l->elements);
}

static void free_dict_literal_node(Stmt *node) {
  DictLiteral *d = (DictLiteral *)node;
  for (size_t i = 0; i < d->element_count; i++) {
    free_node((Stmt *)d->keys[i]);
    free_node((Stmt *)d->values[i]);
  }
  free_safe(d->keys);
  free_safe(d->values);
}

static void free_list_index_node(Stmt *node) {
  ListIndex *l = (ListIndex *)node;
  free_node((Stmt *)l->list);
  free_node((Stmt *)l->start);
  if (l->end) {
    free_node((Stmt *)l->end);
  }
}

static void free_dict_key_node(Stmt *node) {
  DictKey *d = (DictKey *)node;
  free_node((Stmt *)d->dict);
  free_node((Stmt *)d->key);
}

static void free_assign_list_expr_node(Stmt *node) {
  AssignListExpr *a = (AssignListExpr *)node;
  free_node((Stmt *)a->target);
  free_node((Stmt *)a->index);
  free_node((Stmt *)a->value);
}

static void free_assign_dict_expr_node(Stmt *node) {
  AssignDictExpr *a = (AssignDictExpr *)node;
  free_node((Stmt *)a->target);
  free_node((Stmt *)a->key);
  free_node((Stmt *)a->value);
}

static void free_import_node(Stmt *node) {
  ImportStmt *imp = (ImportStmt *)node;
  free_safe(imp->module_name);
  for (size_t i = 0; i < imp->import_count; i++) {
    free_safe(imp->imports[i]->name);
    free_safe(imp->imports[i]->alias);
    free_safe(imp->imports[i]);
  }
  free_safe(imp->imports);
}

static void free_return_node(Stmt *node) {
  ReturnStmt *r = (ReturnStmt *)node;
  if (r->value) {
    free_node((Stmt *)r->value);
  }
}

static void free_arena_block_node(Stmt *node) {
  ArenaBlockExpr *a = (ArenaBlockExpr *)node;
  free_stmt_array(a->body, a->body_count);
}

static void free_unwrap_node(Stmt *node) {
  UnwrapExpr *u = (UnwrapExpr *)node;
  if (u->expr) {
    free_node((Stmt *)u->expr);
  }
}

static void free_match_node(Stmt *node) {
  MatchExpr *m = (MatchExpr *)node;
  if (m->target) {
    free_node((Stmt *)m->target);
  }
  for (size_t i = 0; i < m->case_count; i++) {
    if (m->cases[i]->condition) {
      free_node((Stmt *)m->cases[i]->condition);
    }
    if (m->cases[i]->branch) {
      free_node((Stmt *)m->cases[i]->branch);
    }
    free_safe(m->cases[i]);
  }
  free_safe(m->cases);
}

static void free_type_declaration_node(Stmt *node) {
  TypeDeclaration *td = (TypeDeclaration *)node;
  free_safe(td->name);
  for (size_t i = 0; i < td->field_count; i++) {
    free_safe(td->fields[i]);
  }
  free_safe(td->fields);
}

static void free_member_expr_node(Stmt *node) {
  MemberExpr *m = (MemberExpr *)node;
  if (m->object) {
    free_node((Stmt *)m->object);
  }
  free_safe(m->member);
}

static void free_assign_member_expr_node(Stmt *node) {
  AssignMemberExpr *m = (AssignMemberExpr *)node;
  if (m->object) {
    free_node((Stmt *)m->object);
  }
  free_safe(m->member);
  if (m->value) {
    free_node((Stmt *)m->value);
  }
}

static NodeFreeFn node_free_handlers[] = {
  [ProgramAst] = free_program_node,
  [StringLiteralAst] = free_string_literal_node,
  [IdentifierAst] = free_identifier_node,
  [UnaryExprAst] = free_unary_expr_node,
  [BinaryExprAst] = free_binary_expr_node,
  [VarDeclarationAst] = free_var_declaration_node,
  [AssignVarAst] = free_assign_var_node,
  [IfAst] = free_if_expr_node,
  [WhileAst] = free_while_expr_node,
  [ForAst] = free_for_expr_node,
  [FuncDefAst] = free_func_def_node,
  [CallExprAst] = free_call_expr_node,
  [ListLiteralAst] = free_list_literal_node,
  [DictLiteralAst] = free_dict_literal_node,
  [ListIndexAst] = free_list_index_node,
  [DictKeyAst] = free_dict_key_node,
  [AssignListVarAst] = free_assign_list_var_node,
  [AssignDictVarAst] = free_assign_dict_var_node,
  [ImportAst] = free_import_node,
  [AssignListExprAst] = free_assign_list_expr_node,
  [AssignDictExprAst] = free_assign_dict_expr_node,
  [ReturnAst] = free_return_node,
  [ArenaBlockAst] = free_arena_block_node,
  [ReturnSuccessAst] = free_return_node,
  [ReturnErrorAst] = free_return_node,
  [UnwrapAst] = free_unwrap_node,
  [MatchAst] = free_match_node,
  [TypeDeclarationAst] = free_type_declaration_node,
  [MemberExprAst] = free_member_expr_node,
  [AssignMemberExprAst] = free_assign_member_expr_node,
};

static void free_node(Stmt *node) {
  if (!node || is_preallocated_node(node)) {
    return;
  }

  if (node->kind >= 0 &&
      (size_t)node->kind < (sizeof(node_free_handlers) / sizeof(node_free_handlers[0])) &&
      node_free_handlers[node->kind] != NULL) {
    node_free_handlers[node->kind](node);
  }

  free_safe(node);
}

void free_expr(Expr *expr)       { free_node((Stmt *)expr); }
void free_stmt(Stmt *stmt)       { free_node(stmt); }
void free_program(Program *prog) { free_node((Stmt *)prog); }
