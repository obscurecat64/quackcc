#include "quackcc.h"

Type *type_int = &(Type){TYK_INT};

bool is_integer(Type *type) {
  return type->kind == TYK_INT;
}

Type *create_pointer_to(Type *base) {
  Type *type = calloc(1, sizeof(Type));
  type->kind = TYK_PTR;
  type->base = base;
  return type;
}

Type *create_function_type(Type *return_type) {
  Type *type = calloc(1, sizeof(Type));
  type->kind = TYK_FUN;
  type->return_type = return_type;
  return type;
}

void add_type(Node *node) {
  if (!node || node->type) return;
  
  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  for (Node *n = node->body; n; n = n->next) add_type(n);
  for (Node *n = node->args; n; n = n->next) add_type(n);
  
  switch (node->kind) {
  case NK_ADD:
  case NK_SUB:
  case NK_MUL:
  case NK_DIV:
  case NK_NEG:
  case NK_ASSIGN:
    node->type = node->lhs->type;
    return;
  case NK_EQ:
  case NK_NE:
  case NK_LT:
  case NK_LE:
  case NK_GT:
  case NK_GE:
  case NK_NUM:
  case NK_FUNC_CALL:
    node->type = type_int;
    return;
  case NK_VAR:
    node->type = node->var->type;
    return;
  case NK_ADDR:
    node->type = create_pointer_to(node->lhs->type);
    return;
  case NK_DEREF:
    if (node->lhs->type->kind != TYK_PTR)
      error_at(node->token->loc, "invalid pointer dereference");
    else node->type = node->lhs->type->base;
    return;
  default:
    return;
  }
}

Type *copy_type(Type *original) {
  Type *copy = calloc(1, sizeof(Type));
  *copy = *original;
  return copy;
}
