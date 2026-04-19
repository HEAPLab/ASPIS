#include <stdio.h>

__attribute__((annotate("to_harden")))
void print_string(char *s) {
  printf("The string is: %s", s);
}

int main() {
  char *s = "Hello World";
  print_string(s);
}