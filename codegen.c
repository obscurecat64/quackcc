#include "quackcc.h"

static int depth;
static char *argreg[] = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};
static Fun *current_function;

static void gen_expr(Node *node);

static char *gen_simple_label_name() {
  static int i = 1;
  char *label = malloc(16);
  snprintf(label, 16, ".L%d.%s", i++, current_function->name);
  return label;
}

static char *gen_return_label_name() {
  char *label = malloc(16);
  snprintf(label, 16, ".L.return.%s", current_function->name);
  return label;
}

static void push(char* reg) {
    printf("    str %s, [sp, #-16]!\n", reg);
    depth++;
}

static void pop(char* reg) {
    printf("    ldr %s, [sp], #16\n", reg);
    depth--;
}

static void gen_addr(Node *node) {
  switch (node->kind) {
  case NK_VAR:
    printf("    add x0, fp, #%d\n", node->var->offset);
    return;
  case NK_DEREF:
    gen_expr(node->lhs);
    return;
  default:
    error_at(node->token->loc, "not an lvalue");
  }
}

static void load(Type *type) {
  if (type->kind == TYK_ARRAY) {
    // do not attempt to load a value to the register, because in general we
    // can't load an entire array to a register.
    return;
  }

  printf("    ldr x0, [x0]\n");
}

static void store(void) {
  // this is assuming that x1 is unused; we'll also store into x0
  pop("x1");
  printf("    str x1, [x0]\n");
}

static void gen_expr(Node *node) {
  switch(node->kind) {
  case NK_NUM:
    printf("    mov x0, #%d\n", node->val);
    return;
  case NK_NEG:
    gen_expr(node->lhs);
    printf("    neg x0, x0\n");
    return;
  case NK_VAR:
    gen_addr(node);
    load(node->type);
    return;
  case NK_ASSIGN:
    gen_expr(node->rhs);
    push("x0");
    gen_addr(node->lhs);
    store();
    return;
  case NK_DEREF:
    gen_expr(node->lhs);
    load(node->type);
    return;
  case NK_ADDR:
    gen_addr(node->lhs);
    return;
  case NK_FUNC_CALL: {
    int i = -1;
    // push onto stack, so that we can free up x0 for gen_expr
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen_expr(arg);
      push("x0");
      i++;
    }
    // now assign arguments into registers x0 - x7
    for (; i >= 0; i--) {
      pop(argreg[i]);
    }
    // call func
    printf("    bl _%s\n", node->func_name);
    return;
  }
  default:
    break;
  }

  // evaluate rhs, then push the value in x0 on stack
  // later on, we pop this value from the stack into x1 (since x0 is used by lhs)
  gen_expr(node->rhs);
  push("x0");
  gen_expr(node->lhs);
  pop("x1");

  switch (node->kind) {
  case NK_ADD:
    printf("    add x0, x0, x1\n");
    return;
  case NK_SUB:
    printf("    sub x0, x0, x1\n");
    return;
  case NK_MUL:
    printf("    mul x0, x0, x1\n");
    return;
  case NK_DIV:
    printf("    sdiv x0, x0, x1\n");
    return;
  case NK_EQ:
    printf("    cmp x0, x1\n");
    printf("    mov x0, #0\n");
    printf("    cset x0, eq\n");
    return;
  case NK_NE:
    printf("    cmp x0, x1\n");
    printf("    mov x0, #0\n");
    printf("    cset x0, ne\n");
    return;
  case NK_LT:
    printf("    cmp x0, x1\n");
    printf("    mov x0, #0\n");
    printf("    cset x0, lt\n");
    return;
  case NK_LE:
    printf("    cmp x0, x1\n");
    printf("    mov x0, #0\n");
    printf("    cset x0, le\n");
    return;
  case NK_GT:
    printf("    cmp x0, x1\n");
    printf("    mov x0, #0\n");
    printf("    cset x0, gt\n");
    return;
  case NK_GE:
    printf("    cmp x0, x1\n");
    printf("    mov x0, #0\n");
    printf("    cset x0, ge\n");
    return;
  default:
    error_at(node->token->loc, "invalid expression");
  }
}

static void gen_stmt(Node *node) {
  switch (node->kind) {
    case NK_EXPR_STMT:
      gen_expr(node->lhs);
      return;
    case NK_RETURN_STMT:
      gen_expr(node->lhs);
      printf("    b %s\n", gen_return_label_name());
      return;
    case NK_COMPOUND_STMT:
      for (Node *stmt = node->body; stmt; stmt = stmt->next) {
        gen_stmt(stmt);
        assert(depth == 0);
      }
      return;
    case NK_NULL_STMT:
      // do nothing
      return;
    case NK_IF_STMT: {
      if (node->rhs == NULL) {
        char *l = gen_simple_label_name();
        gen_expr(node->cond);
        printf("    cmp x0, #0\n");
        printf("    beq %s\n", l);
        gen_stmt(node->lhs);
        printf("%s:\n", l);
        return;
      }

      char *l1 = gen_simple_label_name();
      char *l2 = gen_simple_label_name();
      gen_expr(node->cond);
      printf("    cmp x0, #0\n");
      printf("    beq %s\n", l1);
      gen_stmt(node->lhs);
      printf("    b %s\n", l2);
      printf("%s:\n", l1);
      gen_stmt(node->rhs);
      printf("%s:\n", l2);
      return;
    }
    case NK_WHILE_STMT: {
      char *l1 = gen_simple_label_name();
      char *l2 = gen_simple_label_name();
      printf("%s:\n", l1);
      gen_expr(node->cond);
      printf("    cmp x0, #0\n");
      printf("    beq %s\n", l2);
      gen_stmt(node->body);
      printf("    b %s\n", l1);
      printf("%s:\n", l2);
      return;
    }
    case NK_FOR_STMT: {
      char *l1 = gen_simple_label_name();
      char *l2 = gen_simple_label_name();
      if (node->lhs != NULL) gen_expr(node->lhs);
      printf("%s:\n", l1);
      if (node->cond != NULL) {
        gen_expr(node->cond);
        printf("    cmp x0, #0\n");
        printf("    beq %s\n", l2);
      }
      gen_stmt(node->body);
      if (node->rhs != NULL) gen_expr(node->rhs);
      printf("    b %s\n", l1);
      printf("%s:\n", l2);
      return;
    }
    default:
      error_at(node->token->loc, "invalid statement");
  }
}

static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

static void assign_lvar_offsets(Fun *fun) {
  int offset = 0;
  for (Obj *var = fun->locals; var; var = var->next) {
    offset += var->type->size;
    var->offset = -offset;
  }
  fun->stack_size = align_to(offset, 16);
}

static void gen_func(Fun *fun) {
  assign_lvar_offsets(fun);

  current_function = fun;

  printf(".global _%s\n\n", fun->name);
  printf("_%s:\n", fun->name);

  // prologue
  printf("    stp fp, lr, [sp, #-16]!\n");
  printf("    mov fp, sp\n");
  printf("    sub sp, sp, #%d\n", fun->stack_size);

  // move params in registers into their allocated space in the stack
  int i = 0;
  for (Obj *var = fun->params; var; var = var->next)
    printf("    str %s, [fp, %d] \n", argreg[i++], var->offset);

  gen_stmt(fun->body);

  // epilogue
  printf("%s:\n", gen_return_label_name());
  printf("    mov sp, fp\n");
  printf("    ldp fp, lr, [sp], #16\n");
  printf("    ret\n\n");
}

void codegen(Fun *prog) {
  for (Fun *fun = prog; fun; fun = fun->next) gen_func(fun);
}
