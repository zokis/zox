/* Binary/unary operations: number, string, list, dict. */
#include "eval_internal.h"

typedef enum {
  OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
  OP_GT, OP_GTE, OP_LT, OP_LTE, OP_EQ, OP_NEQ,
  OP_AND, OP_OR,
  OP_BIT_AND, OP_BIT_OR, OP_BIT_XOR, OP_SHL, OP_SHR,
  OP_UNKNOWN
} OpKind;

static OpKind get_op_kind(const char *operator) {
  OpKind kind = OP_UNKNOWN;
  if (operator == NULL || operator[0] == '\0') return OP_UNKNOWN;

  switch (operator[0]) {
    case '+': kind = (operator[1] == '\0') ? OP_ADD : OP_UNKNOWN; break;
    case '-': kind = (operator[1] == '\0') ? OP_SUB : OP_UNKNOWN; break;
    case '*':
      if (operator[1] == '\0') kind = OP_MUL;
      else if (operator[1] == '*' && operator[2] == '\0') kind = OP_POW;
      break;
    case '/': kind = (operator[1] == '\0') ? OP_DIV : OP_UNKNOWN; break;
    case '%': kind = (operator[1] == '\0') ? OP_MOD : OP_UNKNOWN; break;
    case '>':
      if (operator[1] == '\0') kind = OP_GT;
      else if (operator[1] == '=' && operator[2] == '\0') kind = OP_GTE;
      else if (operator[1] == '>' && operator[2] == '\0') kind = OP_SHR;
      break;
    case '<':
      if (operator[1] == '\0') kind = OP_LT;
      else if (operator[1] == '=' && operator[2] == '\0') kind = OP_LTE;
      else if (operator[1] == '<' && operator[2] == '\0') kind = OP_SHL;
      break;
    case '=': kind = (operator[1] == '=' && operator[2] == '\0') ? OP_EQ : OP_UNKNOWN; break;
    case '!': kind = (operator[1] == '=' && operator[2] == '\0') ? OP_NEQ : OP_UNKNOWN; break;
    case '&':
      if (operator[1] == '\0') kind = OP_BIT_AND;
      else if (operator[1] == '&' && operator[2] == '\0') kind = OP_AND;
      break;
    case '|':
      if (operator[1] == '\0') kind = OP_BIT_OR;
      else if (operator[1] == '|' && operator[2] == '\0') kind = OP_OR;
      break;
    case '^': kind = (operator[1] == '\0') ? OP_BIT_XOR : OP_UNKNOWN; break;
    default: break;
  }
  return kind;
}

NumberVal *eval_numeric_binary_expr(NumberVal *lhs, NumberVal *rhs,
                                    const char *operator) {
  double result = 0;
  unsigned short int isComparison = 0;
  OpKind kind = get_op_kind(operator);

  switch (kind) {
    case OP_ADD: result = lhs->value + rhs->value; break;
    case OP_SUB: result = lhs->value - rhs->value; break;
    case OP_MUL: result = lhs->value * rhs->value; break;
    case OP_DIV:
      if (rhs->value == 0) error("Error: Division by zero\n");
      result = lhs->value / rhs->value;
      break;
    case OP_MOD: result = (int)lhs->value % (int)rhs->value; break;
    case OP_POW: result = pow(lhs->value, rhs->value); break;
    case OP_GT:  result = lhs->value > rhs->value;  isComparison = 1; break;
    case OP_GTE: result = lhs->value >= rhs->value; isComparison = 1; break;
    case OP_LT:  result = lhs->value < rhs->value;  isComparison = 1; break;
    case OP_LTE: result = lhs->value <= rhs->value; isComparison = 1; break;
    case OP_EQ:  result = lhs->value == rhs->value; isComparison = 1; break;
    case OP_NEQ: result = lhs->value != rhs->value; isComparison = 1; break;
    case OP_AND: result = lhs->value && rhs->value; isComparison = 1; break;
    case OP_OR:  result = lhs->value || rhs->value; isComparison = 1; break;
    case OP_BIT_AND: result = (int)lhs->value & (int)rhs->value; break;
    case OP_BIT_OR:  result = (int)lhs->value | (int)rhs->value; break;
    case OP_BIT_XOR: result = (int)lhs->value ^ (int)rhs->value; break;
    case OP_SHL: result = (int)lhs->value << (int)rhs->value; break;
    case OP_SHR: result = (int)lhs->value >> (int)rhs->value; break;
    default: {
      char error_message[100];
      snprintf(error_message, sizeof(error_message), "Error: Unknown operator '%s'\n", operator);
      error(error_message);
    }
  }

  if (isComparison) return (NumberVal *)MK_BOOL(result);
  return MK_NUMBER(result);
}

