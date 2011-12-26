#include "page_index.h"
#include "page_first.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"
#include "page.h"
#include "util.h"

void redislite_free_key(redislite_page_index_key *key)
{
	if (key == NULL) {
		return;
	}
	redislite_free(key->keyname);
	redislite_free(key);
}

void redislite_free_index(void *db, void *_page)
{
	db = db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	if (_page == NULL) {
		return;
	}
	redislite_page_index *page = (redislite_page_index *)_page;
	size_t i;
	for (i = 0; i < page->number_of_keys; i++) {
		redislite_free_key(page->keys[i]);
	}
	redislite_free(page->keys);
	redislite_free(page);
}

void redislite_write_index(void *db, unsigned char *data, void *_page)
{
	db = db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_page_index *page = (redislite_page_index *)_page;
	redislite_put_4bytes(&data[0], 0); // reserved
	redislite_put_4bytes(&data[4], page->free_space);
	redislite_put_2bytes(&data[8], page->number_of_keys);
	redislite_put_4bytes(&data[10], page->right_page);
	size_t i;
	size_t pos = 14;
	for (i = 0; i < page->number_of_keys; i++) {
		redislite_page_index_key *key = page->keys[i];
		pos += putVarint32(&data[pos], key->keyname_size);
		data[pos] = key->type;
		pos += 1;
		memcpy(&data[pos], key->keyname, key->keyname_size);
		pos += key->keyname_size;
		redislite_put_4bytes(&data[pos], key->left_page);
		pos += 4;
	}
}

void *redislite_read_index(void *db, unsigned char *data)
{
	redislite_page_index *page = redislite_malloc(sizeof(redislite_page_index));
	if (page == NULL) {
		return NULL;
	}
	page->free_space = redislite_get_4bytes(&data[4]);
	page->number_of_keys = redislite_get_2bytes(&data[8]);
	page->alloced_keys = page->number_of_keys;
	page->right_page = redislite_get_4bytes(&data[10]);
	page->db = db;
	if (page->alloced_keys == 0) {
		page->keys = NULL;
		return page;
	}

	page->keys = redislite_malloc(sizeof(redislite_page_index_key) * page->alloced_keys);
	if (page->keys == NULL) {
		goto cleanup;
	}
	size_t i, pos = 14;
	for (i = 0; i < page->number_of_keys; i++) {
		page->keys[i] = redislite_malloc(sizeof(redislite_page_index_key));
		if (page->keys[i] == NULL) {
			goto cleanup;
		}
		pos += getVarint32(&data[pos], page->keys[i]->keyname_size);
		page->keys[i]->type = data[pos];
		pos += 1;
		page->keys[i]->keyname = redislite_malloc(sizeof(char) * page->keys[i]->keyname_size);
		if (page->keys[i]->keyname == NULL) {
			goto cleanup;
		}
		memcpy(page->keys[i]->keyname, &data[pos], page->keys[i]->keyname_size);
		pos += page->keys[i]->keyname_size;
		page->keys[i]->left_page = redislite_get_4bytes(&data[pos]);
		pos += 4;
	}
	return page;
cleanup:
	if (page->keys) {
		for (;; i--) {
			if (page->keys[i] && page->keys[i]->keyname) {
				redislite_free(page->keys[i]->keyname);
			}
			if (page->keys[i]) {
				redislite_free(page->keys[i]);
			}
			if (i == 0) {
				break;
			}
		}
		redislite_free(page->keys);
	}
	redislite_free(page);
	return NULL;
}

redislite_page_index *redislite_page_index_create(void *db)
{
	redislite *_db = (redislite *)db;
	redislite_page_index *page = redislite_malloc(sizeof(redislite_page_index));
	if (page == NULL) {
		return NULL;
	}
	page->free_space = _db->page_size - 14;
	page->number_of_keys = 0;
	page->right_page = 0;
	page->keys = NULL;
	page->alloced_keys = 0;
	page->db = _db;
	return page;
}

static int redislite_keys_are_equal(redislite_page_index_key *key1, redislite_page_index_key *key2)
{
	if (key1->keyname_size != key2->keyname_size) {
		return 0;
	}
	return 0 ==  memcmp(key1->keyname, key2->keyname, MIN(key1->keyname_size, key2->keyname_size));
}

