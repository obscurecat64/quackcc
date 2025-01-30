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

static bool startswith(char *p, char *q) {
  return strncmp(p, q, strlen(q)) == 0;
}

static int get_punct_len(char *p) {
  if (startswith(p, "==") || startswith(p, "!=") ||
      startswith(p, "<=") || startswith(p, ">="))
    return 2;

  if (ispunct(*p)) return 1;

  return 0;
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

  int punct_len = get_punct_len(start);
  if (punct_len) {
    (*pp) = (*pp) + punct_len;
    return create_token(TK_PUNC, start, *pp);
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
  NK_DIV,
  NK_NEG,
  NK_EQ,
  NK_NE,
  NK_LT,
  NK_LE,
  NK_GT,
  NK_GE,
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

static Node* create_unary(NodeKind kind, Node *lhs) {
  Node *node = calloc(1, sizeof(Token));
  node->kind = kind;
  node->lhs = lhs;
  return node;
}

static Node* create_num(int val) {
  Node *node = calloc(1, sizeof(Token));
  node->val = val;
  return node;
}

static void skip(Token **chain) {
  *chain = (*chain)->next;
}

static void consume(Token **chain, char *s) {
  Token *head = *chain;
  if (!equal(head, s)) error_at(head->loc, "expected '%s'", s);
  skip(chain);
}

static Node *expr(Token **chain);
static Node *equality(Token **chain);
static Node *equality_prime(Token **chain, Node *lhs);
static Node *relational(Token **chain);
static Node *relational_prime(Token **chain, Node *lhs);
static Node *sum(Token **chain);
static Node *sum_prime(Token **chain, Node *lhs);
static Node *term(Token **chain);
static Node *term_prime(Token **chain, Node *lhs);
static Node *unary(Token **chain);
static Node *factor(Token **chain);

// Expr -> Equality
static Node *expr(Token **chain) {
  return equality(chain);
}

// Equality -> Relational Equality'
static Node *equality(Token **chain) {
  Node *node_a = relational(chain);
  Node *node_b = equality_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Equality' -> "==" Relational Equality' | "!=" Relational Equality' | ε
static Node *equality_prime(Token **chain, Node *lhs) {
  Token *head = *chain;

  if (!equal(head, "==") && !equal(head, "!=")) return NULL;

  NodeKind kind = equal(head, "==") ? NK_EQ : NK_NE;
  skip(chain);

  Node *node_a = create_binary(kind, lhs, relational(chain));
  Node *node_b = equality_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Relational -> Sum Relational'
static Node *relational(Token **chain) {
  Node *node_a = sum(chain);
  Node *node_b = relational_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Relational' -> RELOP Sum Relational' | ε
static Node *relational_prime(Token **chain, Node *lhs) {
  Token *head = *chain;
  NodeKind kind;

  if (equal(head, "<")) kind = NK_LT;
  else if (equal(head, ">")) kind = NK_GT;
  else if (equal(head, "<=")) kind = NK_LE;
  else if (equal(head, ">=")) kind = NK_GE;
  else return NULL;

  skip(chain);

  Node *node_a = create_binary(kind, lhs, sum(chain));
  Node *node_b = relational_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Sum -> Term Sum'
static Node *sum(Token **chain) {
  Node *node_a = term(chain);
  Node *node_b = sum_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Sum' -> '+' Term Sum' | '-' Term Sum' | ε
static Node *sum_prime(Token **chain, Node* lhs) {
  Token *head = *chain;

  if (!equal(head, "+") && !equal(head, "-")) return NULL;

  NodeKind kind = equal(head, "+") ? NK_ADD : NK_SUB;
  skip(chain);

  Node *node_a = create_binary(kind, lhs, term(chain));
  Node *node_b = sum_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Term -> Unary Term'
static Node *term(Token **chain) {
  Node *node_a = unary(chain);
  Node *node_b = term_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Term' -> '*' Unary Term' | '/' Unary Term' | ε
static Node *term_prime(Token **chain, Node *lhs) {
  Token *head = *chain;

  if (!equal(head, "*") && !equal(head, "/")) return NULL;

  NodeKind kind = equal(head, "*") ? NK_MUL : NK_DIV;
  skip(chain);

  Node *node_a = create_binary(kind, lhs, unary(chain));
  Node *node_b = term_prime(chain, node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Unary -> '+' Unary | '-' Unary | Factor
static Node *unary(Token **chain) {
  Token *head = *chain;

  if (equal(head, "+")) {
    skip(chain);
    return unary(chain);
  }

  if (equal(head, "-")) {
    skip(chain);
    return create_unary(NK_NEG, unary(chain));
  }

  return factor(chain);
}

// Factor -> Number | ( Expr )
static Node *factor(Token **chain) {
  Token *head = *chain;

  if (head->kind == TK_NUM) {
    int val = head->val;
    skip(chain);
    return create_num(val);
  }

  consume(chain, "(");
  Node *node_a = expr(chain);
  consume(chain, ")");

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

  if (node->kind == NK_NEG) {
    gen_expr(node->lhs);
    printf("    neg x0, x0\n");
    return;
  }

  // binary
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
