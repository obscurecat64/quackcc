#include "quackcc.h"

// input tokens are represented by a linked list.
static Token **chain;

static Obj *locals;

static char *get_ident(Token *token) {
  if (token->kind != TK_IDENT)
    error_at(token->loc, "expected an identifier");
  return strndup(token->loc, token->len);
}

static int get_number(Token *token) {
  if (token->kind != TK_NUM)
    error_at(token->loc, "expected a number");
  return token->val;
}

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
    lhs = rhs;
    rhs = temp;
  }

  // ptr + num
  int base_size = lhs->type->base->size;
  rhs = create_binary(NK_MUL, rhs, create_num(base_size, token), token);
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
    int base_size = lhs->type->base->size;
    rhs = create_binary(NK_MUL, rhs, create_num(base_size, token), token);
    return create_binary(NK_SUB, lhs, rhs, token);
  }

  // ptr - ptr
  // return how many elements are between the two
  int base_size = lhs->type->base->size;
  Node *node = create_binary(NK_SUB, lhs, rhs, token);
  node->type = type_int;
  return create_binary(NK_DIV, node, create_num(base_size, token), token);
};

static Node *create_var(Obj *var, Token *token) {
  Node *node = create_node(NK_VAR, token);
  node->var = var;
  return node;
}

static Obj *create_local(char *name, Type *type) {
  Obj *obj = calloc(1, sizeof(Obj));
  obj->name = name;
  obj->type = type;
  obj->next = locals;
  locals = obj;
  return obj;
}

