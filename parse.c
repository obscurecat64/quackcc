#include "quackcc.h"

static Token **chain;

static Node *create_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *create_unary(NodeKind kind, Node *lhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  return node;
}

static Node *create_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = NK_NUM;
  node->val = val;
  return node;
}

static void skip() {
  *chain = (*chain)->next;
}

static void consume(char *s) {
  Token *head = *chain;
  if (!equal(head, s)) error_at(head->loc, "expected '%s'", s);
  skip();
}

static Node *program();
static Node *stmt();
static Node *expr_stmt();
static Node *expr();
static Node *equality();
static Node *equality_prime(Node *lhs);
static Node *relational();
static Node *relational_prime(Node *lhs);
static Node *sum();
static Node *sum_prime(Node *lhs);
static Node *term();
static Node *term_prime(Node *lhs);
static Node *unary();
static Node *factor();

// Program -> Stmt+
static Node *program() {
  Node *head = stmt();
  Node *curr = head;
  while ((*chain)->kind != TK_EOF) {
    Node *stmt_node = stmt();
    curr->next = stmt_node;
    curr = stmt_node;
  }
  curr->next = NULL;
  return head;
}

// Stmt -> ExprStmt
static Node *stmt() {
  return expr_stmt();
}

// ExprStmt -> Expr ';'
static Node *expr_stmt() {
  Node *node = create_unary(NK_EXPR_STMT, expr());
  consume(";");
  return node;
}

// Expr -> Equality
static Node *expr() {
  return equality();
}

// Equality -> Relational Equality'
static Node *equality() {
  Node *node_a = relational();
  Node *node_b = equality_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Equality' -> "==" Relational Equality' | "!=" Relational Equality' | ε
static Node *equality_prime(Node *lhs) {
  Token *head = *chain;

  if (!equal(head, "==") && !equal(head, "!=")) return NULL;

  NodeKind kind = equal(head, "==") ? NK_EQ : NK_NE;
  skip();

  Node *node_a = create_binary(kind, lhs, relational());
  Node *node_b = equality_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Relational -> Sum Relational'
static Node *relational() {
  Node *node_a = sum();
  Node *node_b = relational_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Relational' -> RELOP Sum Relational' | ε
static Node *relational_prime(Node *lhs) {
  Token *head = *chain;
  NodeKind kind;

  if (equal(head, "<")) kind = NK_LT;
  else if (equal(head, ">")) kind = NK_GT;
  else if (equal(head, "<=")) kind = NK_LE;
  else if (equal(head, ">=")) kind = NK_GE;
  else return NULL;

  skip();

  Node *node_a = create_binary(kind, lhs, sum());
  Node *node_b = relational_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Sum -> Term Sum'
static Node *sum() {
  Node *node_a = term();
  Node *node_b = sum_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Sum' -> '+' Term Sum' | '-' Term Sum' | ε
static Node *sum_prime(Node* lhs) {
  Token *head = *chain;

  if (!equal(head, "+") && !equal(head, "-")) return NULL;

  NodeKind kind = equal(head, "+") ? NK_ADD : NK_SUB;
  skip();

  Node *node_a = create_binary(kind, lhs, term());
  Node *node_b = sum_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Term -> Unary Term'
static Node *term() {
  Node *node_a = unary();
  Node *node_b = term_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Term' -> '*' Unary Term' | '/' Unary Term' | ε
static Node *term_prime(Node *lhs) {
  Token *head = *chain;

  if (!equal(head, "*") && !equal(head, "/")) return NULL;

  NodeKind kind = equal(head, "*") ? NK_MUL : NK_DIV;
  skip();

  Node *node_a = create_binary(kind, lhs, unary());
  Node *node_b = term_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Unary -> '+' Unary | '-' Unary | Factor
static Node *unary() {
  Token *head = *chain;

  if (equal(head, "+")) {
    skip();
    return unary();
  }

  if (equal(head, "-")) {
    skip();
    return create_unary(NK_NEG, unary());
  }

  return factor();
}

// Factor -> Number | ( Expr )
static Node *factor() {
  Token *head = *chain;

  if (head->kind == TK_NUM) {
    int val = head->val;
    skip();
    return create_num(val);
  }

  consume("(");
  Node *node_a = expr();
  consume(")");

  return node_a;
}

Node *parse(Token *head) {
  chain = &head;
  Node *ast = program();
  return ast;
}
