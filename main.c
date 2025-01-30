#include "quackcc.h"

// Reports an error and exit.
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

int main(int argc, char **argv) {
  if (argc != 2) error("%s: invalid number of arguments", argv[0]);

  // tokenise
  Token *token = tokenise(argv[1]);
  // parse
  Node *ast = parse(token);
  // generate code
  codegen(ast);

  return 0;
}
