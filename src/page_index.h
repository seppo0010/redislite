#ifndef _PAGE_INTERNAL_H
#define _PAGE_INTERNAL_H
#include <stddef.h>

typedef struct {
	char type;
	void *page;
	int keyname_size;
	char *keyname;
	int left_page;
} redislite_page_index_key;

typedef struct {
	void *db;
	int free_space;
	int number_of_keys;
	int right_page;
	int alloced_keys;
	redislite_page_index_key **keys;
} redislite_page_index;

redislite_page_index *redislite_page_index_create(void *db);
int redislite_insert_key(void *_cs, char *key, int length, int left, char type);
int redislite_page_index_add_key(redislite_page_index *page, int pos, int left, char *key, int length, char type);
void redislite_write_index(void *_db, unsigned char *data, void *page);
void *redislite_read_index(void *db, unsigned char *data);
int redislite_page_index_type(void *_db, void *_cs, char *key, int length, char *type);
int redislite_value_page_for_key(void *_db, void *_cs, char *key, int length, char *type);
void redislite_free_index(void *db, void *_page);
int redislite_delete_keys(void *_cs, int q, char **keys, size_t *lengths);
int redislite_exists_key(void *_db, void *_cs, char *key, int length);

#endif
