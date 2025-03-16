#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef struct Type Type;
typedef struct Node Node;

//
// main.c
//

void error(char *fmt, ...);

//
// tokenise.c
//

typedef enum {
  TK_NUM,
  TK_PUNC,
  TK_EOF,
  TK_IDENT,
  TK_KEYWORD,
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind;
  Token *next;
  int val;
  char* loc;
  int len;
};

void error_at(char *loc, char *fmt, ...);
bool equal(Token *token, char *s);
Token *tokenise(char *p);

//
// parse.c
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
  NK_EXPR_STMT,
  NK_VAR,
  NK_ASSIGN,
  NK_COMPOUND_STMT,
  NK_NULL_STMT,
  NK_RETURN_STMT,
  NK_IF_STMT,
  NK_WHILE_STMT,
  NK_FOR_STMT,
  NK_ADDR,
  NK_DEREF,
  NK_FUNC_CALL,
} NodeKind;

typedef struct Obj Obj;
struct Obj {
  Obj *next;
  Type *type;
  char *name;
  int offset;
};

struct Node {
  NodeKind kind;
  Node *lhs;
  Node *rhs;
  Node *next;
  int val;
  Obj *var;
  Node *body;
  Node *cond;
  Token *token;
  Type *type;
  char *func_name;
  Node *args;
};

typedef struct Fun Fun;
struct Fun {
  Fun *next;
  char *name;
  Node *body;
  Obj *params;
  Obj *locals;
  int stack_size;
};

Fun *parse(Token *head);

//
// type.c
//

typedef enum {
  TYK_INT,
  TYK_PTR,
  TYK_FUN,
  TYK_ARRAY,
} TypeKind;

struct Type {
  TypeKind kind;

  // sizeof() value
  int size;

  // pointer-to or array-of type
  Type *base;

  // declaration
  Token *ident;

  // array
  int array_len;

  // function type
  Type *return_type;
  Type *param_types;
  Type *next_param_type;
};

extern Type *type_int;

bool is_integer(Type *type);
void add_type(Node *node);
Type *create_pointer_to(Type *base);
Type *create_array_of(Type *base, int len);
Type *create_function_type(Type *return_type);
Type *copy_type(Type *original);

//
// codegen.c
//

void codegen(Fun *prog);
