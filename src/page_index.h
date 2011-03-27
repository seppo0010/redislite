#ifndef _PAGE_INTERNAL_H
#define _PAGE_INTERNAL_H

typedef struct {
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
	redislite_page_index_key** keys;
} redislite_page_index;

redislite_page_index *redislite_page_index_create(void* db);
int redislite_page_index_add_key(redislite_page_index *page, char *key, int length);

#endif
