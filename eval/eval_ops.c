/* Binary/unary operations: number, string, list, dict. */
#include "eval_internal.h"

NumberVal *eval_numeric_binary_expr(NumberVal *lhs, NumberVal *rhs,
                                    const char *operator) {
  double result;
  unsigned short int isComparison = 0;

  if (!strcmp(operator, "+"))       result = lhs->value + rhs->value;
  else if (!strcmp(operator, "-"))  result = lhs->value - rhs->value;
  else if (!strcmp(operator, "*"))  result = lhs->value * rhs->value;
  else if (!strcmp(operator, "/")) {
    if (rhs->value == 0) error("Error: Division by zero\n");
    result = lhs->value / rhs->value;
  }
  else if (!strcmp(operator, "%"))  result = (int)lhs->value % (int)rhs->value;
  else if (!strcmp(operator, "**")) result = pow(lhs->value, rhs->value);
  else if (!strcmp(operator, ">"))  { result = lhs->value > rhs->value;  isComparison = 1; }
  else if (!strcmp(operator, ">=")) { result = lhs->value >= rhs->value; isComparison = 1; }
  else if (!strcmp(operator, "<"))  { result = lhs->value < rhs->value;  isComparison = 1; }
  else if (!strcmp(operator, "<=")) { result = lhs->value <= rhs->value; isComparison = 1; }
  else if (!strcmp(operator, "==")) { result = lhs->value == rhs->value; isComparison = 1; }
  else if (!strcmp(operator, "!=")) { result = lhs->value != rhs->value; isComparison = 1; }
  else if (!strcmp(operator, "&&")) { result = lhs->value && rhs->value; isComparison = 1; }
  else if (!strcmp(operator, "||")) { result = lhs->value || rhs->value; isComparison = 1; }
  else if (!strcmp(operator, "&"))  result = (int)lhs->value & (int)rhs->value;
  else if (!strcmp(operator, "|"))  result = (int)lhs->value | (int)rhs->value;
  else if (!strcmp(operator, "^"))  result = (int)lhs->value ^ (int)rhs->value;
  else if (!strcmp(operator, "<<")) result = (int)lhs->value << (int)rhs->value;
  else if (!strcmp(operator, ">>")) result = (int)lhs->value >> (int)rhs->value;
  else {
    char error_message[100];
    snprintf(error_message, sizeof(error_message), "Error: Unknown operator '%s'\n", operator);
    error(error_message);
  }

  if (isComparison) return (NumberVal *)MK_BOOL(result);
  return MK_NUMBER(result);
}

