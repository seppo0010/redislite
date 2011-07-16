#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"
#include "page_index.h"
#include "page_list.h"
#include "util.h"

void redislite_delete_list(void *_cs, void *_page)
{
	redislite_page_list *page = (redislite_page_list *)_page;
	if (page == NULL) {
		return;
	}
	if (page->right_page) {
		redislite_page_delete(_cs, page->right_page, REDISLITE_PAGE_TYPE_LIST);
	}
}

void redislite_free_list(void *_db, void *_page)
{
	redislite_page_list *page = (redislite_page_list *)_page;
	if (page == NULL) {
		return;
	}
	int i;
	for (i = 0; i < page->size; i++)
		if (page->element[i] != NULL) {
			redislite_free(page->element[i]);
		}
	if (page->element != NULL) {
		redislite_free(page->element);
	}
	if (page->element_len != NULL) {
		redislite_free(page->element_len);
	}
	redislite_free(page);
}

void redislite_write_list(void *_db, unsigned char *data, void *_page)
{
	redislite_page_list *page = (redislite_page_list *)_page;
	if (page == NULL) {
		return;
	}

	redislite_put_4bytes(&data[0], 0); // reserved
	redislite_put_4bytes(&data[4], page->size);
	redislite_put_4bytes(&data[8], page->right_page);
	redislite_put_4bytes(&data[12], page->left_page);

	int pos = 16;
	int i;
	for (i = 0; i < page->size; i++) {
		pos += putVarint32(&data[pos], page->element_len[i]);
		memcpy(&data[pos], page->element[i], page->element_len[i]);
		pos += page->element_len[i];
	}
}

void *redislite_read_list(void *_db, unsigned char *data)
{
	redislite_page_list *page = redislite_malloc(sizeof(redislite_page_list));
	if (page == NULL) {
		return NULL;
	}
	page->element_alloced =
	    page->size = redislite_get_4bytes(&data[4]);
	page->right_page = redislite_get_4bytes(&data[8]);
	page->left_page = redislite_get_4bytes(&data[12]);
	page->element_len = NULL; // in case the next alloc fails, we don't want to free this
	if (page->element_alloced == 0) {
		page->element = NULL;
		return page;
	}
	page->element = redislite_malloc(sizeof(char *) * page->size);
	if (page->element == NULL) {
		goto cleanup;
	}
	page->element_len = redislite_malloc(sizeof(int) * page->size);
	if (page->element_len == NULL) {
		goto cleanup;
	}
	int i, pos = 16;
	for (i = 0; i < page->size; i++) {
		pos += getVarint32(&data[pos], page->element_len[i]);
		page->element[i] = redislite_malloc(sizeof(char) * page->element_len[i]);
		if (page->element[i] == NULL) {
			goto cleanup;
		}
		memcpy(page->element[i], &data[pos], page->element_len[i]);
		pos += page->element_len[i];
	}
	return page;
cleanup:
	if (page->element) {
		for (; i >= 0; i--) {
			if (page->element[i]) {
				redislite_free(page->element[i]);
			}
		}
		redislite_free(page->element);
	}
	redislite_free(page);
	return NULL;

}

void redislite_delete_list_first(void *_cs, void *_page)
{
	redislite_page_list_first *page = (redislite_page_list_first *)_page;
	if (page == NULL) {
		return;
	}
	redislite_delete_list(_cs, page->list);
}

void redislite_free_list_first(void *_db, void *_page)
{
	redislite_page_list_first *page = (redislite_page_list_first *)_page;
	if (page == NULL) {
		return;
	}
	redislite_free_list(_db, page->list);
	redislite_free(page);
}

void redislite_write_list_first(void *_db, unsigned char *data, void *_page)
{
	redislite_page_list_first *page = (redislite_page_list_first *)_page;
	if (page == NULL) {
		return;
	}
	//we are writing on pos=8 since the first 4 bytes are reserved
	//and we don't need them
	redislite_write_list(_db, &data[4], page->list);

	redislite_put_4bytes(&data[0], 0); // reserved
	redislite_put_4bytes(&data[4], page->total_size);
}

void *redislite_read_list_first(void *_db, unsigned char *data)
{
	void *list = redislite_read_list(_db, &data[4]);
	if (list == NULL) {
		return NULL;
	}
	redislite_page_list_first *page = redislite_malloc(sizeof(redislite_page_list_first));
	page->list = list;
	page->total_size = redislite_get_4bytes(&data[4]);
	return page;
}