static ListVal *eval_list_cross_product(ListVal *lhs, ListVal *rhs) {
  size_t result_size = lhs->size * rhs->size;
  ListVal *new_list = MK_LIST(result_size);
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
  return new_list;
}

static ListVal *eval_list_concat(ListVal *lhs, ListVal *rhs) {
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
  ListVal *new_list = MK_LIST(lhs->size + rhs->size);
  new_list->size = lhs->size + rhs->size;
  for (size_t i = 0; i < lhs->size; i++) {
    new_list->items[i] = lhs->items[i]; retain(new_list->items[i]);
  }
  for (size_t i = 0; i < rhs->size; i++) {
    new_list->items[lhs->size + i] = rhs->items[i]; retain(new_list->items[lhs->size + i]);
  }
  return new_list;
}

static ListVal *eval_list_set_op(ListVal *lhs, ListVal *rhs, const char *operator) {
  ListVal *new_list = MK_LIST(lhs->capacity + rhs->capacity);
  new_list->size = 0;
  if (!strcmp(operator, "-") || !strcmp(operator, "^")) {
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
    for (size_t i = 0; i < lhs->size; i++) {
      if (contains((RuntimeVal *)rhs, lhs->items[i]) &&
          !contains((RuntimeVal *)new_list, lhs->items[i])) {
        new_list->items[new_list->size++] = lhs->items[i]; retain(new_list->items[new_list->size - 1]);
      }
    }
  }
  return new_list;
}

ListVal *eval_list_binary_expr(ListVal *lhs, ListVal *rhs, const char *operator) {
  if (!strcmp(operator, "*")) return eval_list_cross_product(lhs, rhs);
  if (!strcmp(operator, "+")) return eval_list_concat(lhs, rhs);
  if (!strcmp(operator, "-") || !strcmp(operator, "^") ||
      !strcmp(operator, "|") || !strcmp(operator, "&")) {
    return eval_list_set_op(lhs, rhs, operator);
  }
  return NULL;
}

static const char *remove_prefix(const char *operator) {
  return (operator[0] == '&') ? operator + 1 : operator;
}

static int is_mapped_op(const char *operator) {
  if (operator[0] != '&') return 0;
  char op = operator[1];
  return (op == '+' || op == '*' || op == '/' || op == '-' || op == '%');
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
  } else if (is_mapped_op(operator)) {
    const char *op = remove_prefix(operator);
    ListVal *new_list = MK_LIST(lhs->capacity);
    for (size_t i = 0; i < lhs->size; i++) {
      new_list->items[new_list->size++] = eval_binary_expr_evaluated(lhs->items[i], rhs, op);
    }
    return (RuntimeVal *)new_list;
  }
  return NULL;
}

static void dict_merge_entries(DictVal *dest, DictVal *src) {
  for (size_t i = 0; i < src->capacity; i++) {
    if (src->entries[i].key != NULL) {
      dict_set_val(dest, src->entries[i].key, src->entries[i].value);
    }
  }
}

