#ifndef _PAGE_FIRST_H
#define _PAGE_FIRST_H
#include "page_index.h"

typedef struct {
	int number_of_keys;
	redislite_page_index *page;
} redislite_page_index_first;

void redislite_write_first(void *_db, unsigned char *data, void *page);
void *redislite_read_first(void *_db, unsigned char *data);
void redislite_free_first(void *_db, void *page);
#endif