static int lpush(void *_cs, redislite_page_list *list, char *value, int value_len)
{
	char *element = redislite_malloc(sizeof(char) * value_len);
	if (element == NULL) {
		return REDISLITE_OOM;
	}
	memcpy(element, value, value_len);
	if (list->element_alloced == list->size) {
		if (list->element_alloced == 0) {
			void *element = redislite_malloc(sizeof(char *) * (list->size + 1));
			if (element == NULL) {
				return REDISLITE_OOM;
			}
			int *element_len = redislite_malloc(sizeof(int) * (list->size + 1));
			if (element_len == NULL) {
				redislite_free(element);
				return REDISLITE_OOM;
			}
			list->element = element;
			list->element_len = element_len;
		}
		else {
			void *element = redislite_realloc(list->element, sizeof(char *) * (list->size + 1));
			if (element == NULL) {
				return REDISLITE_OOM;
			}
			int *element_len = redislite_realloc(list->element_len, sizeof(int) * (list->size + 1));
			if (element_len == NULL) {
				return REDISLITE_OOM;    // TODO: rollback element realloc?
			}
			list->element = element;
			list->element_len = element_len;
		}
		list->element_alloced++;
	}

	int i;
	for (i = list->size; i > 0; i--) {
		list->element[i] = list->element[i - 1];
		list->element_len[i] = list->element_len[i - 1];
	}
	list->element[0] = element;
	list->element_len[0] = value_len;
	list->size++;
	return REDISLITE_OK;
}

size_t redislite_free_bytes(void *_db, redislite_page_list *page, char type)
{
	redislite *db = (redislite *)_db;
	int i, pos = type == REDISLITE_PAGE_TYPE_LIST_FIRST ? 20 : 16;
	unsigned char placeholder[9];
	if (page->element_len) {
		for (i = 0; i < page->size; i++) {
			pos += putVarint32(placeholder, page->element_len[i]) + page->element_len[i];
		}
	}
	return db->page_size - pos;
}

int redislite_lpushx_by_keyname(void *_cs, char *keyname, int keyname_len, char *value, int value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, keyname, keyname_len, &type);
	if (page_num < 0 && page_num != REDISLITE_NOT_FOUND) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_ERR; // TODO: error for wrong type
	}

	if (page_num == 0) {
		return 0;
	}

	return redislite_lpush_page_num(_cs, &page_num, value, value_len);
}

int redislite_lpush_by_keyname(void *_cs, char *keyname, int keyname_len, char *value, int value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, keyname, keyname_len, &type);
	if (page_num < 0 && page_num != REDISLITE_NOT_FOUND) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_ERR; // TODO: error for wrong type
	}

	int is_new = page_num == REDISLITE_NOT_FOUND;
	int status = redislite_lpush_page_num(_cs, &page_num, value, value_len);
	if (status == REDISLITE_OK) {
		if (is_new) {
			status = redislite_insert_key(cs, keyname, keyname_len, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
			if (status < 0) {
				return status;
			}
		}
	}
	return status;
}

