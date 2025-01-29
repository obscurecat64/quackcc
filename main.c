#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef enum {
  TK_NUM,
  TK_PUNC,
  TK_EOF
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind;
  Token *next;
  int val;
  char* loc;
  int len;
};

//
// Tokeniser
//

static char *current_input;

// Reports an error and exit.
static void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

static void verror_at(char *loc, char *fmt, va_list ap) {
  int pos = loc - current_input;
  fprintf(stderr, "%s\n", current_input);
  fprintf(stderr, "%*s", pos, ""); // print pos spaces.
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

static void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(loc, fmt, ap);
}

static Token *create_token(TokenKind kind, char *start, char *end) {
  Token *token = calloc(1, sizeof(Token));
  token->kind = kind;
  token->loc = start;
  token->len = end - start;
  return token;
}

static Token *get_next_token(char **pp) {
  while (isspace(**pp)) (*pp)++;

  if (**pp == '\0') return create_token(TK_EOF, *pp, *pp + 1);

  char *start = *pp;

  if (isdigit(*start)) {
    unsigned long val = strtoul(start, pp, 10);
    Token *token = create_token(TK_NUM, start, *pp);
    token->val = (int)val;
    return token;
  }

  if (ispunct(*start)) {
    return create_token(TK_PUNC, start, ++(*pp));
  }

  error_at(*pp, "invalid token!");
  return NULL;
}

static Token *tokenise(char *p) {
  Token head = {};
  Token *curr = &head;
  while (curr->kind != TK_EOF) {
    Token *next_token = get_next_token(&p);
    curr->next = next_token;
    curr = next_token;
  }
  return head.next;
}

static int get_number(Token *token) {
  if (token->kind != TK_NUM) error_at(token->loc, "expected a number");
  return token->val;
}

static bool equal(Token *token, char *s) {
  return memcmp(token->loc, s, token->len) == 0 && s[token->len] == '\0';
}

//
// Parser
//

typedef enum {
  NK_NUM,
  NK_ADD,
  NK_SUB,
  NK_MUL,
  NK_DIV
} NodeKind;

typedef struct Node Node;
struct Node {
  NodeKind kind;
  Node *lhs;
  Node *rhs;
  int val;
};

static Node* create_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Token));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node* create_num(int val) {
  Node *node = calloc(1, sizeof(Token));
  node->val = val;
  return node;
}

static void consume(Token **chain) {
  *chain = (*chain)->next;
}

static void consume_if_equal(Token **chain, char *s) {
  Token *head = *chain;
  if (!equal(head, s)) error_at(head->loc, "expected '%s'", s);
  consume(chain);
}

static Node *expr(Token **chain);
static Node *expr_prime(Token **chain, Node *lhs);
static Node *term(Token **chain);
static Node *term_prime(Token **chain, Node *lhs);
static Node *factor(Token **chain);

// Expr -> Term Expr'
static Node *expr(Token **chain) {
  Node *node_a = term(chain);
  Node *node_b = expr_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Expr' -> + Term Expr' | - Term Expr' | ε
static Node *expr_prime(Token **chain, Node* lhs) {
  Token *head = *chain;

  if (!equal(head, "+") && !equal(head, "-")) return NULL;

  NodeKind kind = equal(head, "+") ? NK_ADD : NK_SUB;
  consume(chain);

  Node *node_a = create_binary(kind, lhs, term(chain));
  Node *node_b = expr_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Term -> Factor Term'
static Node *term(Token **chain) {
  Node *node_a = factor(chain);
  Node *node_b = term_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Term' -> * Factor Term' | / Factor Term' | ε
static Node *term_prime(Token **chain, Node *lhs) {
  Token *head = *chain;

  if (!equal(head, "*") && !equal(head, "/")) return NULL;

  NodeKind kind = equal(head, "*") ? NK_MUL : NK_DIV;
  consume(chain);

  Node *node_a = create_binary(kind, lhs, factor(chain));
  Node *node_b = term_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Factor -> Number | ( Expr )
static Node *factor(Token **chain) {
  Token *head = *chain;

  if (head->kind == TK_NUM) {
    int val = head->val;
    consume(chain);
    return create_num(val);
  }

  consume_if_equal(chain, "(");
  Node *node_a = expr(chain);
  consume_if_equal(chain, ")");

  return node_a;
}

//
// Code generator
//
static void push(char* reg) {
    printf("    str %s, [sp, #-16]!\n", reg);
}

static void pop(char* reg) {
    printf("    ldr %s, [sp], #16\n", reg);
}

static void gen_expr(Node *node) {
  if (node->kind == NK_NUM) {
    printf("    mov x0, #%d\n", node->val);
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
  default:
    error("invalid expression");
  }
}

//
// main
//

int main(int argc, char **argv) {
  if (argc != 2) error("%s: invalid number of arguments\n", argv[0]);
  
  current_input = argv[1];

  // tokenise
  Token *token = tokenise(current_input);

  // parse
  Node *ast = expr(&token);

  if (token->kind != TK_EOF) error_at(token->loc, "extra token");

  // generate code
  printf(".global _main\n\n");
  printf("_main:\n");

  gen_expr(ast);

  printf("    ret\n");

  return 0;
}