ListVal *eval_list_binary_expr(ListVal *lhs, ListVal *rhs, const char *operator) {
  ListVal *new_list = NULL;
  if (!strcmp(operator, "*")) {
    size_t result_size = lhs->size * rhs->size;
    new_list = MK_LIST(result_size);
    size_t index = 0;
    for (size_t i = 0; i < lhs->size; i++) {
      for (size_t j = 0; j < rhs->size; j++) {
        ListVal *pair = MK_LIST(2);
        pair->items[0] = lhs->items[i]; retain(pair->items[0]);
        pair->items[1] = rhs->items[j]; retain(pair->items[1]);
        pair->size = 2;
        new_list->items[index] = (RuntimeVal *)pair; retain(new_list->items[index]);
        release((RuntimeVal *)pair);
        index++;
      }
    }
    new_list->size = result_size;
  } else if (!strcmp(operator, "+")) {
    if (lhs->base.ref_count == 1) {
      size_t needed = lhs->size + rhs->size;
      if (needed > lhs->capacity) {
        lhs->capacity = needed > lhs->capacity * 2 ? needed : lhs->capacity * 2;
        lhs->items = realloc_safe(lhs->items, sizeof(RuntimeVal *) * lhs->capacity,
                                  "eval_list_binary_expr in-place realloc");
      }
      for (size_t i = 0; i < rhs->size; i++) {
        lhs->items[lhs->size + i] = rhs->items[i];
        retain(lhs->items[lhs->size + i]);
      }
      lhs->size = needed;
      retain((RuntimeVal *)lhs);
      return lhs;
    }
    new_list = MK_LIST(lhs->size + rhs->size);
    new_list->size = lhs->size + rhs->size;
    for (size_t i = 0; i < lhs->size; i++) {
      new_list->items[i] = lhs->items[i]; retain(new_list->items[i]);
    }
    for (size_t i = 0; i < rhs->size; i++) {
      new_list->items[lhs->size + i] = rhs->items[i]; retain(new_list->items[lhs->size + i]);
    }
  } else if (!strcmp(operator, "-") || !strcmp(operator, "^")) {
    new_list = MK_LIST(lhs->capacity + rhs->capacity);
    new_list->size = 0;
    for (size_t i = 0; i < lhs->size; i++) {
      if (!contains((RuntimeVal *)rhs, lhs->items[i]) &&
          !contains((RuntimeVal *)new_list, lhs->items[i])) {
        new_list->items[new_list->size++] = lhs->items[i]; retain(new_list->items[new_list->size - 1]);
      }
    }
    if (!strcmp(operator, "^")) {
      for (size_t i = 0; i < rhs->size; i++) {
        if (!contains((RuntimeVal *)lhs, rhs->items[i]) &&
            !contains((RuntimeVal *)new_list, rhs->items[i])) {
          new_list->items[new_list->size++] = rhs->items[i]; retain(new_list->items[new_list->size - 1]);
        }
      }
    }
  } else if (!strcmp(operator, "|")) {
    new_list = MK_LIST(lhs->capacity + rhs->capacity);
    new_list->size = 0;
    for (size_t i = 0; i < lhs->size; i++) {
      if (!contains((RuntimeVal *)new_list, lhs->items[i])) {
        new_list->items[new_list->size++] = lhs->items[i]; retain(new_list->items[new_list->size - 1]);
      }
    }
    for (size_t i = 0; i < rhs->size; i++) {
      if (!contains((RuntimeVal *)new_list, rhs->items[i])) {
        new_list->items[new_list->size++] = rhs->items[i]; retain(new_list->items[new_list->size - 1]);
      }
    }
  } else if (!strcmp(operator, "&")) {
    new_list = MK_LIST(lhs->capacity + rhs->capacity);
    new_list->size = 0;
    for (size_t i = 0; i < lhs->size; i++) {
      if (contains((RuntimeVal *)rhs, lhs->items[i]) &&
          !contains((RuntimeVal *)new_list, lhs->items[i])) {
        new_list->items[new_list->size++] = lhs->items[i]; retain(new_list->items[new_list->size - 1]);
      }
    }
  }
  return new_list;
}

static const char *remove_prefix(const char *operator) {
  return (operator[0] == '&') ? operator + 1 : operator;
}

RuntimeVal *eval_list_any_binary_expr(const char *operator, ListVal *lhs, RuntimeVal *rhs) {
  if (!strcmp(operator, "<<")) {
    if (lhs->size >= lhs->capacity) {
      lhs->capacity = lhs->capacity * 2 + 1;
      lhs->items = realloc_safe(lhs->items, sizeof(RuntimeVal *) * lhs->capacity,
                                "eval_list_any_binary_expr realloc");
    }
    retain(rhs);
    lhs->items[lhs->size++] = rhs;
    retain((RuntimeVal *)lhs);
    return (RuntimeVal *)lhs;
  } else if (!strcmp(operator, "*") && rhs->type == NUMBER_T) {
    size_t repeat = (size_t)((NumberVal *)rhs)->value;
    ListVal *new_list = MK_LIST(lhs->size * repeat);
    for (size_t i = 0; i < lhs->size * repeat; i++) {
      new_list->items[i] = lhs->items[i % lhs->size]; retain(new_list->items[i]);
    }
    new_list->size = lhs->size * repeat;
    return (RuntimeVal *)new_list;
  } else if (operator[0] == '&' &&
             (operator[1] == '+' || operator[1] == '*' ||
              operator[1] == '/' || operator[1] == '-' ||
              operator[1] == '%')) {
    const char *op = remove_prefix(operator);
    ListVal *new_list = MK_LIST(lhs->capacity);
    for (size_t i = 0; i < lhs->size; i++) {
      new_list->items[new_list->size++] = eval_binary_expr_evaluated(lhs->items[i], rhs, op);
    }
    return (RuntimeVal *)new_list;
  }
  return NULL;
}

DictVal *eval_dict_binary_expr(DictVal *lhs, DictVal *rhs, const char *operator) {
  DictVal *new_dict = NULL;
  if (!strcmp(operator, "+")) {
    if (lhs->base.ref_count == 1) {
      for (size_t i = 0; i < rhs->capacity; i++) {
        if (rhs->entries[i].key != NULL) {
          dict_set_val(lhs, rhs->entries[i].key, rhs->entries[i].value);
        }
      }
      retain((RuntimeVal *)lhs);
      return lhs;
    }
    new_dict = MK_DICT(lhs->size + rhs->size);
    for (size_t i = 0; i < lhs->capacity; i++) {
      if (lhs->entries[i].key != NULL) {
        dict_set_val(new_dict, lhs->entries[i].key, lhs->entries[i].value);
      }
    }
    for (size_t i = 0; i < rhs->capacity; i++) {
      if (rhs->entries[i].key != NULL) {
        dict_set_val(new_dict, rhs->entries[i].key, rhs->entries[i].value);
      }
    }
  }
  return new_dict;
}

