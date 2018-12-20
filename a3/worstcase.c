#include <stdlib.h>
#include <stdio.h>

#define PAGE_SIZE 4096
#define PAGE_NUM 100000

extern int read;
int main() {
  int *allocate = malloc(PAGE_NUM * PAGE_SIZE);
  if (allocate == NULL) { exit(1); }

  for (int i = 0; i < PAGE_NUM; i++) { allocate[i*PAGE_SIZE] = i; }
  for (int j = 0; j < PAGE_NUM; j++) { read = allocate[j*PAGE_SIZE]; }
  return 0;
}
