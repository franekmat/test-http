#include <stdio.h>
#include <string.h>
int main() {

  char *cos = "\r\n";
  printf ("%lu vs %lu\n", strlen(cos), strlen("\r\n"));

  return 0;
}
