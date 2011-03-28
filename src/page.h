#ifndef _PAGE_H
#define _PAGE_H

typedef enum {
	redislite_page_type_first, /* the first is like an index, but first 100 bytes are taken */
	redislite_page_type_index,
	redislite_page_type_data,
	redislite_page_type_overflow,
	redislite_page_type_freelist
} redislite_page_type;

typedef struct {
	redislite_page_type type;
	int number;
	void *data;
} redislite_page;

void *redislite_page_get(void* db, int num, redislite_page_type* type);
#endif
