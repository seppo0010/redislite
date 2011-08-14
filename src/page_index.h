#ifndef _PAGE_INTERNAL_H
#define _PAGE_INTERNAL_H
#include <stddef.h>

typedef struct {
	char type;
	void *page;
	size_t keyname_size;
	char *keyname;
	int left_page;
} redislite_page_index_key;

typedef struct {
	void *db;
	size_t free_space;
	size_t number_of_keys;
	int right_page;
	size_t alloced_keys;
	redislite_page_index_key **keys;
} redislite_page_index;

redislite_page_index *redislite_page_index_create(void *db);
int redislite_insert_key(void *_cs, char *key, size_t length, int left, char type);
int redislite_page_index_add_key(void *_cs, redislite_page_index *page, int pos, int left, char *key, size_t length, char type);
void redislite_write_index(void *_db, unsigned char *data, void *page);
void *redislite_read_index(void *db, unsigned char *data);
int redislite_page_index_type(void *_db, void *_cs, char *key, size_t length, char *type);
int redislite_value_page_for_key(void *_db, void *_cs, char *key, size_t length, char *type);
void redislite_free_index(void *db, void *_page);
int redislite_delete_key(void *_cs, char *key, size_t length, int delete_data);
int redislite_delete_keys(void *_cs, int q, char **keys, size_t *lengths);
int redislite_exists_key(void *_db, void *_cs, char *key, size_t length);
int redislite_get_keys(void *_db, void *_cs, char *pattern, int pattern_len, int *number_of_keys_p, char ***keys_p, int **keys_length_p);
int redislite_page_index_rename_key(void *_cs, char *src, size_t src_len, char *target, size_t target_len);
int redislite_page_index_renamenx_key(void *_cs, char *src, size_t src_len, char *target, size_t target_len);

#endif
