#include "memory.h"

#ifdef OOM_SIMULATION
static int c = 0;
#endif

void *redislite_malloc(size_t size) {
#ifdef OOM_SIMULATION
	if (++c > OOM_SIMULATION)
	return NULL;
#endif
	return malloc(size);
}

void *redislite_calloc(size_t num, size_t size) {
#ifdef OOM_SIMULATION
	if (++c > OOM_SIMULATION)
	return NULL;
#endif
	return calloc(num, size);
}

void *redislite_realloc(void *ptr, size_t size) {
#ifdef OOM_SIMULATION
	if (++c > OOM_SIMULATION)
	return NULL;
#endif
	return realloc(ptr, size);
}

void redislite_free(void *ptr) {
	free(ptr);
}