static RuntimeVal *eval_string_binary_expr(StringVal *lhs, StringVal *rhs,
                                           const char *operator) {
  if (!strcmp(operator, "+")) {
    size_t new_size = strlen(lhs->value) + strlen(rhs->value);
    char *new_value = malloc_safe(new_size + 1, "eval_string_binary_expr");
    strcpy(new_value, lhs->value);
    strcat(new_value, rhs->value);
    RuntimeVal *result = (RuntimeVal *)MK_STRING(new_value);
    free_safe(new_value);
    return result;
  } else if (!strcmp(operator, "-")) {
    int len_a = strlen(lhs->value);
    int len_b = strlen(rhs->value);
    char *result = malloc_safe(len_a + 1, "eval_string_binary_expr result");
    int result_index = 0;
    for (int i = 0; i < len_a;) {
      int j;
      for (j = 0; j < len_b && lhs->value[i + j] == rhs->value[j]; j++);
      if (j == len_b) i += len_b;
      else result[result_index++] = lhs->value[i++];
    }
    result[result_index] = '\0';
    RuntimeVal *r = (RuntimeVal *)MK_STRING(result);
    free_safe(result);
    return r;
  }
  if (!strcmp(operator, "==")) return (RuntimeVal *)MK_BOOL(!strcmp(lhs->value, rhs->value));
  if (!strcmp(operator, "!=")) return (RuntimeVal *)MK_BOOL(strcmp(lhs->value, rhs->value) != 0);
  error("Unsupported operator for string binary expression");
  return NULL;
}

RuntimeVal *eval_string_repeat(StringVal *str, NumberVal *num) {
  int repeat_count = (int)num->value;
  if (repeat_count < 0) error("Cannot repeat string a negative number of times");
  char *new_value = malloc_safe(strlen(str->value) * repeat_count + 1, "eval_string_repeat");
  new_value[0] = '\0';
  for (int i = 0; i < repeat_count; i++) strcat(new_value, str->value);
  RuntimeVal *result = (RuntimeVal *)MK_STRING(new_value);
  free_safe(new_value);
  return result;
}

Expr *runtime_value_to_expr(RuntimeVal *val) {
  if (val->type == NUMBER_T)  return (Expr *)create_numeric_literal(((NumberVal *)val)->value);
  if (val->type == BOOLEAN_T) return (Expr *)create_boolean_literal(((BooleanVal *)val)->value);
  if (val->type == STRING_T)  return (Expr *)create_string_literal(((StringVal *)val)->value);
  if (val->type == NIL_T)     return (Expr *)create_nil_literal();
  return NULL;
}