/*
static int redislite_keys_cmp(redislite_page_index_key* key1, redislite_page_index_key* key2) {
	int cmp_result = memcmp(key1->keyname, key2->keyname, MIN(key1->keyname_size, key2->keyname_size));
	if (cmp_result == 0) {
		if (key1->keyname_size == key2->keyname_size)
			cmp_result = 0;
		else
			cmp_result = (key1->keyname_size > key2->keyname_size ? 1 : -1);
	}
	return cmp_result;
}
*/

static int redislite_remove_key(void *_cs, void *_key, int page_num)
{
	redislite_page_index_key *key = (redislite_page_index_key *)_key;
	redislite_page_index *page = (redislite_page_index *)key->page;
	if (page == NULL) {
		return REDISLITE_NOT_FOUND;
	}
	size_t i, j;
	// TODO: binary search
	for (i = 0; i < page->number_of_keys; i++) {
		if (redislite_keys_are_equal(page->keys[i], key)) {
			redislite_free_key(page->keys[i]);
			for (j = 1 + i; j < page->number_of_keys; j++) {
				page->keys[j - 1] = page->keys[j];
			}
			page->number_of_keys--;
			if (key != page->keys[i]) {
				redislite_free_key(key);
			}
			if (page_num > 0) {
				int status = redislite_add_modified_page(_cs, page_num, REDISLITE_PAGE_TYPE_INDEX, page);
				if (status < 0) {
					return status;
				}
			}
			return REDISLITE_OK;
		}
	}
	return REDISLITE_NOT_FOUND;
}

static redislite_page_index_key *redislite_index_key_for_index_name(void *_db, void *_cs, void *first_page, char *key, size_t length, int *status, int *page_num)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = (redislite *)_db;
	int found;
	size_t pos, i;
	int cmp_result;
	redislite_page_index *page = ((redislite_page_index_first *)first_page)->page;
	if (page == NULL) {
		page = ((redislite_page_index_first *)db->root)->page;
	}

	int _page_num = 0;

	*status = REDISLITE_OK;

	while (page != NULL) {
		found = pos = 0;
		for (i = 0; i < page->number_of_keys; i++) {
			cmp_result = (memcmp(page->keys[i]->keyname, key, MIN(page->keys[i]->keyname_size, length)));
			if (cmp_result == 0) {
				if (page->keys[i]->keyname_size == length) {
					redislite_page_index *new_page = redislite_page_get(db, cs, page->keys[i]->left_page, page->keys[i]->type);
					if (new_page == NULL) {
						*status = REDISLITE_OOM;
						return NULL;
					}
					if (page->keys[i]->type == REDISLITE_PAGE_TYPE_INDEX) {
						found = 1;
						_page_num = page->keys[i]->left_page;
						if (_cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
							redislite_free_index(db, page);
						}
						page = new_page;
						break;
					}

					redislite_page_index_key *ret = redislite_malloc(sizeof(redislite_page_index_key));
					if (ret == NULL) {
						*status = REDISLITE_OOM;
						return NULL;
					}
					ret->type = page->keys[i]->type;
					ret->page = page;
					ret->keyname_size = page->keys[i]->keyname_size;
					ret->left_page = page->keys[i]->left_page;
					ret->keyname = redislite_malloc(sizeof(char) * ret->keyname_size);
					if (ret->keyname == NULL) {
						redislite_free(ret);
						*status = REDISLITE_OOM;
						return NULL;
					}
					memcpy(ret->keyname, page->keys[i]->keyname, ret->keyname_size);

					if (_cs == NULL) {
						redislite_page_type *page_type = redislite_page_get_type(db, page->keys[i]->type);
						if (page_type->free_function) {
							page_type->free_function(db, new_page);
						}
						if (page != ((redislite_page_index_first *)db->root)->page) {
							redislite_free_index(db, page);
						}
					}
					if (page_num) {
						*page_num = _page_num;
					}
					return ret;
				}
				cmp_result = (page->keys[i]->keyname_size > length ? 1 : -1);
			}

			if (cmp_result < 0) {
				pos = i + 1;
			}
			else {
				break;
			}
		}
		if (found) {
			continue;
		}

		char type;
		if (pos == page->number_of_keys) {
			_page_num = page->right_page;
			type = REDISLITE_PAGE_TYPE_INDEX;
		}
		else {
			_page_num = page->keys[pos]->left_page;
			type = page->keys[pos]->type;
		}

		if (_page_num == 0) {
			if (_cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
				redislite_free_index(db, page);
			}
			*status = REDISLITE_NOT_FOUND;
			return NULL;
		}

		void *new_page = redislite_page_get(db, cs, _page_num, type);
		if (new_page == NULL) {
			*status = REDISLITE_OOM;
			return NULL;
		}
		if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
			redislite_free_index(db, page);
		}
		if (type == REDISLITE_PAGE_TYPE_INDEX) {
			page = (redislite_page_index *)new_page;
		}
		else {
			if (cs == NULL) {
				redislite_page_type *page_type = redislite_page_get_type(db, type);
				if (page_type->free_function) {
					page_type->free_function(db, new_page);
				}
			}
			*status = REDISLITE_NOT_FOUND;
			return NULL;
		}
	}
	*status = REDISLITE_NOT_FOUND;
	return NULL;
}

