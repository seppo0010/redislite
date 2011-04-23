#include "page_index.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "redislite.h"
#include "page.h"
#include "util.h"

void redislite_free_key(redislite_page_index_key* key) {
	if (key == NULL) return;
	redislite_free(key->keyname);
	redislite_free(key);
}

void redislite_free_index(void *db, void *_page)
{
	if (_page == NULL) return;
	redislite_page_index *page = (redislite_page_index*)_page;
	int i;
	for (i=0; i<page->number_of_keys; i++) {
		redislite_free_key(page->keys[i]);
	}
	redislite_free(page->keys);
	redislite_free(page);
}

void redislite_write_index(void *db, unsigned char *data, void *_page)
{
	redislite_page_index *page = (redislite_page_index*)_page;
	data[0] = REDISLITE_PAGE_TYPE_INDEX;
	redislite_put_4bytes(&data[1], page->free_space);
	redislite_put_2bytes(&data[5], page->number_of_keys);
	redislite_put_4bytes(&data[7], page->right_page);
	int i;
	int pos = 11;
	for (i=0; i < page->number_of_keys; i++) {
		redislite_page_index_key* key = page->keys[i];
		pos += putVarint32(&data[pos], key->keyname_size);
		memcpy(&data[pos], key->keyname, key->keyname_size);
		pos += key->keyname_size;
		redislite_put_4bytes(&data[pos], key->left_page);
		pos += 4;
	}
}

void *redislite_read_index(void *db, unsigned char *data)
{
	redislite* _db = (redislite*)db;
	redislite_page_index *page = redislite_malloc(sizeof(redislite_page_index));
	if (page == NULL) return NULL;
	page->free_space = redislite_get_4bytes(&data[1]);
	page->number_of_keys = redislite_get_2bytes(&data[5]);
	page->alloced_keys = page->number_of_keys;
	page->right_page = redislite_get_4bytes(&data[7]);
	page->keys = redislite_malloc(sizeof(redislite_page_index_key) * page->alloced_keys);
	if (page->keys == NULL) goto cleanup;
	int i, pos = 11;
	for (i=0; i<page->number_of_keys; i++) {
		page->keys[i] = redislite_malloc(sizeof(redislite_page_index_key));
		if (page->keys[i] == NULL) goto cleanup;
		pos += getVarint32(&data[pos], page->keys[i]->keyname_size);
		page->keys[i]->keyname = redislite_malloc(sizeof(char) * page->keys[i]->keyname_size);
		if (page->keys[i]->keyname == NULL) goto cleanup;
		memcpy(page->keys[i]->keyname, &data[pos], page->keys[i]->keyname_size);
		pos += page->keys[i]->keyname_size;
		page->keys[i]->left_page = redislite_get_4bytes(&data[pos]);
		pos += 4;
	}
	page->db = db;
	return page;
cleanup:
	if (page->keys) {
		for (;i>=0;i--) {
			if (page->keys[i]->keyname) redislite_free(page->keys[i]->keyname);
			if (page->keys[i]) redislite_free(page->keys[i]);
		}
		redislite_free(page->keys);
	}
	redislite_free(page);
	return NULL;
}

redislite_page_index *redislite_page_index_create(void* db)
{
	redislite* _db = (redislite*)db;
	redislite_page_index *page = redislite_malloc(sizeof(redislite_page_index));
	if (page == NULL) return NULL;
	page->free_space = _db->page_size - 11;
	page->number_of_keys = 0;
	page->right_page = 0;
	page->keys = NULL;
	page->alloced_keys = 0;
	page->db = _db;
	return page;
}

static redislite_page_index_key *redislite_index_key_for_index_name(void *_db, void *_cs, unsigned char *key, int length, int* status)
{
	changeset *cs = (changeset*)_cs;
	redislite *db = (redislite*)_db;
	int pos, found;
	int i;
	int cmp_result;
	char type;
	redislite_page_index *page = (redislite_page_index*)db->root;

	*status = REDISLITE_OK;
	while (page != NULL) {
		found = pos = 0;
		for (i=0; i < page->number_of_keys; i++) {
			cmp_result = (memcmp(page->keys[i]->keyname, key, MIN(page->keys[i]->keyname_size, length)));
			if (cmp_result == 0) {
				if (page->keys[i]->keyname_size == length) {
					redislite_page_index* new_page = redislite_page_get(db, cs, page->keys[i]->left_page, &type);
					if (new_page == NULL) { *status = REDISLITE_OOM; return NULL; }
					if (type == REDISLITE_PAGE_TYPE_INDEX) {
						found = 1;
						if (_cs == NULL && page != db->root) redislite_free_index(db, page);
						page = new_page;
						break;
					}

					redislite_page_index_key *ret = redislite_malloc(sizeof(redislite_page_index_key));
					if (ret == NULL) { *status = REDISLITE_OOM; return NULL; }
					ret->keyname_size = page->keys[i]->keyname_size;
					ret->left_page = page->keys[i]->left_page;
					ret->keyname = redislite_malloc(sizeof(char) * ret->keyname_size);
					if (ret->keyname == NULL) { redislite_free(ret); *status = REDISLITE_OOM; return NULL; }
					memcpy(ret->keyname, page->keys[i]->keyname, ret->keyname_size);

					if (_cs == NULL) {
						if (page != db->root) redislite_free_index(db, page);
						redislite_page_type *page_type = redislite_page_get_type(db, type);
						page_type->free_function(db, new_page);
					}
					return ret;
				}
				cmp_result = (page->keys[i]->keyname_size > length ? 1 : -1);
			}

			if (cmp_result < 0) {
				pos = i+1;
			} else {
				break;
			}
		}
		if (found) continue;

		int page_num;
		if (pos == page->number_of_keys) {
			page_num = page->right_page;
		} else {
			page_num = page->keys[pos]->left_page;
		}

		if (page_num == 0) {
			if (_cs == NULL && page != db->root) redislite_free_index(db, page);
			return NULL;
		}

		redislite_page_index* new_page = redislite_page_get(db, cs, page_num, &type);
		if (new_page == NULL) { *status = REDISLITE_OOM; return NULL; }
		if (cs == NULL && page != db->root) redislite_free_index(db, page);
		if (type == REDISLITE_PAGE_TYPE_INDEX) {
			page = new_page;
		} else {
			return NULL;
		}
	}
}

