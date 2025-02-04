#include "quackcc.h"

static Token **chain;
static Obj *locals;

static Node *create_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

static Node *create_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = create_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *create_unary(NodeKind kind, Node *lhs) {
  Node *node = create_node(kind);
  node->lhs = lhs;
  return node;
}

static Node *create_num(int val) {
  Node *node = create_node(NK_NUM);
  node->val = val;
  return node;
}

static Node *create_var(Obj *var) {
  Node *node = create_node(NK_VAR);
  node->var = var;
  return node;
}

static Obj *register_local(char *name) {
  Obj *obj = calloc(1, sizeof(Obj));
  obj->name = name;
  obj->next = locals;
  locals = obj;
  return obj;
}

static Obj *find_var(char *name) {
  for (Obj *var = locals; var; var = var->next) {
    if (strcmp(var->name, name) == 0) return var;
  }
  return NULL;
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
static Node *if_stmt();
static Node *compound_stmt();
static Node *null_stmt();
static Node *return_stmt();
static Node *expr_stmt();
static Node *expr();
static Node *assign();
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

static bool can_start_stmt() {
  Token *head = *chain;

  if (head->kind == TK_IDENT || head->kind == TK_NUM) return true;

  if (head->kind == TK_KEYWORD) {
    if (equal(head, "return") || equal(head, "if")) return true;
  }

  if (head->kind == TK_PUNC) {
    // can start compound and null stmt
    if (equal(head, "{") || equal(head, ";")) return true;
    // can start expr stmt
    if (equal(head, "(") || equal(head, "+") || equal(head, "-")) return true;
  }

  return false;
}

// Program -> CompoundStmt EOF
static Node *program() {
  Node *node = compound_stmt();
  if ((*chain)->kind != TK_EOF) {
    char *token_str = strndup((*chain)->loc, (*chain)->len);
    error_at((*chain)->loc, "unexpected '%s'", token_str);
  }
  return node;
}

// Stmt -> ExprStmt | CompoundStmt | NullStmt | ReturnStmt | IfStmt
static Node *stmt() {
  Token *head = *chain;

  if (head->kind == TK_KEYWORD) {
    if (equal(head, "return")) return return_stmt();
    if (equal(head, "if")) return if_stmt();
  }
  if (head->kind == TK_PUNC) {
    if (equal(head, "{")) return compound_stmt();
    if (equal(head, ";")) return null_stmt();
  }
  return expr_stmt();
}

// IfStmt -> 'if' '(' expr ')' Stmt 'else' Stmt
static Node *if_stmt() {
  Node *node = create_node(NK_IF_STMT);
  consume("if");
  consume("(");
  node->cond = expr();
  consume(")");
  node->lhs = stmt();

  Token *head = *chain;
  if (head->kind != TK_KEYWORD || !equal(head, "else")) return node;

  consume("else");
  node->rhs = stmt();
  return node;
}

// CompoundStmt -> '{' Stmt* '}'
static Node *compound_stmt() {
  consume("{");
  Node temp = {};
  Node *curr = &temp;
  while (can_start_stmt()) {
    Node *stmt_node = stmt();
    curr->next = stmt_node;
    curr = stmt_node;
  }
  Node *node = create_node(NK_COMPOUND_STMT);
  node->body = temp.next;
  consume("}");
  return node;
}

// NullStmt -> ';'
static Node *null_stmt() {
  consume(";");
  Node *node = create_node(NK_NULL_STMT);
  return node;
}

// ReturnStmt -> 'return' Expr ';'
static Node *return_stmt() {
  consume("return");
  Node *node = create_unary(NK_RETURN_STMT, expr());
  consume(";");
  return node;
}

// ExprStmt -> Expr ';'
static Node *expr_stmt() {
  Node *node = create_unary(NK_EXPR_STMT, expr());
  consume(";");
  return node;
}

// Expr -> Equality | Assign
static Node *expr() {
  Token *head = *chain;
  Token *lookahead = head->next;

  if (head->kind == TK_IDENT && lookahead->kind == TK_PUNC &&
      equal(lookahead, "=")) {
    return assign();
  }

  return equality();
}

// Assign -> Ident '=' Expr
static Node *assign() {
  Token *head = *chain;

  if (head->kind != TK_IDENT)
    error_at(head->loc, "expected identifier as left-hand side of assignment");

  char *name = strndup(head->loc, head->len);
  Obj *var = find_var(name);
  if (var == NULL) var = register_local(name);
  Node *node_a = create_var(var);
  skip();
  consume("=");
  Node *node_b = expr();

  return create_binary(NK_ASSIGN, node_a, node_b);
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

// Factor -> Number | ( Expr ) | Ident
static Node *factor() {
  Token *head = *chain;

  if (head->kind == TK_NUM) {
    int val = head->val;
    skip();
    return create_num(val);
  }

  if (head->kind == TK_IDENT) {
    char *name = strndup(head->loc, head->len);
    Obj *var = find_var(name);
    // here we have some additional logic in the parsing step
    // probably will be extracted out to the preprocessing step later on
    if (var == NULL) error_at(head->loc, "Accessing variable not yet declared");
    Node *node = create_var(var);
    skip();
    return node;
  }

  consume("(");
  Node *node = expr();
  consume(")");

  return node;
}

Fun *parse(Token *head) {
  chain = &head;
  Node *body = program();
  Fun *fun = calloc(1, sizeof(Fun));
  fun->body = body;
  fun->locals = locals;
  return fun;
}