int redislite_page_index_type(void *_db, void *_cs, void *first_page, char *key, size_t length, char *type)
{
	redislite *db = (redislite *)_db;
	int status;
	redislite_page_index_key *index_key = redislite_index_key_for_index_name(db, _cs, first_page, key, length, &status, NULL);
	if (status == REDISLITE_OOM) {
		return REDISLITE_OOM;
	}
	int ret = REDISLITE_NOT_FOUND;
	if (index_key != NULL) {
		ret = REDISLITE_OK;
		*type = index_key->type;
	}
	redislite_free_key(index_key);
	return ret;
}

int redislite_value_page_for_key(void *_db, void *_cs, void *first_page, char *key, size_t length, char *type)
{
	redislite *db = (redislite *)_db;
	int status;
	redislite_page_index_key *index_key = redislite_index_key_for_index_name(db, _cs, first_page, key, length, &status, NULL);
	if (status == REDISLITE_OOM) {
		return REDISLITE_OOM;
	}
	int ret = REDISLITE_NOT_FOUND;
	if (index_key != NULL) {
		ret = index_key->left_page;
		if (type) {
			*type = index_key->type;
		}
	}
	redislite_free_key(index_key);
	return ret;
}

int redislite_exists_key(void *_db, void *_cs, void *first_page, char *key, size_t length)
{
	redislite *db = (redislite *)_db;
	int status;
	redislite_page_index_key *index_key = redislite_index_key_for_index_name(db, _cs, first_page, key, length, &status, NULL);
	if (status != REDISLITE_OK) {
		return status;
	}
	if (index_key != NULL) {
		redislite_free_key(index_key);
		return 1;
	}
	return 0;
}

int redislite_delete_key(void *_cs, void *first_page, char *key, size_t length, int delete_data)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = cs->db;
	int status, page_num;
	redislite_page_index_key *index_key = redislite_index_key_for_index_name(db, _cs, first_page, key, length, &status, &page_num);
	if (status != REDISLITE_OK) {
		return status;
	}
	if (index_key != NULL) {
		if (index_key->page) {
			int left_page = index_key->left_page;
			char type = index_key->type;
			status = redislite_remove_key(_cs, index_key, page_num);
			if (first_page == db->root) {
				((redislite_page_index_first *)first_page)->number_of_keys--;
				status = redislite_add_modified_page(cs, 0, REDISLITE_PAGE_TYPE_FIRST, db->root);
			}
			index_key = NULL;
			if (status == REDISLITE_OK && delete_data) {
				redislite_page_delete(_cs, left_page, type);
			}
		}
		else {
			status = REDISLITE_ERR;
		}
	}
	if (index_key) {
		redislite_free_key(index_key);
	}
	return status;
}

