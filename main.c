#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "%s: invalid number of arguments\n", argv[0]);
    return 1;
  }

  printf(".global _main\n");
  printf("\n");
  printf("_main:\n");
  printf("    mov x0, #%d\n", atoi(argv[1]));
  printf("    ret\n");
  return 0;
}