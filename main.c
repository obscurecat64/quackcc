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
typedef struct Token {
  TokenKind kind;
  Token *next;
  int val;
  char* loc;
  int len;
} Token;

// Reports an error and exit.
static void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
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

  if (*start == '+' || *start == '-') {
    return create_token(TK_PUNC, start, ++(*pp));
  }

  error("invalid token");
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
  if (token->kind != TK_NUM) error("expected a number");
  return token->val;
}

static bool equal(Token *token, char *val) {
  return memcmp(token->loc, val, token->len) == 0 && val[token->len] == '\0';
}

int main(int argc, char **argv) {
  if (argc != 2) error("%s: invalid number of arguments\n", argv[0]);
  
  char *p = argv[1];

  // tokenise
  Token *token = tokenise(p);

  printf(".global _main\n\n");
  printf("_main:\n");

  // the pattern for now would be, number punc number punc ...
  // first token should be a num

  printf("    mov x0, #%d\n", get_number(token));
  token = token->next;

  while (token->kind != TK_EOF) {
    // get op
    if (equal(token, "+")) {
      printf("    add x0, x0, #%d\n", get_number(token->next));
      token = token->next->next;
      continue;
    }

    if (equal(token, "-")) {
      printf("    sub x0, x0, #%d\n", get_number(token->next));
      token = token->next->next;
      continue;
    }

    error("expected '+' or '-'");
  }

  printf("    ret\n");
  return 0;
}