int redislite_delete_keys(void *_cs, int q, char **keys, size_t *lengths)
{
	changeset *cs = (changeset *)_cs;
	int status = REDISLITE_OK;
	int i;
	int counter = 0;
	for (i = 0; i < q; i++) {
		status = redislite_delete_key(_cs, cs->db->root, keys[i], lengths[i], 1);
		if (status != REDISLITE_OK && status != REDISLITE_NOT_FOUND) {
			break;
		}
		if (status == REDISLITE_OK) {
			counter++;
		}
	}

	return counter;
}

int redislite_insert_key(void *_cs, void *first_page, int first_page_num, char *key, size_t length, int left, char type)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = cs->db;
	if (db->readonly) {
		return REDISLITE_READONLY;
	}

	size_t i, pos;
	int cmp_result, should_continue;
	redislite_page_index *page = ((redislite_page_index_first *)first_page)->page;
	((redislite_page_index_first *)first_page)->number_of_keys++;
	int page_num = first_page_num;
	int status = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_FIRST, first_page);
	if (status < 0) {
		return status;
	}

	while (page != NULL) {
		should_continue = 0;
		pos = 0;
		for (i = 0; i < page->number_of_keys; i++) {
			cmp_result = (memcmp(page->keys[i]->keyname, key, MIN(page->keys[i]->keyname_size, length)));
			if (cmp_result == 0) {
				if (page->keys[i]->keyname_size == length) {
					if (page->keys[i]->type == REDISLITE_PAGE_TYPE_INDEX) {
						redislite_page_index *new_page = redislite_page_get(db, cs, page->keys[i]->left_page, REDISLITE_PAGE_TYPE_INDEX);
						if (new_page == NULL) {
							return REDISLITE_OOM;
						}
						page_num = page->keys[i]->left_page;
						if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
							redislite_free_index(db, page);
						}
						page = new_page;
						should_continue = 1;
						break;
					}
					redislite_page_delete(_cs, page->keys[i]->left_page, page->keys[i]->type);
					page->keys[i]->left_page = left;
					page->keys[i]->type = type;
					int result;
					((redislite_page_index_first *)first_page)->number_of_keys--;
					if (page != ((redislite_page_index_first *)first_page)->page) {
						result = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_INDEX, page);
					}
					else {
						result = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_FIRST, first_page);
					}
					if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
						redislite_free_index(db, page);
					}
					if (result < 0) {
						return result;
					}
					return REDISLITE_OK;
				}
				cmp_result = (page->keys[i]->keyname_size > length ? 1 : -1);
			}
			if (cmp_result < 0) {
				pos = i + 1;
			}
			else {
				break;
			}
		}
		if (should_continue) {
			continue;
		}
		if (pos < page->number_of_keys) {
			if (page->keys[pos]->type == REDISLITE_PAGE_TYPE_INDEX) {
				redislite_page_index *new_page = redislite_page_get(db, cs, page->keys[pos]->left_page, REDISLITE_PAGE_TYPE_INDEX);
				if (new_page == NULL) {
					return REDISLITE_OOM;
				}
				page_num = page->keys[pos]->left_page;
				if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
					redislite_free_index(db, page);
				}
				page = new_page;
				continue;
			}
		}
		else {
			if (page->right_page > 0) {
				redislite_page_index *new_page = redislite_page_get(db, cs, page->right_page, REDISLITE_PAGE_TYPE_INDEX);
				if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
					redislite_free_index(db, page);
				}
				if (new_page == NULL) {
					return REDISLITE_OOM;
				}
				page_num = page->right_page;
				page = new_page;
				continue;
			}
		}

		int result = redislite_page_index_add_key(cs, page, pos, left, key, length, type);
		if (result == REDISLITE_OK) {
			if (page != ((redislite_page_index_first *)first_page)->page) {
				result = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_INDEX, page);
			}
			else {
				result = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_FIRST, first_page);
			}
			if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
				redislite_free_index(db, page);
			}
			if (result < 0) {
				return -result;
			}
			return REDISLITE_OK;
		}
		if (result == REDISLITE_OOM || result == REDISLITE_OK) {
			if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
				redislite_free_index(db, page);
			}
			return result;
		}

		int previous_page_num = page_num;
		char next_type;
		if (pos == page->number_of_keys) {
			page_num = page->right_page;
			next_type = REDISLITE_PAGE_TYPE_INDEX;
		}
		else {
			page_num = page->keys[pos]->left_page;
			next_type = page->keys[pos]->type;
		}

		if (page_num == 0) {
			redislite_page_index *new_page = redislite_page_index_create(db);
			if (new_page == NULL) {
				if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
					redislite_free_index(db, page);
				}
				return REDISLITE_OOM;
			}

			int r = redislite_page_index_add_key(cs, new_page, 0, left, key, length, type);
			if (r != REDISLITE_OK) {
				redislite_free(new_page);
				if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
					redislite_free_index(db, page);
				}
				return r;
			}

			page_num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_INDEX, new_page);
			if (pos < page->number_of_keys) {
				int r = redislite_page_index_add_key(cs, new_page, 0, page->keys[pos]->left_page, page->keys[pos]->keyname, page->keys[pos]->keyname_size, type);
				if (r != REDISLITE_OK) {
					redislite_free(new_page);
					if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
						redislite_free_index(db, page);
					}
					return r;
				}
			}

			if (pos == page->number_of_keys) {
				page->right_page = page_num;
			}
			else {
				page->keys[pos]->left_page = page_num;
			}
			if (page != ((redislite_page_index_first *)first_page)->page) {
				result = redislite_add_modified_page(cs, previous_page_num, REDISLITE_PAGE_TYPE_INDEX, page);
			}
			else {
				result = redislite_add_modified_page(cs, previous_page_num, REDISLITE_PAGE_TYPE_FIRST, first_page);
			}
			if (result < 0) {
				return result;
			}
			if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
				redislite_free_index(db, page);
			}
			return REDISLITE_OK;
		}

		if (next_type == REDISLITE_PAGE_TYPE_INDEX) {
			redislite_page_index *new_page = redislite_page_get(db, cs, page_num, REDISLITE_PAGE_TYPE_INDEX);
			if (new_page == NULL) {
				if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
					redislite_free_index(db, page);
				}
				return REDISLITE_OOM;
			}
			if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
				redislite_free_index(db, page);
			}
			page = new_page;
		}
		else {
			redislite_page_index *new_index_page = redislite_page_index_create(db);
			if (new_index_page == NULL) {
				if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
					redislite_free_index(db, page);
				}
				return REDISLITE_OOM;
			}
			/* assert pos<size */
			int r = redislite_page_index_add_key(cs, new_index_page, 0, page->keys[pos]->left_page, page->keys[pos]->keyname, page->keys[pos]->keyname_size, next_type);
			if (r != REDISLITE_OK) {
				redislite_free(new_index_page);
				if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
					redislite_free_index(db, page);
				}
				return r;
			}
			page_num = page->keys[pos]->left_page = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_INDEX, new_index_page);
			page->keys[pos]->page = new_index_page;
			page->keys[pos]->type = REDISLITE_PAGE_TYPE_INDEX;
			if (cs == NULL && page != ((redislite_page_index_first *)db->root)->page) {
				redislite_free_index(db, page);
			}
			if (page != ((redislite_page_index_first *)first_page)->page) {
				result = redislite_add_modified_page(cs, previous_page_num, REDISLITE_PAGE_TYPE_INDEX, page);
			}
			else {
				result = redislite_add_modified_page(cs, previous_page_num, REDISLITE_PAGE_TYPE_FIRST, first_page);
			}
			if (result < 0) {
				return result;
			}

			redislite_add_modified_page(cs, 0, REDISLITE_PAGE_TYPE_FIRST, db->root);
			page = new_index_page;
		}
	}
	return REDISLITE_NOT_FOUND;
}

