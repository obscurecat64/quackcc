#include "quackcc.h"

static int depth;

static void push(char* reg) {
    printf("    str %s, [sp, #-16]!\n", reg);
    depth++;
}

static void pop(char* reg) {
    printf("    ldr %s, [sp], #16\n", reg);
    depth--;
}

static void gen_addr(Node *node) {
  if (node->kind != NK_VAR) error("not an lvalue");

  int offset = (node->name - 'a' + 1) * 16;
  printf("    sub x0, fp, #%d\n", offset);
  return;
}

static void gen_expr(Node *node) {
  if (node->kind == NK_NUM) {
    printf("    mov x0, #%d\n", node->val);
    return;
  }

  if (node->kind == NK_NEG) {
    gen_expr(node->lhs);
    printf("    neg x0, x0\n");
    return;
  }

  if (node->kind == NK_VAR) {
    gen_addr(node);
    printf("    ldr x0, [x0]\n");
    return;
  }

  if (node->kind == NK_ASSIGN) {
    gen_expr(node->rhs);
    push("x0");
    gen_addr(node->lhs);
    pop("x1");
    printf("    str x1, [x0]\n");
    return;
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
    error("invalid expression!");
  }
}

static void gen_stmt(Node *node) {
  switch (node->kind) {
    case NK_EXPR_STMT:
      gen_expr(node->lhs);
      return;
    default:
      error("invalid statement!");
  }
}

void codegen(Node *node) {
  printf(".global _main\n\n");
  printf("_main:\n");
  
  // prologue
  printf("    str fp, [sp, #-16]!\n");
  printf("    mov fp, sp\n");
  printf("    sub sp, sp, #416\n"); // 26 * 16 = 416; make room for vars a-z

  while (node != NULL) {
    gen_stmt(node);
    node = node->next;
    assert(depth == 0);
  }

  // epilogue
  printf("    add sp, fp, #16\n");
  printf("    ldr fp, [fp]\n");
  printf("    ret\n");
}
