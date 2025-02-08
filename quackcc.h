#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

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
} NodeKind;

typedef struct Obj Obj;
struct Obj {
  Obj *next;
  char *name;
  int offset;
};

typedef struct Node Node;
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
};

typedef struct Fun Fun;
struct Fun {
  Node *body;
  Obj *locals;
  int stack_size;
};

Fun *parse(Token *head);

//
// parse.c
//

void codegen(Fun *prog);