int redislite_page_index_rename_key(void *_cs, void *first_page, char *src, size_t src_len, char *target, size_t target_len)
{
	if (src_len == target_len && memcmp(src, target, target_len) == 0) {
		return REDISLITE_SOURCE_DESTINATION_SAME;
	}
	changeset *cs = (changeset *)_cs;

	int status;
	redislite_page_index_key *key = redislite_index_key_for_index_name(cs->db, cs, first_page, src, src_len, &status, NULL);
	if (status != REDISLITE_OK) {
		return status;
	}
	status = redislite_insert_key(_cs, first_page, 0, target, target_len, key->left_page, key->type);
	if (status != REDISLITE_OK) {
		return status;
	}
	status = redislite_delete_key(_cs, cs->db->root, src, src_len, 0); // TODO: avoid double lookup
	return status;
}

int redislite_page_index_renamenx_key(void *_cs, void *first_page, char *src, size_t src_len, char *target, size_t target_len)
{
	changeset *cs = (changeset *)_cs;

	int status;
	redislite_index_key_for_index_name(cs->db, cs, first_page, target, target_len, &status, NULL);
	if (status == REDISLITE_NOT_FOUND) {
		return redislite_page_index_rename_key(_cs, first_page, src, src_len, target, target_len);
	}
	else if (status == REDISLITE_OK) {
		return REDISLITE_ALREADY_EXISTS;
	}
	else {
		return status;
	}
}

