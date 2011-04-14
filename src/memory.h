#include <stdio.h>
#include <stdlib.h>

void *redislite_malloc(size_t size);
void *redislite_calloc(size_t num, size_t size);
void *redislite_realloc(void *ptr, size_t size);
void redislite_free(void *ptr);
