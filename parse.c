#include "quackcc.h"

// input tokens are represented by a linked list.
static Token **chain;

static Obj *locals;

static Node *create_node(NodeKind kind, Token *token) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->token = token;
  return node;
}

static Node *create_binary(NodeKind kind, Node *lhs, Node *rhs, Token *token) {
  Node *node = create_node(kind, token);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *create_unary(NodeKind kind, Node *lhs, Token *token) {
  Node *node = create_node(kind, token);
  node->lhs = lhs;
  return node;
}

static Node *create_num(int val, Token *token) {
  Node *node = create_node(NK_NUM, token);
  node->val = val;
  return node;
}

// In C, `+` operator is overloaded to perform the pointer arithmetic.
// If p is a pointer, p+n adds not n but sizeof(*p)*n to the value of p,
// so that p+n points to the location n elements (not bytes) ahead of p.
// In other words, we need to scale an integer value before adding to a
// pointer value. This function takes care of the scaling.
static Node *create_add(Node *lhs, Node *rhs, Token *token) {
  add_type(lhs);
  add_type(rhs);

  bool is_lhs_integer = is_integer(lhs->type);
  bool is_rhs_integer = is_integer(rhs->type);

  // ptr + ptr
  if (!is_lhs_integer && !is_rhs_integer)
    error_at(token->loc, "invalid operands");

  // num + num
  if (is_lhs_integer && is_rhs_integer)
    return create_binary(NK_ADD, lhs, rhs, token);

  // num + ptr
  // canonicalize `num + ptr` to `ptr + num`
  if (is_lhs_integer && !is_rhs_integer) {
    Node *temp = lhs;
    lhs = temp;
    rhs = lhs;
  }

  // ptr + num
  rhs = create_binary(NK_MUL, rhs, create_num(8, token), token);
  return create_binary(NK_ADD, lhs, rhs, token);
};

// Like `+`, `-` is overloaded for the pointer type.
static Node *create_sub(Node *lhs, Node *rhs, Token *token) {
  add_type(lhs);
  add_type(rhs);

  bool is_lhs_integer = is_integer(lhs->type);
  bool is_rhs_integer = is_integer(rhs->type);

  // num - ptr
  if (is_lhs_integer && !is_rhs_integer)
    error_at(token->loc, "invalid operands");

  // num - num
  if (is_lhs_integer && is_rhs_integer)
    return create_binary(NK_SUB, lhs, rhs, token);

  // ptr - num
  if (is_rhs_integer) {
    rhs = create_binary(NK_MUL, rhs, create_num(8, token), token);
    return create_binary(NK_SUB, lhs, rhs, token);
  }

  // ptr - ptr
  // return how many elements are between the two
  Node *node = create_binary(NK_SUB, lhs, rhs, token);
  node->type = type_int;
  return create_binary(NK_DIV, node, create_num(8, token), token);
};

static Node *create_var(Obj *var, Token *token) {
  Node *node = create_node(NK_VAR, token);
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

static Token *consume(char *s) {
  Token *head = *chain;
  if (!equal(head, s)) error_at(head->loc, "expected '%s'", s);
  skip();
  return head;
}

static Node *program();
static Node *stmt();
static Node *if_stmt();
static Node *while_stmt();
static Node *for_stmt();
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
    if (equal(head, "return") || equal(head, "if") || equal(head, "for") ||
        equal(head, "while"))
      return true;
  }

  if (head->kind == TK_PUNC) {
    // can start compound and null stmt
    if (equal(head, "{") || equal(head, ";")) return true;
    // can start expr stmt
    if (equal(head, "(") || equal(head, "+") || equal(head, "-") ||
        equal(head, "*") || equal(head, "&")) return true;
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

// Stmt -> ExprStmt | CompoundStmt | NullStmt | ReturnStmt | IfStmt | ForStmt
static Node *stmt() {
  Token *head = *chain;

  if (head->kind == TK_KEYWORD) {
    if (equal(head, "return")) return return_stmt();
    if (equal(head, "if")) return if_stmt();
    if (equal(head, "for")) return for_stmt();
    if (equal(head, "while")) return while_stmt();
  }
  if (head->kind == TK_PUNC) {
    if (equal(head, "{")) return compound_stmt();
    if (equal(head, ";")) return null_stmt();
  }
  return expr_stmt();
}

// IfStmt -> 'if' '(' Expr ')' Stmt ('else' Stmt)?
static Node *if_stmt() {
  Token *if_token = consume("if");
  Node *node = create_node(NK_IF_STMT, if_token);
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

// WhileStmt -> 'while' '(' Expr ')' Stmt
static Node *while_stmt() {
  Token *while_token = consume("while");
  Node *node = create_node(NK_WHILE_STMT, while_token);
  consume("(");
  node->cond = expr();
  consume(")");
  node->body = stmt();
  return node;
}

// ForStmt -> 'for' '(' Expr? ';' Expr? ';' Expr? ')' Stmt
static Node *for_stmt() {
  Node *init_node = NULL;
  Node *cond_node = NULL;
  Node *update_node = NULL;
  Token *for_token = consume("for");
  consume("(");
  if (!equal((*chain), ";")) init_node = expr();
  consume(";");
  if (!equal((*chain), ";")) cond_node = expr();
  consume(";");
  if (!equal((*chain), ")")) update_node = expr();
  consume(")");
  Node *body_node = stmt();

  Node *for_node = create_node(NK_FOR_STMT, for_token);
  for_node->lhs = init_node;
  for_node->cond = cond_node;
  for_node->rhs = update_node;
  for_node->body = body_node;

  return for_node;
}

// CompoundStmt -> '{' Stmt* '}'
static Node *compound_stmt() {
  Token *lbrace_token = consume("{");
  Node temp = {};
  Node *curr = &temp;
  while (can_start_stmt()) {
    Node *stmt_node = stmt();
    curr->next = stmt_node;
    curr = stmt_node;
    add_type(curr);
  }
  Node *node = create_node(NK_COMPOUND_STMT, lbrace_token);
  node->body = temp.next;
  consume("}");
  return node;
}

// NullStmt -> ';'
static Node *null_stmt() {
  Token *semicolon_token = consume(";");
  Node *node = create_node(NK_NULL_STMT, semicolon_token);
  return node;
}

// ReturnStmt -> 'return' Expr ';'
static Node *return_stmt() {
  Token *return_token = consume("return");
  Node *node = create_unary(NK_RETURN_STMT, expr(), return_token);
  consume(";");
  return node;
}

// ExprStmt -> Expr ';'
static Node *expr_stmt() {
  Node *node = create_unary(NK_EXPR_STMT, expr(), *chain);
  consume(";");
  return node;
}

// Expr -> Assign
static Node *expr() {
  return assign();
}

// Assign -> Equality ('=' Assign)?
static Node *assign() {
  Node *node_a = equality();
  Token *head = *chain;
  if (!equal(head, "=")) return node_a;
  Token *equal_token = consume("=");
  Node *node_b = assign();
  return create_binary(NK_ASSIGN, node_a, node_b, equal_token);
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

  Node *node_a = create_binary(kind, lhs, relational(), head);
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

  Node *node_a = create_binary(kind, lhs, sum(), head);
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

  Node *node_a = NULL;
  skip();
  if (equal(head, "+")) node_a = create_add(lhs, term(), head);
  else node_a = create_sub(lhs, term(), head);

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

  Node *node_a = create_binary(kind, lhs, unary(), head);
  Node *node_b = term_prime(node_a);

  if (node_b == NULL) return node_a;
  return node_b;
}

// Unary -> '+' Unary | '-' Unary | '*' Unary | '&' Unary | Factor
static Node *unary() {
  Token *head = *chain;

  if (equal(head, "+")) {
    skip();
    return unary();
  }

  if (equal(head, "-")) {
    skip();
    return create_unary(NK_NEG, unary(), head);
  }

  if (equal(head, "&")) {
    skip();
    return create_unary(NK_ADDR, unary(), head);
  }

  if (equal(head, "*")) {
    skip();
    return create_unary(NK_DEREF, unary(), head);
  }

  return factor();
}

// Factor -> Number | ( Expr ) | Ident
static Node *factor() {
  Token *head = *chain;

  if (head->kind == TK_NUM) {
    int val = head->val;
    skip();
    return create_num(val, head);
  }

  if (head->kind == TK_IDENT) {
    char *name = strndup(head->loc, head->len);
    Obj *var = find_var(name);
    if (var == NULL) var = register_local(name);
    Node *node = create_var(var, head);
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
