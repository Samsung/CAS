#include <stdio.h>

static const char* WORLD = "world";

void hello(const char *who) { printf("Hello, %s!\n", who); }

int main() {
  hello(WORLD);
  return 0;
}
