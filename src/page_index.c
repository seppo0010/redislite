#include "page_index.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "redislite.h"
#include "util.h"

redislite_page_index *redislite_page_index_create(void* db)
{
	redislite* _db = (redislite*)db;
	redislite_page_index *page = malloc(sizeof(redislite_page_index));
	if (page == NULL) return NULL;
	page->free_space = _db->page_size - 11;
	page->number_of_keys = 0;
	page->right_page = 0;
	page->keys = NULL;
	page->alloced_keys = 0;
	page->db = _db;
	return page;
}

int redislite_page_index_add_key(redislite_page_index *page, int pos, int left, char *key, int length)
{
	redislite_page_index_key *index_key = malloc(sizeof(redislite_page_index_key));
	if (index_key == NULL) return REDISLITE_OOM;

	char length_str[9];
	int new_key_length = length + 4;
	new_key_length += putVarint32(length_str, length);
	if (page->free_space < new_key_length) {
		return REDISLITE_ERR;
	}

	if (page->alloced_keys == 0) {
		page->alloced_keys = 10;
		page->keys = malloc(sizeof(redislite_page_index_key) * page->alloced_keys);
	} else if (page->alloced_keys == page->number_of_keys) {
		redislite_page_index_key** new_keys = realloc(page->keys, sizeof(redislite_page_index_key) * page->alloced_keys * 2);
		if (new_keys == NULL) return REDISLITE_OOM;
		page->keys = new_keys;
		page->alloced_keys *= 2;
	}

	index_key->keyname_size = length;
	index_key->keyname = malloc(length * sizeof(char));
	if (index_key->keyname == NULL) return REDISLITE_OOM;
	memcpy(index_key->keyname, key, length);
	index_key->left_page = left;

	if (pos == -1) pos = page->number_of_keys;
	int i;
	for (i = page->number_of_keys-1; i >= pos; --i) {
		page->keys[i+1] = page->keys[i];
	}
	page->keys[pos] = index_key;
	page->number_of_keys++;
	page->free_space -= new_key_length;

	return REDISLITE_OK;
}