int redislite_value_page_for_key(void *_db, void *_cs, unsigned char *key, int length)
{
	redislite *db = (redislite*)_db;
	int status;
	redislite_page_index_key *index_key = redislite_index_key_for_index_name(db, _cs, key, length, &status);
	if (status == REDISLITE_OOM) return REDISLITE_OOM;
	int ret = 0;
	if (index_key != NULL) {
		ret = index_key->left_page;
		redislite_free_key(index_key);
	}
	return ret;
}

int redislite_delete_key(void *_cs, unsigned char *key, int length)
{
	changeset *cs = (changeset*)_cs;
	redislite *db = cs->db;
	int status;
	redislite_page_index_key *index_key = redislite_index_key_for_index_name(db, _cs, key, length, &status);
	if (status != REDISLITE_OK) return status;
	if (index_key != NULL) {
		redislite_page_delete(_cs, index_key->left_page);
		redislite_free_key(index_key);
	}
	return REDISLITE_OK;
}

int redislite_insert_key(void *_cs, unsigned char *key, int length, int left)
{
	changeset *cs = (changeset*)_cs;
	redislite *db = cs->db;
	if (db->readonly) return REDISLITE_READONLY;
	int pos;
	int i;
	int cmp_result;
	redislite_page_index *page = (redislite_page_index*)db->root;
	int page_num = 0;

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
		redislite_page_index_key *new_key = NULL;
		int result = redislite_page_index_add_key(page, pos, left, key, length);
		if (result == REDISLITE_OK) {
			result = redislite_add_modified_page(cs, page_num, page_num == 0 ? REDISLITE_PAGE_TYPE_FIRST : REDISLITE_PAGE_TYPE_INDEX, page);
			if (result < 0) return -result;
			return REDISLITE_OK;
		}
		if (result == REDISLITE_OOM || result == REDISLITE_OK) return result;

		char type;
		if (pos == page->number_of_keys) {
			page_num = page->right_page;
		} else {
			page_num = page->keys[pos]->left_page;
		}

		if (page_num == 0) {
			redislite_page_index* new_page = redislite_page_index_create(db);
			if (new_page == NULL) return REDISLITE_OOM;
			int r = redislite_page_index_add_key(new_page, 0, left, key, length);
			if (r != REDISLITE_OK) { redislite_free(new_page); return r; }
			page_num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_INDEX, new_page);
			if (pos < page->number_of_keys) {
				int r = redislite_page_index_add_key(new_page, 0, page->keys[pos]->left_page, page->keys[pos]->keyname, page->keys[pos]->keyname_size);
				if (r != REDISLITE_OK) { redislite_free(new_page); return r; }
			}

			if (pos == page->number_of_keys) {
				page->right_page = page_num;
			} else {
				page->keys[pos]->left_page = page_num;
			}
			// TODO: set page as dirty
			return REDISLITE_OK;
		}

		redislite_page_index* new_page = redislite_page_get(db, cs, page_num, &type);
		if (new_page == NULL) return REDISLITE_OOM;
		if (type == REDISLITE_PAGE_TYPE_INDEX) {
			page = new_page;
		} else {
			redislite_page_index* new_index_page = redislite_page_index_create(db);
			if (new_index_page == NULL) return REDISLITE_OOM;
			/* assert pos<size */
			int r = redislite_page_index_add_key(new_index_page, 0, page->keys[pos]->left_page, page->keys[pos]->keyname, page->keys[pos]->keyname_size);
			if (r != REDISLITE_OK) { redislite_free(new_index_page); return r; }
			page->keys[pos]->left_page = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_INDEX, new_index_page);
			page = new_index_page;
		}
	}
	return REDISLITE_ERR;
}

int redislite_page_index_add_key(redislite_page_index *page, int pos, int left, unsigned char *key, int length)
{
	redislite_page_index_key *index_key = redislite_malloc(sizeof(redislite_page_index_key));
	if (index_key == NULL) return REDISLITE_OOM;

	char length_str[9];
	int new_key_length = length + 4;
	new_key_length += putVarint32(length_str, length);
	if (page->free_space < new_key_length) {
		redislite_free(index_key);
		return REDISLITE_ERR;
	}

	if (page->alloced_keys == 0) {
		page->alloced_keys = 10;
		page->keys = redislite_malloc(sizeof(redislite_page_index_key) * page->alloced_keys);
	} else if (page->alloced_keys == page->number_of_keys) {
		redislite_page_index_key** new_keys = redislite_realloc(page->keys, sizeof(redislite_page_index_key) * page->alloced_keys * 2);
		if (new_keys == NULL) return REDISLITE_OOM;
		page->keys = new_keys;
		page->alloced_keys *= 2;
	}

	index_key->keyname_size = length;
	index_key->keyname = redislite_malloc(length * sizeof(char));
	if (index_key->keyname == NULL) { redislite_free(index_key); return REDISLITE_OOM; }
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
