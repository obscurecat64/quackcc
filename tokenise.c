#include "quackcc.h"

static char *current_input;

static void verror_at(char *loc, char *fmt, va_list ap) {
  int pos = loc - current_input;
  fprintf(stderr, "%s\n", current_input);
  fprintf(stderr, "%*s", pos, ""); // print pos spaces.
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error_at(char *loc, char *fmt, ...) {
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

static int get_ident_len(char *c) {
  char *start = c;

  if (!isalpha(*start) && *start != '_') return 0;
  c++;
  while (isalnum(*c) || *c == '_') c++;

  return c - start;
}

static int get_keyword_len(char *p) {
  char *keywords[] = {"return", "if", "else"};
  int len = sizeof(keywords) / sizeof(char*);

  for (int i = 0; i < len; i++) {
    char *keyword = keywords[i];
    if (startswith(p, keyword)) return strlen(keyword);
  }

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

  int keyword_len = get_keyword_len(start);
  if (keyword_len) {
    (*pp) = (*pp) + keyword_len;
    return create_token(TK_KEYWORD, start, *pp);
  }

  int ident_len = get_ident_len(start);
  if (ident_len) {
    (*pp) = (*pp) + ident_len;
    return create_token(TK_IDENT, start, *pp);
  }

  error_at(*pp, "invalid token!");
  return NULL;
}

Token *tokenise(char *p) {
  current_input = p;
  Token head = {};
  Token *curr = &head;
  while (curr->kind != TK_EOF) {
    Token *next_token = get_next_token(&p);
    curr->next = next_token;
    curr = next_token;
  }
  return head.next;
}

bool equal(Token *token, char *s) {
  return memcmp(token->loc, s, token->len) == 0 && s[token->len] == '\0';
}
