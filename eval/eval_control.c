/* Control flow: if, while, for. */
#include "eval_internal.h"

short int is_while_finished(WhileExpr *while_expr, Environment *env) {
  RuntimeVal *condition_val = evaluate(&(while_expr->condition->stmt), env);
  if (condition_val->type != BOOLEAN_T) error("Condition of '#' must be a boolean.\n");
  short int result = ((BooleanVal *)condition_val)->value;
  release(condition_val);
  return result;
}

RuntimeVal *eval_while_expr(WhileExpr *while_expr, Environment *env) {
  Environment *while_env = create_environment(env, "while_env");
  RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();
  while (is_while_finished(while_expr, while_env)) {
    Environment *while_env_loop = create_environment(while_env, "while_env_loop");
    for (size_t i = 0; i < while_expr->body_count; i++) {
      RuntimeVal *tmp = evaluate(while_expr->body[i], while_env_loop);
      if (i < while_expr->body_count - 1) {
        release(tmp);
      } else {
        RuntimeVal *old = lastEvaluated;
        lastEvaluated = tmp;
        release(old);
      }
      if (cf_signal == CF_BREAK || cf_signal == CF_CONTINUE || cf_signal == CF_RETURN) break;
    }
    free_environment(while_env_loop);
    if (cf_signal == CF_BREAK)    { cf_signal = CF_NONE; break; }
    if (cf_signal == CF_CONTINUE) { cf_signal = CF_NONE; continue; }
    if (cf_signal == CF_RETURN)   break;
  }
  free_environment(while_env);
  return lastEvaluated;
}

RuntimeVal *eval_if_expr(IfExpr *if_expr, Environment *env) {
  Environment *if_env = create_environment(env, "if_env");
  RuntimeVal *condition_val = evaluate(&(if_expr->condition->stmt), if_env);
  if (condition_val->type != BOOLEAN_T) error("Condition of '?' must be a boolean.\n");
  short int cond = ((BooleanVal *)condition_val)->value;
  release(condition_val);

  if (cond) {
    RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();
    for (size_t i = 0; i < if_expr->body_count; i++) {
      if (i < if_expr->body_count - 1) {
        RuntimeVal *tmp = evaluate(if_expr->body[i], if_env);
        release(tmp);
      } else {
        release(lastEvaluated);
        lastEvaluated = evaluate(if_expr->body[i], if_env);
      }
    }
    free_environment(if_env);
    return lastEvaluated;
  }
  free_environment(if_env);

  if (if_expr->else_if != NULL) {
    return evaluate((Stmt *)if_expr->else_if, env);
  }
  if (if_expr->else_body != NULL) {
    Environment *else_env = create_environment(env, "else_env");
    RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();
    for (size_t i = 0; i < if_expr->else_body_count; i++) {
      if (i < if_expr->else_body_count - 1) {
        RuntimeVal *tmp = evaluate(if_expr->else_body[i], else_env);
        release(tmp);
      } else {
        release(lastEvaluated);
        lastEvaluated = evaluate(if_expr->else_body[i], else_env);
      }
    }
    free_environment(else_env);
    return lastEvaluated;
  }
  return (RuntimeVal *)MK_NIL();
}

RuntimeVal *eval_for_expr(ForExpr *for_expr, Environment *env) {
  Environment *for_env = create_environment(env, "for_env");
  RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();

  RuntimeVal *init_val = evaluate((Stmt *)for_expr->initialization, for_env);
  release(init_val);

  while (1) {
    RuntimeVal *condition_val = evaluate(&(for_expr->condition->stmt), for_env);
    if (condition_val->type != BOOLEAN_T) error("Condition of '@' must be a boolean.\n");
    short int cond = ((BooleanVal *)condition_val)->value;
    release(condition_val);
    if (!cond) break;

    Environment *for_env_loop = create_environment(for_env, "for_env_loop");
    for (size_t i = 0; i < for_expr->body_count; i++) {
      RuntimeVal *tmp = evaluate(for_expr->body[i], for_env_loop);
      if (i < for_expr->body_count - 1) {
        release(tmp);
      } else {
        RuntimeVal *old = lastEvaluated;
        lastEvaluated = tmp;
        release(old);
      }
      if (cf_signal == CF_BREAK || cf_signal == CF_CONTINUE || cf_signal == CF_RETURN) break;
    }
    free_environment(for_env_loop);

    if (cf_signal == CF_BREAK)    { cf_signal = CF_NONE; break; }
    if (cf_signal == CF_RETURN)   break;
    if (cf_signal == CF_CONTINUE) { cf_signal = CF_NONE; }

    RuntimeVal *inc_val = evaluate((Stmt *)for_expr->increment, for_env);
    release(inc_val);
  }
  free_environment(for_env);
  return lastEvaluated;
}

#include "../zox_alloc.h"

RuntimeVal *eval_arena_block(ArenaBlockExpr *arena_expr, Environment *env) {
  size_t snapshot = zox_arena_get_offset();
  Environment *arena_env = create_environment(env, "arena_env");
  RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();

  for (size_t i = 0; i < arena_expr->body_count; i++) {
    RuntimeVal *tmp = evaluate(arena_expr->body[i], arena_env);
    if (i < arena_expr->body_count - 1) {
      release(tmp);
    } else {
      RuntimeVal *old = lastEvaluated;
      lastEvaluated = tmp;
      release(old);
    }
    if (cf_signal == CF_BREAK || cf_signal == CF_CONTINUE || cf_signal == CF_RETURN) break;
  }

  /* Promotion phase: deep clone into heap (force_heap) then reset arena. */
  zox_alloc_force_heap(1);
  RuntimeVal *promoted = promote_val(lastEvaluated);
  if (promoted == lastEvaluated) retain(promoted);
  zox_alloc_force_heap(0);

  release(lastEvaluated);
  free_environment(arena_env);
  zox_arena_set_offset(snapshot);

  return promoted;
}