static void create_param_locals(Type *param_type) {
  if (!param_type) return;
  create_param_locals(param_type->next_param_type);
  create_local(get_ident(param_type->ident), param_type);
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

static Fun *program(void);
static Fun *function_def(void);
static Node *stmt(void);
static Node *declaration(void);
static Node *declaration_prime(Type *base);
static Type *declarator(Type *type);
static Type *declarator_prefix(Type *type);
static Type *func_params(Type *type);
static Type *array_dimension(Type *type);
static Type *declarator_suffix(Type *type);
static Type *decl_spec(void);
static Node *if_stmt(void);
static Node *while_stmt(void);
static Node *for_stmt(void);
static Node *compound_stmt(void);
static Node *null_stmt(void);
static Node *return_stmt(void);
static Node *expr_stmt(void);
static Node *expr(void);
static Node *assign(void);
static Node *equality(void);
static Node *equality_prime(Node *lhs);
static Node *relational(void);
static Node *relational_prime(Node *lhs);
static Node *sum(void);
static Node *sum_prime(Node *lhs);
static Node *term(void);
static Node *term_prime(Node *lhs);
static Node *unary(void);
static Node *postfix(void);
static Node *factor(void);
static Node *args(void);

static bool can_start_stmt() {
  Token *head = *chain;

  if (head->kind == TK_IDENT || head->kind == TK_NUM) return true;

  if (head->kind == TK_KEYWORD) {
    if (equal(head, "return") || equal(head, "if") || equal(head, "for") ||
        equal(head, "while") || equal(head, "sizeof"))
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

// Program -> FunctionDefinition* EOF
static Fun *program() {
  Fun temp = {};
  Fun *curr = &temp;

  while ((*chain)->kind != TK_EOF) {
    curr->next = function_def();
    curr = curr->next;
  }

  return temp.next;
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

// Declaration -> DeclSpec (Declaration' ("," Declaration')*)? ";"
static Node *declaration() {
  Token *start_token = *chain;
  Type *base = decl_spec();

  if (equal(*chain, ";")) return create_node(NK_COMPOUND_STMT, start_token);

  Node temp = {};
  Node *curr = &temp;

  int i = 0;
  while (!equal(*chain, ";")) {
    if (i++ > 0) consume(",");
    Node *result = declaration_prime(base);
    if (result == NULL) continue;
    curr->next = create_unary(NK_EXPR_STMT, result, start_token);
    curr = curr->next;
  }

  skip();
  Node *node = create_node(NK_COMPOUND_STMT, start_token);
  node->body = temp.next;

  return node;
}

// Declaration' -> Declarator ("=" Expr)?
static Node *declaration_prime(Type *base) {
  Type *type = declarator(base);
  Obj *var = create_local(get_ident(type->ident), type);
  Node *node_a = create_var(var, type->ident);

  if (!equal(*chain, "="))
    return NULL;

  Token *equal_token = consume("=");
  Node *node_b = expr();
  return create_binary(NK_ASSIGN, node_a, node_b, equal_token);
}

// Declarator -> DeclaratorPrefix DeclaratorSuffix?
static Type *declarator(Type *type) {
  type = declarator_prefix(type);

  if (!equal(*chain, "(") && !equal(*chain, "[")) return type;
  return declarator_suffix(type);
}

// DeclaratorPrefix ->  "*"* Ident
static Type *declarator_prefix(Type *type) {
  while (equal(*chain, "*")) {
    type = create_pointer_to(type);
    skip();
  }

  Token *head = *chain;
  if (head->kind != TK_IDENT) error_at(head->loc, "expected a variable name");

  if (type == type_int) type = copy_type(type);
  type->ident = head;
  skip();

  return type;
}

// FuncParams ->
// "(" ((DeclSpec DeclaratorPrefix) ("," DeclSpec DeclaratorPrefix)* )? ")"
static Type *func_params(Type *type) {
  Token *ident = type->ident;
  type->ident = NULL;

  consume("(");

  // no parameters
  if (equal(*chain, ")")) {
    // TODO: void?
    consume(")");
    type = create_function_type(type);
    type->ident = ident;
    return type;
  }

  Type temp = {};
  Type *curr = &temp;
  int i = 0;

  while (!equal(*chain, ")")) {
    if (i++) consume(",");
    Type *type = decl_spec();
    type = declarator_prefix(type);
    curr->next_param_type = type;
    curr = curr->next_param_type;
  }

  consume(")");

  type = create_function_type(type);
  type->ident = ident;
  type->param_types = temp.next_param_type;

  return type;
}

// ArrayDimension -> ("[" num "]")*
static Type *array_dimension(Type *type) {
  Token *ident = type->ident;
  type->ident = NULL;

  int dimensions[16];
  int stack_top = -1;

  while (equal(*chain, "[")) {
    if (stack_top + 1 >= 16)
      error_at((*chain)->loc, "Too many array dimensions");
    consume("[");
    int len = get_number(*chain);
    skip();
    consume("]");
    dimensions[++stack_top] = len;
  }

  for (; stack_top >= 0; stack_top--)
    type = create_array_of(type, dimensions[stack_top]);

  type->ident = ident;
  return type;
}

// DeclaratorSuffix -> FuncParams | ArrayDimension
static Type *declarator_suffix(Type *type) {
  if (equal(*chain, "(")) return func_params(type);
  if (equal(*chain, "[")) return array_dimension(type);

  error_at((*chain)->loc, "expected '(' or '['");
  return NULL;
}

// DeclSpec -> "int"
static Type *decl_spec() {
  consume("int");
  return type_int;
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

// CompoundStmt -> '{' (Stmt | Declaration)* '}'
static Node *compound_stmt() {
  Token *lbrace_token = consume("{");
  Node temp = {};
  Node *curr = &temp;
  while (!equal(*chain, "}")) {
    if (can_start_stmt()) {
      Node *stmt_node = stmt();
      curr->next = stmt_node;
      curr = curr->next;
    } else {
      curr->next = declaration();
      curr = curr->next;
    }
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

// Unary -> '+' Unary | '-' Unary | '*' Unary | '&' Unary | Postfix
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

  return postfix();
}

// Postfix -> Factor ("[" Expr "]")*
static Node *postfix() {
  Node *arr = factor();

  Node *curr = arr;
  while (equal(*chain, "[")) {
    // x[y] is short for *(x+y)
    Token *start = *chain;
    consume("[");
    Node *index = expr();
    consume("]");

    Node *add_node = create_add(curr, index, start);
    curr = create_unary(NK_DEREF, add_node, start);
  }

  return curr;
}

// Factor -> Number | ( Expr ) | "sizeof" Unary | Ident (Args)?
static Node *factor() {
  Token *head = *chain;

  if (head->kind == TK_NUM) {
    int val = get_number(head);
    skip();
    return create_num(val, head);
  }

  if (head->kind == TK_IDENT) {
    // function call
    if (equal(head->next, "(")) {
      Node *node = create_node(NK_FUNC_CALL, head->next);
      char *func_name = strndup(head->loc, head->len);
      node->func_name = func_name;
      skip();
      node->args = args();
      return node;
    }

    // referencing a variable
    char *name = strndup(head->loc, head->len);
    Obj *var = find_var(name);
    if (var == NULL) error_at(head->loc, "undefined variable");
    Node *node = create_var(var, head);
    skip();
    return node;
  }

  if (equal(*chain, "sizeof")) {
    skip();
    Node *node = create_unary(NK_SIZEOF, unary(), head);
    return node;
  }

  consume("(");
  Node *node = expr();
  consume(")");

  return node;
}


// Args -> '(' ( Expr ( ',' Expr ) * )? ')'
static Node *args() {
  consume("(");

  // no arguments
  if (equal(*chain, ")")) {
    consume(")");
    return NULL;
  }

  Node temp = {};
  Node *curr = &temp;
  int i = 0;

  while (!equal((*chain), ")")) {
    if (i++) consume(",");
    curr->next = expr();
    curr = curr->next;
  }

  consume(")");

  return temp.next;
}

// FunctionDefinition ->
// DeclSpec DeclaratorPrefix FuncParams CompoundStatement
Fun *function_def() {
  Type *type = decl_spec();
  type = declarator_prefix(type);
  type = func_params(type);

  // reset locals
  locals = NULL;

  Fun *fun = calloc(1, sizeof(Fun));
  fun->name = get_ident(type->ident);

  create_param_locals(type->param_types);
  fun->params = locals;

  fun->body = compound_stmt();
  fun->locals = locals;

  return fun;
}

Fun *parse(Token *head) {
  chain = &head;
  return program();
}