DictVal *eval_dict_binary_expr(DictVal *lhs, DictVal *rhs, const char *operator) {
  if (strcmp(operator, "+") != 0) return NULL;

  if (lhs->base.ref_count == 1) {
    dict_merge_entries(lhs, rhs);
    retain((RuntimeVal *)lhs);
    return lhs;
  }

  DictVal *new_dict = MK_DICT(lhs->size + rhs->size);
  dict_merge_entries(new_dict, lhs);
  dict_merge_entries(new_dict, rhs);
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

static RuntimeVal *eval_list_equality(ListVal *lhs, ListVal *rhs, const char *operator) {
  int eq = (lhs->size == rhs->size);
  for (size_t i = 0; i < lhs->size && eq; i++) {
    RuntimeVal *res = eval_binary_expr_evaluated(lhs->items[i], rhs->items[i], "==");
    eq = (res->type == BOOLEAN_T && ((BooleanVal *)res)->value);
    release(res);
  }
  return (RuntimeVal *)MK_BOOL(!strcmp(operator, "==") ? eq : !eq);
}

static RuntimeVal *eval_dict_equality(DictVal *lhs, DictVal *rhs, const char *operator) {
  if (lhs->size != rhs->size) return (RuntimeVal *)MK_BOOL(!strcmp(operator, "!="));
  int eq = 1;
  for (size_t i = 0; i < lhs->capacity && eq; i++) {
    if (lhs->entries[i].key != NULL) {
      Entry *found = dict_find_entry(rhs, lhs->entries[i].key);
      if (!found) { eq = 0; break; }
      RuntimeVal *cmp = eval_binary_expr_evaluated(lhs->entries[i].value, found->value, "==");
      eq = (cmp->type == BOOLEAN_T && ((BooleanVal *)cmp)->value);
      release(cmp);
    }
  }
  return (RuntimeVal *)MK_BOOL(!strcmp(operator, "==") ? eq : !eq);
}

static RuntimeVal *eval_struct_equality(StructVal *lhs, StructVal *rhs, const char *operator) {
  if (lhs->type_def != rhs->type_def)
    return (RuntimeVal *)MK_BOOL(!strcmp(operator, "!="));
  int eq = 1;
  for (size_t i = 0; i < lhs->type_def->field_count && eq; i++) {
    RuntimeVal *cmp = eval_binary_expr_evaluated(lhs->values[i], rhs->values[i], "==");
    eq = (cmp->type == BOOLEAN_T && ((BooleanVal *)cmp)->value);
    release(cmp);
  }
  return (RuntimeVal *)MK_BOOL(!strcmp(operator, "==") ? eq : !eq);
}

static RuntimeVal *eval_number_boolean_binary(RuntimeVal *lhs, RuntimeVal *rhs, const char *operator) {
  if (rhs->type == NUMBER_T || rhs->type == BOOLEAN_T) {
    NumberVal *lhs_num = (lhs->type == NUMBER_T) ? (NumberVal *)lhs : MK_NUMBER(((BooleanVal *)lhs)->value ? 1.0 : 0.0);
    NumberVal *rhs_num = (rhs->type == NUMBER_T) ? (NumberVal *)rhs : MK_NUMBER(((BooleanVal *)rhs)->value ? 1.0 : 0.0);
    RuntimeVal *result = (RuntimeVal *)eval_numeric_binary_expr(lhs_num, rhs_num, operator);
    if (lhs->type == BOOLEAN_T) release((RuntimeVal *)lhs_num);
    if (rhs->type == BOOLEAN_T) release((RuntimeVal *)rhs_num);
    return result;
  }
  return NULL;
}

static RuntimeVal *eval_string_binary(RuntimeVal *lhs, RuntimeVal *rhs, const char *operator) {
  if (rhs->type == STRING_T)
    return eval_string_binary_expr((StringVal *)lhs, (StringVal *)rhs, operator);
  if (rhs->type == NUMBER_T && !strcmp(operator, "*"))
    return eval_string_repeat((StringVal *)lhs, (NumberVal *)rhs);
  return NULL;
}

static RuntimeVal *eval_list_any_binary(RuntimeVal *lhs, RuntimeVal *rhs, const char *operator) {
  if (!strcmp(operator, "<<"))
    return eval_list_any_binary_expr(operator, (ListVal *)lhs, rhs);
  if (rhs->type == LIST_T) {
    if (!strcmp(operator, "==") || !strcmp(operator, "!="))
      return eval_list_equality((ListVal *)lhs, (ListVal *)rhs, operator);
    return (RuntimeVal *)eval_list_binary_expr((ListVal *)lhs, (ListVal *)rhs, operator);
  }
  return eval_list_any_binary_expr(operator, (ListVal *)lhs, rhs);
}

static RuntimeVal *eval_dict_any_binary(RuntimeVal *lhs, RuntimeVal *rhs, const char *operator) {
  if (rhs->type == DICT_T) {
    if (!strcmp(operator, "==") || !strcmp(operator, "!="))
      return eval_dict_equality((DictVal *)lhs, (DictVal *)rhs, operator);
    return (RuntimeVal *)eval_dict_binary_expr((DictVal *)lhs, (DictVal *)rhs, operator);
  }
  return NULL;
}

static RuntimeVal *eval_struct_any_binary(RuntimeVal *lhs, RuntimeVal *rhs, const char *operator) {
  if (rhs->type == STRUCT_T) {
    if (!strcmp(operator, "==") || !strcmp(operator, "!="))
      return eval_struct_equality((StructVal *)lhs, (StructVal *)rhs, operator);
  }
  return NULL;
}

/* lhs/rhs arrive ref+1; caller releases them. */
RuntimeVal *eval_binary_expr_evaluated(RuntimeVal *lhs, RuntimeVal *rhs,
                                       const char *operator) {
  RuntimeVal *res = NULL;
  switch (lhs->type) {
    case NUMBER_T:
    case BOOLEAN_T: res = eval_number_boolean_binary(lhs, rhs, operator); break;
    case STRING_T:  res = eval_string_binary(lhs, rhs, operator); break;
    case LIST_T:    res = eval_list_any_binary(lhs, rhs, operator); break;
    case DICT_T:    res = eval_dict_any_binary(lhs, rhs, operator); break;
    case STRUCT_T:  res = eval_struct_any_binary(lhs, rhs, operator); break;
    default: break;
  }
  if (res) return res;

  if (rhs->type == LIST_T && lhs->type != LIST_T && !strcmp(operator, "*"))
    return eval_list_any_binary_expr(operator, (ListVal *)rhs, lhs);

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
