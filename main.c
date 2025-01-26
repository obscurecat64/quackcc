#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "%s: invalid number of arguments\n", argv[0]);
    return 1;
  }

  char *p = argv[1];

  printf(".global _main\n\n");
  printf("_main:\n");
  printf("    mov x0, #%d\n", strtol(p, &p, 10));

  while (*p) {
    if (*p == '+') {
      p++;
      printf("    add x0, x0, #%d\n", strtol(p, &p, 10));
    } else if (*p == '-') {
      p++;
      printf("    sub x0, x0, #%d\n", strtol(p, &p, 10));
    } else {
      fprintf(stderr, "unexpected character: '%c'\n", *p);
      return 1;
    }
  }

  printf("    ret\n");
  return 0;
}