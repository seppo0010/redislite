#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "redislite.h"
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
	page->element = redislite_malloc(sizeof(char *) * page->size);
	if (page->element == NULL) {
		goto cleanup;
	}
	page->element_len = redislite_malloc(sizeof(char *) * page->size);
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

int redislite_lpush_by_keyname(void *_cs, char *keyname, int keyname_len, char *value, int value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int status;
	int page_num = redislite_value_page_for_key(cs->db, cs, keyname, keyname_len, &type);
	if (page_num < 0 && page_num != REDISLITE_NOT_FOUND) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_ERR; // TODO: error for wrong type
	}

	redislite_page_list_first *page = NULL;
	if (page_num > 0) {
		page = redislite_page_get(cs->db, cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	}

	if (page_num == REDISLITE_NOT_FOUND) {
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
		page->list->size = 0;
		page->list->element_alloced = page->list->size = 0;
		page->list->element_len = 0;
		type = REDISLITE_PAGE_TYPE_LIST_FIRST;
	}
	else {
		page = redislite_page_get(cs->db, _cs, page_num, type);
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
				int i = 1;
				while (redislite_free_bytes(cs->db, old_right, REDISLITE_PAGE_TYPE_LIST) >= 0 && page->list->size > 0) {
					status = lpush(_cs, old_right, page->list->element[page->list->size - 1], page->list->element_len[page->list->size - 1]);
					page->list->size--;
					i++;
				}
				if (redislite_free_bytes(cs->db, page->list, type) >= size) {
					redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_LIST, old_right);
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
			right->left_page = page_num;
			right->right_page = page->list->right_page;
			right->size = 0;
			right->element_alloced = right->size = 0;
			right->element_len = 0;
		}
		status = lpush(_cs, right, page->list->element[page->list->size - 1], page->list->element_len[page->list->size - 1]);
		if (status != REDISLITE_OK) {
			redislite_free(right);
			return status;
		}
		page->list->size--;
	}
	if (right != NULL) {
		page->list->right_page = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_LIST, right);
		if (old_right != NULL) {
			old_right->left_page = page->list->right_page;
		}
	}

	{
		status = lpush(cs, page->list, value, value_len);
		if (status != REDISLITE_OK) {
			return status;
		}
		page->total_size++;
		int is_new = page_num == REDISLITE_NOT_FOUND;
		if (is_new) {
			page_num = -1;
		}
		page_num = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
		if (page_num < 0) {
			redislite_free(page->list);
			redislite_free(page);
			return page_num;
		}
		if (is_new) {
			status = redislite_insert_key(cs, keyname, keyname_len, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
			if (status < 0) {
				redislite_free(page->list);
				redislite_free(page);
				return status;
			}
		}
	}
	return status;
}

int redislite_llen_by_keyname(void *_db, void *_cs, char *keyname, int keyname_len, int *len)
{
	char type;
	int page_num = redislite_value_page_for_key(_db, _cs, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_ERR; // TODO: error for wrong type
	}

	redislite_page_list_first *page = redislite_page_get(_db, _cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	*len = page->total_size;
	if (_cs == NULL) {
		redislite_free_list_first(_db, page);
	}
	return REDISLITE_OK;
}

int redislite_lrange_by_keyname(void *_db, void *_cs, char *keyname, int keyname_len, int start, int end, int *list_count, char ***list, int **list_len)
{
	return 0;
}
