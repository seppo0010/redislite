#include "memory.h"

void *redislite_malloc(size_t size) {
	return malloc(size);
}

void *redislite_calloc(size_t num, size_t size) {
	return calloc(num, size);
}

void *redislite_realloc(void *ptr, size_t size) {
	return realloc(ptr, size);
}

void redislite_free(void *ptr) {
	free(ptr);
}