/* lhs/rhs arrive ref+1; caller releases them. */
RuntimeVal *eval_binary_expr_evaluated(RuntimeVal *lhs, RuntimeVal *rhs,
                                       const char *operator) {
  if ((lhs->type == NUMBER_T || lhs->type == BOOLEAN_T) &&
      (rhs->type == NUMBER_T || rhs->type == BOOLEAN_T)) {
    NumberVal *lhs_num = (lhs->type == NUMBER_T) ? (NumberVal *)lhs : MK_NUMBER(((BooleanVal *)lhs)->value ? 1.0 : 0.0);
    NumberVal *rhs_num = (rhs->type == NUMBER_T) ? (NumberVal *)rhs : MK_NUMBER(((BooleanVal *)rhs)->value ? 1.0 : 0.0);
    RuntimeVal *result = (RuntimeVal *)eval_numeric_binary_expr(lhs_num, rhs_num, operator);
    if (lhs->type == BOOLEAN_T) release((RuntimeVal *)lhs_num);
    if (rhs->type == BOOLEAN_T) release((RuntimeVal *)rhs_num);
    return result;
  }
  if (lhs->type == STRING_T && rhs->type == STRING_T)
    return eval_string_binary_expr((StringVal *)lhs, (StringVal *)rhs, operator);
  if (lhs->type == STRING_T && rhs->type == NUMBER_T && !strcmp(operator, "*"))
    return eval_string_repeat((StringVal *)lhs, (NumberVal *)rhs);
  if (lhs->type == LIST_T) {
    if (!strcmp(operator, "<<"))
      return eval_list_any_binary_expr(operator, (ListVal *)lhs, rhs);
    if (rhs->type == LIST_T) {
      if (!strcmp(operator, "==") || !strcmp(operator, "!=")) {
        int eq = (((ListVal *)lhs)->size == ((ListVal *)rhs)->size);
        ListVal *ll = (ListVal *)lhs, *rl = (ListVal *)rhs;
        for (size_t i = 0; i < ll->size && eq; i++) {
          RuntimeVal *res = eval_binary_expr_evaluated(ll->items[i], rl->items[i], "==");
          eq = (res->type == BOOLEAN_T && ((BooleanVal *)res)->value);
          release(res);
        }
        return (RuntimeVal *)MK_BOOL(!strcmp(operator, "==") ? eq : !eq);
      }
      return (RuntimeVal *)eval_list_binary_expr((ListVal *)lhs, (ListVal *)rhs, operator);
    }
    return eval_list_any_binary_expr(operator, (ListVal *)lhs, rhs);
  }
  if (rhs->type == LIST_T && lhs->type != LIST_T && !strcmp(operator, "*"))
    return eval_list_any_binary_expr(operator, (ListVal *)rhs, lhs);
  if (lhs->type == DICT_T && rhs->type == DICT_T) {
    if (!strcmp(operator, "==") || !strcmp(operator, "!=")) {
      DictVal *ld = (DictVal *)lhs, *rd = (DictVal *)rhs;
      unsigned short int eq = 1;
      for (size_t i = 0; i < ld->capacity && eq; i++) {
        if (ld->entries[i].key != NULL) {
          Entry *found = dict_find_entry(rd, ld->entries[i].key);
          if (!found) { eq = 0; break; }
          RuntimeVal *cmp = eval_binary_expr_evaluated(ld->entries[i].value, found->value, "==");
          eq = (cmp->type == BOOLEAN_T && ((BooleanVal *)cmp)->value);
          release(cmp);
        }
      }
      return (RuntimeVal *)MK_BOOL(!strcmp(operator, "==") ? eq : !eq);
    }
    return (RuntimeVal *)eval_dict_binary_expr((DictVal *)lhs, (DictVal *)rhs, operator);
  }
  if (lhs->type == STRUCT_T && rhs->type == STRUCT_T) {
    if (!strcmp(operator, "==") || !strcmp(operator, "!=")) {
      StructVal *sa = (StructVal *)lhs, *sb = (StructVal *)rhs;
      if (sa->type_def != sb->type_def)
        return (RuntimeVal *)MK_BOOL(!strcmp(operator, "!="));
      int eq = 1;
      for (size_t i = 0; i < sa->type_def->field_count && eq; i++) {
        RuntimeVal *cmp = eval_binary_expr_evaluated(sa->values[i], sb->values[i], "==");
        eq = (cmp->type == BOOLEAN_T && ((BooleanVal *)cmp)->value);
        release(cmp);
      }
      return (RuntimeVal *)MK_BOOL(!strcmp(operator, "==") ? eq : !eq);
    }
  }
  if (lhs->type != rhs->type) {
    if (!strcmp(operator, "==")) return (RuntimeVal *)MK_BOOL(0);
    if (!strcmp(operator, "!=")) return (RuntimeVal *)MK_BOOL(1);
    return (RuntimeVal *)MK_BOOL(0);
  }
  if (!strcmp(operator, "==")) return (RuntimeVal *)MK_BOOL(lhs == rhs);
  if (!strcmp(operator, "!=")) return (RuntimeVal *)MK_BOOL(lhs != rhs);
  char error_message[100];
  snprintf(error_message, sizeof(error_message),
           "Unsupported types (%s, %s) for operator %s\n",
           type_to_string(lhs->type), type_to_string(rhs->type), operator);
  error(error_message);
  return NULL;
}

RuntimeVal *eval_unary_expr(UnaryExpr *unary_expr, Environment *env) {
  RuntimeVal *value = evaluate(&(unary_expr->expr->stmt), env);
  if (value->type != NUMBER_T) error("Unary operator not applicable to non-number type");
  double result = ((NumberVal *)value)->value;
  if (strcmp(unary_expr->operator, "-") == 0) result = -result;
  release(value);
  return (RuntimeVal *)MK_NUMBER(result);
}
