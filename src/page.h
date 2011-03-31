#ifndef _PAGE_H
#define _PAGE_H

#define REDISLITE_PAGE_TYPE_FIRST 'F'
#define REDISLITE_PAGE_TYPE_INDEX 'I'

typedef struct {
	char identifier;
	void (*write_function)(void *_db, unsigned char *data, void *page);
	void* (*read_function)(void *_db, unsigned char *data);
} redislite_page_type;

typedef struct {
	redislite_page_type* type;
	int number;
	void *data;
} redislite_page;

void *redislite_page_get(void* db, int num, char* type);
redislite_page_type *redislite_page_get_type(char identifier);
#endif
