#ifndef _PAGE_H
#define _PAGE_H

#define REDISLITE_PAGE_TYPE_FIRST 'F'
#define REDISLITE_PAGE_TYPE_INDEX 'I'
#define REDISLITE_PAGE_TYPE_STRING 'S'
#define REDISLITE_PAGE_TYPE_STRING_OVERFLOW 'O'
#define REDISLITE_PAGE_TYPE_FREELIST 'R'
#define REDISLITE_PAGE_TYPE_LIST 'L'
#define REDISLITE_PAGE_TYPE_LIST_FIRST 'M'

typedef struct {
	char identifier;
	void (*write_function)(void *_db, unsigned char *data, void *page);
	void *(*read_function)(void *_db, unsigned char *data);
	void (*free_function)(void *_db, void *page);
	void (*delete_function)(void *_db, void *page);
} redislite_page_type;

typedef struct {
	redislite_page_type *type;
	int number;
	void *data;
} redislite_page;

void *redislite_page_get(void *_db, void *_cs, int num, char type);
int redislite_page_register_type(void *db, redislite_page_type *type);
void *redislite_page_get_by_keyname(void *_db, void *_cs, char *key_name, size_t length, char *type);
redislite_page_type *redislite_page_get_type(void *db, char identifier);
int redislite_page_delete(void *_cs, int num, char type);
#endif