int redislite_lpush_page_num(void *_cs, int *page_num_p, char *value, int value_len)
{
	changeset *cs = (changeset *)_cs;
	int status;
	char type = REDISLITE_PAGE_TYPE_LIST_FIRST;

	redislite_page_list_first *page = NULL;
	if (*page_num_p > 0) {
		page = redislite_page_get(cs->db, cs, *page_num_p, REDISLITE_PAGE_TYPE_LIST_FIRST);
	}

	if (*page_num_p == REDISLITE_NOT_FOUND) {
		page = redislite_malloc(sizeof(redislite_page_list_first));
		if (page == NULL) {
			return REDISLITE_OOM;
		}
		page->list = redislite_malloc(sizeof(redislite_page_list));
		if (page->list == NULL) {
			redislite_free(page);
			return REDISLITE_OOM;
		}
		page->total_size = 0;
		page->list->left_page = 0;
		page->list->right_page = 0;
		page->list->element_alloced = page->list->size = 0;
		page->list->element_len = 0;
		type = REDISLITE_PAGE_TYPE_LIST_FIRST;
	}
	else {
		page = redislite_page_get(cs->db, _cs, *page_num_p, type);
	}

	if (page == NULL) {
		return REDISLITE_ERR;
	}

	if (type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	int size = value_len;
	{
		unsigned char length_str[9];
		size += putVarint32(length_str, value_len);
	}

	redislite_page_list *right = NULL;
	redislite_page_list *old_right = NULL;
	while (redislite_free_bytes(cs->db, page->list, type) < size) {
		if (page->list->size == 0) {
			printf("Popped all elements to make space -- value too big?\n");
			redislite_free(right);
			return REDISLITE_ERR;
		}
		if (right == NULL) {
			// We have no space on this page; trying to move things to the next
			// If that fails, we are gonna create a new one in between
			if (page->list->right_page != 0) {
				old_right = redislite_page_get(cs->db, cs, page->list->right_page, REDISLITE_PAGE_TYPE_LIST);
				int i = 0, tmp_size;
				unsigned char placeholder[9];
				while (page->list->size > 0) {
					tmp_size = page->list->element_len[page->list->size - 1] + putVarint32(placeholder, page->list->element_len[page->list->size - 1]);
					if (redislite_free_bytes(cs->db, old_right, REDISLITE_PAGE_TYPE_LIST) < tmp_size) {
						break;
					}
					if (redislite_free_bytes(cs->db, page->list, type) >= size) {
						break;
					}
					status = lpush(_cs, old_right, page->list->element[page->list->size - 1], page->list->element_len[page->list->size - 1]);
					page->list->size--;
					i++;
				}
				if (redislite_free_bytes(cs->db, page->list, type) >= size) {
					for (; i > 0; i--) {
						redislite_free(page->list->element[i + page->list->size - 1]);
					}
					redislite_add_modified_page(cs, page->list->right_page, REDISLITE_PAGE_TYPE_LIST, old_right);
					break;
				}
				else {
					page->list->size += i;
					old_right->size -= i;
				}
			}

			right = redislite_malloc(sizeof(redislite_page_list));
			if (right == NULL) {
				return REDISLITE_OOM;
			}
			right->left_page = *page_num_p;
			right->right_page = page->list->right_page;
			right->element_alloced = right->size = 0;
			right->element_len = 0;
		}
		status = lpush(_cs, right, page->list->element[page->list->size - 1], page->list->element_len[page->list->size - 1]);
		if (status != REDISLITE_OK) {
			redislite_free(right);
			return status;
		}
		page->list->size--;
		redislite_free(page->list->element[page->list->size]);
	}
	if (right != NULL) {
		page->list->right_page = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_LIST, right);
		if (old_right != NULL) {
			old_right->left_page = page->list->right_page;
		}
		else {
			page->list->left_page = page->list->right_page;
		}
	}

	{
		status = lpush(cs, page->list, value, value_len);
		if (status != REDISLITE_OK) {
			return status;
		}
		page->total_size++;
		int is_new = *page_num_p == REDISLITE_NOT_FOUND;
		if (is_new) {
			*page_num_p = -1;
		}
		*page_num_p = redislite_add_modified_page(cs, *page_num_p, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
		if (*page_num_p < 0) {
			redislite_free(page->list);
			redislite_free(page);
			return *page_num_p;
		}
	}
	return status;
}

int redislite_lpop_by_keyname(void *_cs, char *keyname, int keyname_len, char **value, int *value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int i;
	int page_num = redislite_value_page_for_key(cs->db, cs, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}

	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(cs->db, cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);

	redislite_page_list *list = page->list;
	int list_page_num = page_num;
	while (list->size == 0) {
		list_page_num = list->right_page;
		list = redislite_page_get(cs->db, cs, list->right_page, REDISLITE_PAGE_TYPE_LIST);
	}

	if (page->list->size == 0 && list_page_num == page->list->right_page && redislite_free_bytes(cs->db, list, REDISLITE_PAGE_TYPE_FIRST) > 0 && list->size > 0) {
		for (i = 0; i < list->size; i++)
			lpush(cs, page->list, list->element[list->size - i - 1], list->element_len[list->size - i - 1]);
		page->list->right_page = list->right_page;
		list->right_page = list->left_page = 0;
		redislite_page_delete(_cs, list_page_num, REDISLITE_PAGE_TYPE_LIST);
		list_page_num = page_num;

		if (page->list->right_page > 0) {
			list = redislite_page_get(cs->db, cs, page->list->right_page, REDISLITE_PAGE_TYPE_LIST);
			list->left_page = page_num;
			redislite_add_modified_page(cs, page->list->right_page, REDISLITE_PAGE_TYPE_LIST, list);
		}
		list = page->list;
		// TODO: delete old page
	}

	*value = list->element[0];
	*value_len = list->element_len[0];
	for (i = 1; i < list->size; i++) {
		list->element[i - 1] = list->element[i];
		list->element_len[i - 1] = list->element_len[i];
	}
	list->size--;
	page->total_size--;

	if (page->total_size == 0) {
		char *value_aux = redislite_malloc(sizeof(char) * (*value_len));
		if (value_aux == NULL) {
			return REDISLITE_OOM;
		}
		memcpy(value_aux, *value, *value_len);
		redislite_delete_key(_cs, keyname, keyname_len);
		redislite_free(*value);
		*value = value_aux;
		// TODO: avoid double key lookup
	}
	else {
		redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
		if (list != page->list) {
			redislite_add_modified_page(cs, list_page_num, REDISLITE_PAGE_TYPE_LIST, list);
		}
	}

	return REDISLITE_OK;
}

int redislite_llen_by_keyname(void *_db, void *_cs, char *keyname, int keyname_len, int *len)
{
	char type;
	int page_num = redislite_value_page_for_key(_db, _cs, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(_db, _cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	*len = page->total_size;
	if (_cs == NULL) {
		redislite_free_list_first(_db, page);
	}
	return REDISLITE_OK;
}

int redislite_lrange_by_keyname(void *_db, void *_cs, char *keyname, int keyname_len, int start, int end, int *ret_list_count_p, char ***ret_list_p, int **ret_list_len_p)
{
	char type;
	int page_num = redislite_value_page_for_key(_db, _cs, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(_db, _cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	if (page == NULL) {
		return REDISLITE_OOM;
	}
	int len = page->total_size;

	if (start < 0) {
		start += len;
	}
	if (end < 0) {
		end += len;
	}
	if (start < 0) {
		start = 0;
	}
	if (end >= len) {
		end = len;
	}

	if (start > end || start >= len) {
		*ret_list_count_p = 0;
		return REDISLITE_OK;
	}

	int ret_list_count = end - start + 1;
	char **ret_list = redislite_malloc(sizeof(char *) * ret_list_count);
	if (ret_list == NULL) {
		return REDISLITE_OOM;
	}
	int *ret_list_len = redislite_malloc(sizeof(int) * ret_list_count);
	if (ret_list_len == NULL) {
		redislite_free(ret_list);
		return REDISLITE_OOM;
	}

	redislite_page_list *list = page->list;
	int page_start = 0;
	int pos = 0;
	int i, right_page;
	while (page_start <= end) {
		if (page_start + list->size > start) {
			for (i = 0; i < list->size && pos < ret_list_count; i++) {
				if (page_start + i >= start) {
					ret_list[pos] = redislite_malloc(sizeof(char) * list->element_len[i]);
					if (ret_list[pos] == NULL) {
						goto cleanup;
					}
					memcpy(ret_list[pos], list->element[i], list->element_len[i]);
					ret_list_len[pos] = list->element_len[i];
					pos++;
				}
			}
		}
		if (pos == ret_list_count) {
			break;
		}
		page_start += list->size;
		right_page = list->right_page;
		if (_cs == NULL && page->list != list) {
			redislite_free_list(_db, list);
		}
		list = redislite_page_get(_db, _cs, right_page, REDISLITE_PAGE_TYPE_LIST);
		if (page == NULL) {
			goto cleanup;
		}
	}

	if (_cs == NULL && page->list != list) {
		redislite_free_list(_db, list);
	}
	redislite_free_list_first(_db, page);
	*ret_list_p = ret_list;
	*ret_list_len_p = ret_list_len;
	*ret_list_count_p = ret_list_count;
	return REDISLITE_OK;
cleanup:
	for (; pos >= 0; pos--) {
		redislite_free(ret_list[i]);
	}
	redislite_free(ret_list);
	redislite_free(ret_list_len);
	return REDISLITE_OOM;
}

int redislite_lindex_by_keyname(void *_db, void *_cs, char *keyname, int keyname_len, int pos, char **value, int *value_len)
{
	// We are gonna keep constant time for pos=-1
	// pos=0 is already as fast as possible in lrange
	if (pos != -1) {
		int ret_list_count;
		int *ret_list_len;
		char **ret_list;
		int status = redislite_lrange_by_keyname(_db, _cs, keyname, keyname_len, pos, pos, &ret_list_count, &ret_list, &ret_list_len);
		if (status == REDISLITE_OK && ret_list_count == 1) {
			*value = ret_list[0];
			*value_len = ret_list_len[0];
			redislite_free(ret_list);
			redislite_free(ret_list_len);
		}
		else {
			*value = NULL;
			*value_len = 0;
		}
		return status;
	}
	char type;
	int page_num = redislite_value_page_for_key(_db, _cs, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}

	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(_db, _cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	if (page == NULL) {
		goto cleanup;
	}

	redislite_page_list *list;
	if (page->list->left_page == 0) {
		list = page->list;
	}
	else {
		list = redislite_page_get(_db, _cs, page->list->left_page, REDISLITE_PAGE_TYPE_LIST);
		if (list == NULL) {
			goto cleanup;
		}
	}
	char *ret_value = redislite_malloc(sizeof(char) * list->element_len[list->size - 1]);
	if (ret_value == NULL) {
		goto cleanup;
	}
	memcpy(ret_value, list->element[list->size - 1], list->element_len[list->size - 1]);
	*value = ret_value;
	*value_len = list->element_len[list->size - 1];
	if (list != NULL && list != page->list) {
		redislite_free_list(_db, list);
	}
	if (page != NULL) {
		redislite_free_list_first(_db, page);
	}
	return REDISLITE_OK;

cleanup:
	if (_cs == NULL) {
		if (list != NULL && list != page->list) {
			redislite_free_list(_db, list);
		}
		if (page != NULL) {
			redislite_free_list_first(_db, page);
		}
	}
	return REDISLITE_OOM;
}
