#include "page_index.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "redislite.h"
#include "page.h"
#include "util.h"

void redislite_write_index(unsigned char *data, redislite_page_index *page)
{
	redislite_put_4bytes(&data[0], page->free_space);
	redislite_put_2bytes(&data[4], page->number_of_keys);
	redislite_put_4bytes(&data[6], page->right_page);
	int i;
	int pos = 10;
	for (i=0; i < page->number_of_keys; i++) {
		redislite_page_index_key* key = page->keys[i];
		pos += putVarint32(&data[pos], key->keyname_size);
		memcpy(&data[pos], key->keyname, key->keyname_size);
		pos += key->keyname_size;
		redislite_put_4bytes(&data[pos], key->left_page);
		pos += 4;
	}
}

redislite_page_index *redislite_read_index(void *db, unsigned char *data)
{
	redislite* _db = (redislite*)db;
	redislite_page_index *page = malloc(sizeof(redislite_page_index));
	if (page == NULL) return NULL;
	page->free_space = redislite_get_4bytes(&data[0]);
	page->number_of_keys = redislite_get_2bytes(&data[4]);
	page->alloced_keys = page->number_of_keys;
	page->right_page = redislite_get_4bytes(&data[6]);
	page->keys = malloc(sizeof(redislite_page_index_key) * page->alloced_keys);
	int i, pos = 10;
	for (i=0; i<page->number_of_keys; i++) {
		page->keys[i] = malloc(sizeof(redislite_page_index_key));
		pos += getVarint32(&data[pos], page->keys[i]->keyname_size);
		page->keys[i]->keyname = malloc(sizeof(char) * page->keys[i]->keyname_size);
		memcpy(&data[pos], page->keys[i]->keyname, page->keys[i]->keyname_size);
		pos += page->keys[i]->keyname_size;
		page->keys[i]->left_page = redislite_get_4bytes(&data[pos]);
		pos += 4;
	}
	page->db = db;
	return page;
}

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

int redislite_insert_key(void *_db, unsigned char *key, int length, int left)
{
	redislite *db = (redislite*)_db;
	int pos;
	int i;
	int cmp_result;
	redislite_page_index *page = (redislite_page_index*)db->root;
int t = 0;
	while (page != NULL) {
		pos = 0;
		for (i=0; i < page->number_of_keys; i++) {
			cmp_result = (memcmp(page->keys[i]->keyname, key, MIN(page->keys[i]->keyname_size, length)));
			if (cmp_result == 0) {
				cmp_result = (page->keys[i]->keyname_size > length ? 1 : -1);
				/* assert != length */
			}
			if (cmp_result < 0) {
				pos = i+1;
			} else {
				break;
			}
		}
		int result = redislite_page_index_add_key(page, pos, left, key, length);
		if (result == REDISLITE_OOM || result == REDISLITE_OK) return result;

		int page_num;
		redislite_page_type type;
		if (pos == page->number_of_keys) {
			page_num = page->right_page;
		} else {
			page_num = page->keys[pos]->left_page;
		}

		if (page_num == 0) {
			redislite_page_index* new_page = redislite_page_index_create(db);
			if (new_page == NULL) return REDISLITE_OOM;
			page_num = redislite_add_modified_page(db, -1, redislite_page_type_index, new_page);
			if (pos == page->number_of_keys) {
				page->right_page = page_num;
			} else {
				page->keys[pos]->left_page = page_num;
			}
			// TODO: set page as dirty
			left = 0; // TODO: left should point to the data page
			return redislite_page_index_add_key(new_page, 0, left, key, length);
		}

		redislite_page_index* new_page = redislite_page_get(db, page_num, &type);
		if (type == redislite_page_type_index) {
			page = new_page;
		} else {
			redislite_page_index* new_index_page = redislite_page_index_create(db);
			if (new_index_page == NULL) return REDISLITE_OOM;
			/* assert pos<size */
			int r = redislite_page_index_add_key(new_index_page, 0, page->keys[pos]->left_page, page->keys[pos]->keyname, page->keys[pos]->keyname_size);
			if (r != REDISLITE_OK) return r;
			page->keys[pos]->left_page = redislite_add_modified_page(db, -1, redislite_page_type_index, new_index_page);
			page = new_index_page;
		}
	}
	return REDISLITE_ERR;
}

int redislite_page_index_add_key(redislite_page_index *page, int pos, int left, unsigned char *key, int length)
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