int redislite_page_index_add_key(void *_cs, redislite_page_index *page, int pos, int left, char *key, size_t length, char type)
{
	changeset *cs = (changeset *)_cs;
	redislite_page_index_key *index_key = redislite_malloc(sizeof(redislite_page_index_key));
	if (index_key == NULL) {
		return REDISLITE_OOM;
	}

	unsigned char length_str[9];
	size_t new_key_length = length + 5;
	new_key_length += putVarint32(length_str, length);
	if (page->free_space < new_key_length) {
		redislite_free(index_key);
		return REDISLITE_ERR;
	}

	if (page->alloced_keys == 0) {
		page->alloced_keys = 10;
		page->keys = redislite_malloc(sizeof(redislite_page_index_key) * page->alloced_keys);
	}
	else if (page->alloced_keys == page->number_of_keys) {
		redislite_page_index_key **new_keys = redislite_realloc(page->keys, sizeof(redislite_page_index_key) * page->alloced_keys * 2);
		if (new_keys == NULL) {
			return REDISLITE_OOM;
		}
		page->keys = new_keys;
		page->alloced_keys *= 2;
	}

	index_key->type = type;
	index_key->page = page;
	index_key->keyname_size = length;
	index_key->keyname = redislite_malloc(length * sizeof(char));
	if (index_key->keyname == NULL) {
		redislite_free(index_key);
		return REDISLITE_OOM;
	}
	memcpy(index_key->keyname, key, length);
	index_key->left_page = left;

	size_t i, target_pos;
	if (pos < 0) {
		target_pos = page->number_of_keys;
	}
	else {
		target_pos = (size_t)pos;
	}

	if (page->number_of_keys > 0) {
		for (i = page->number_of_keys - 1; i >= target_pos; --i) {
			page->keys[i + 1] = page->keys[i];
			if (i == 0) {
				break;
			}
		}
	}
	page->keys[target_pos] = index_key;
	page->number_of_keys++;
	page->free_space -= new_key_length;
	if (type != REDISLITE_PAGE_TYPE_INDEX) {
		return redislite_add_modified_page(cs, 0, REDISLITE_PAGE_TYPE_FIRST, cs->db->root);
	}

	return REDISLITE_OK;
}

// TODO: unit test
int redislite_get_keys(void *_db, void *_cs, char *pattern, int pattern_len, int *number_of_keys_p, char ***keys_p, int **keys_length_p)
{
	redislite *db = (redislite *)_db;
	size_t i = 0, keys_pos = 0;
	size_t keys_alloced = 20;
	int *keys_length = NULL, *pages = NULL;

	char **keys = redislite_malloc(sizeof(char *) * keys_alloced);
	if (keys == NULL) {
		goto cleanup;
	}
	keys_length = redislite_malloc(sizeof(int) * keys_alloced);
	if (keys_length == NULL) {
		goto cleanup;
	}

	size_t pages_pos = 0;
	size_t pages_alloced = 20;
	pages = redislite_malloc(sizeof(int) * pages_alloced);
	if (pages == NULL) {
		goto cleanup;
	}
	pages[0] = 0;

	redislite_page_index *page;
	int current_page_num;
	int allkeys = pattern_len == 2 && (pattern[0] == '*' && pattern[1] == '\0');
	while (1) {
		if (pages[pages_pos] == 0) {
			page = ((redislite_page_index_first *)db->root)->page;
		}
		else {
			page = redislite_page_get(db, _cs, pages[pages_pos], REDISLITE_PAGE_TYPE_INDEX);
		}
		current_page_num = pages[pages_pos];

		if (page == NULL) {
			goto cleanup;
		}

		if (pages_alloced < pages_pos + page->number_of_keys) {
			pages_alloced *= 2;
			if (pages_alloced < pages_pos + page->number_of_keys) {
				pages_alloced = pages_pos + page->number_of_keys;
			}
			int *pages_tmp = redislite_realloc(pages, sizeof(char *) * pages_alloced);
			if (pages_tmp == NULL) {
				goto cleanup;
			}
			pages = pages_tmp;
		}

		if (keys_alloced < keys_pos + page->number_of_keys + 1) {
			keys_alloced *= 2;
			if (keys_alloced < keys_pos + page->number_of_keys) {
				keys_alloced = keys_pos + page->number_of_keys + 1;
			}
			char **keys_tmp = redislite_realloc(keys, sizeof(char *) * keys_alloced);
			if (keys_tmp == NULL) {
				goto cleanup;
			}
			keys = keys_tmp;

			int *keys_length_tmp = redislite_realloc(keys_length, sizeof(int) * keys_alloced);
			if (keys_length_tmp == NULL) {
				goto cleanup;
			}
			keys_length = keys_length_tmp;
		}

		if (page->right_page) {
			pages[pages_pos++] = page->right_page;
		}
		if (page == ((redislite_page_index_first *)db->root)->page || (_cs != NULL && (void *)page == redislite_modified_page(_cs, pages[pages_pos - 1])->data)) {
			// should copy
			for (i = 0; i < page->number_of_keys; i++) {
				if (page->keys[i]->type == REDISLITE_PAGE_TYPE_INDEX) {
					pages[pages_pos++] = page->keys[i]->left_page;
				}
				else if (allkeys || redislite_stringmatchlen(pattern, pattern_len, page->keys[i]->keyname, page->keys[i]->keyname_size, 0)) {
					keys[keys_pos] = redislite_malloc(sizeof(char *) * page->keys[i]->keyname_size);
					if (keys[keys_pos] == NULL) {
						goto cleanup;
					}
					memcpy(keys[keys_pos], page->keys[i]->keyname, page->keys[i]->keyname_size);
					keys_length[keys_pos] = page->keys[i]->keyname_size;
					keys_pos++;
				}
			}
		}
		else {
			// should NOT copy
			for (i = 0; i < page->number_of_keys; i++) {
				if (page->keys[i]->type == REDISLITE_PAGE_TYPE_INDEX) {
					pages[pages_pos++] = page->keys[i]->left_page;
				}
				else if (allkeys || redislite_stringmatchlen(pattern, pattern_len, page->keys[i]->keyname, page->keys[i]->keyname_size, 0)) {
					keys[keys_pos] = page->keys[i]->keyname;
					keys_length[keys_pos] = page->keys[i]->keyname_size;
					keys_pos++;
				}
			}
			if (page != ((redislite_page_index_first *)db->root)->page) {
				for (i = 0; i < page->number_of_keys; i++) {
					redislite_free(page->keys[i]); // not freeing the char*, only the key
				}
				if (_cs == NULL) {
					redislite_free(page->keys);
					redislite_free(page);
				}
				else {
					// we are cheating here!
					page->number_of_keys = 0;
					redislite_close_opened_page(_cs, current_page_num);
				}
			}
		}

		if (pages_pos == 0) {
			break;
		}
		pages_pos--;
	}
	redislite_free(pages);

	// downsizing the arrays, we don't need to consume more memory than we are consuming now
	if (keys_alloced > keys_pos && keys_pos > 0) {
		keys_alloced = keys_pos;
		char **keys_tmp = redislite_realloc(keys, sizeof(char *) * keys_alloced);
		if (keys_tmp == NULL) {
			goto cleanup;
		}
		keys = keys_tmp;

		int *keys_length_tmp = redislite_realloc(keys_length, sizeof(int) * keys_alloced);
		if (keys_length_tmp == NULL) {
			goto cleanup;
		}
		keys_length = keys_length_tmp;
	}
	else if (keys_pos == 0) {
		redislite_free(keys_length);
		redislite_free(keys);
		keys = NULL;
		keys_length = NULL;
	}

	*number_of_keys_p = keys_pos;
	*keys_p = keys;
	*keys_length_p = keys_length;
	return REDISLITE_OK;

cleanup:
	if (keys) {
		for (; keys_pos > 0; keys_pos--) {
			redislite_free(keys[keys_pos] - 1);
		}
		redislite_free(keys);
	}
	if (keys_length) {
		redislite_free(keys_length);
	}
	if (pages) {
		redislite_free(pages);
	}
	return REDISLITE_OOM;
}

int redislite_get_random_key_name(void *_db, void *_cs, char **key_p, size_t *key_length_p)
{
	redislite *db = (redislite *)_db;
	size_t i = 0;
	char *key = NULL;
	int key_length = 0, *pages = NULL;

	size_t pages_pos = 0;
	size_t pages_alloced = 20;
	pages = redislite_malloc(sizeof(int) * pages_alloced);
	if (pages == NULL) {
		goto cleanup;
	}
	pages[0] = 0;

	redislite_page_index *page;
	int current_page_num;
	size_t number_of_keys = ((redislite_page_index_first *)db->root)->number_of_keys;
	if (number_of_keys == 0) {
		return REDISLITE_NOT_FOUND;
	}
	size_t random_position = (float)rand() / RAND_MAX * number_of_keys;
	while (1) {
		if (pages[pages_pos] == 0) {
			page = ((redislite_page_index_first *)db->root)->page;
		}
		else {
			page = redislite_page_get(db, _cs, pages[pages_pos], REDISLITE_PAGE_TYPE_INDEX);
		}
		current_page_num = pages[pages_pos];

		if (page == NULL) {
			goto cleanup;
		}

		if (pages_alloced < pages_pos + page->number_of_keys) {
			pages_alloced *= 2;
			if (pages_alloced < pages_pos + page->number_of_keys) {
				pages_alloced = pages_pos + page->number_of_keys;
			}
			int *pages_tmp = redislite_realloc(pages, sizeof(char *) * pages_alloced);
			if (pages_tmp == NULL) {
				goto cleanup;
			}
			pages = pages_tmp;
		}

		if (page->right_page) {
			pages[pages_pos++] = page->right_page;
		}
		for (i = 0; i < page->number_of_keys; i++) {
			if (page->keys[i]->type == REDISLITE_PAGE_TYPE_INDEX) {
				pages[pages_pos++] = page->keys[i]->left_page;
			}
			else {
				if (random_position-- == 0) {
					key = redislite_malloc(sizeof(char) * page->keys[i]->keyname_size);
					if (key == NULL) {
						goto cleanup;
					}
					memcpy(key, page->keys[i]->keyname, page->keys[i]->keyname_size);
					key_length = page->keys[i]->keyname_size;
				}
			}
		}

		if (pages_pos == 0) {
			break;
		}
		pages_pos--;
	}
	redislite_free(pages);

	*key_p = key;
	*key_length_p = key_length;
	return REDISLITE_OK;

cleanup:
	if (key) {
		redislite_free(key);
	}
	if (pages) {
		redislite_free(pages);
	}
	return REDISLITE_OOM;
}

int redislite_flush(void *_cs)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = cs->db;

	redislite_page_index *page = (redislite_page_index *)((redislite_page_index_first *)db->root)->page;
	size_t i;
	for (i = 0; i < page->number_of_keys; i++) {
		redislite_free_key(page->keys[i]);
	}
	redislite_free(page->keys);
	page->keys = NULL;
	page->number_of_keys = 0;
	page->right_page = 0;
	// TODO: multithread safeness
	db->number_of_pages = 0;
	db->first_freelist_page = 0;
	db->number_of_freelist_pages = 0;
	((redislite_page_index_first *)((redislite *)page->db)->root)->number_of_keys = 0;
	redislite_add_modified_page(_cs, 0, REDISLITE_PAGE_TYPE_FIRST, (redislite_page_index_first *)db->root);
	return REDISLITE_OK;
}
