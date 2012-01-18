#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"
#include "page_index.h"
#include "page_list.h"
#include "util.h"

static int rpush(redislite_page_list *list, char *value, size_t value_len);
static int lpush(redislite_page_list *list, char *value, size_t value_len);

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
	_db = _db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_page_list *page = (redislite_page_list *)_page;
	if (page == NULL) {
		return;
	}
	size_t i;
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
	_db = _db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_page_list *page = (redislite_page_list *)_page;
	if (page == NULL) {
		return;
	}

	redislite_put_4bytes(&data[0], 0); // reserved
	redislite_put_4bytes(&data[4], page->size);
	redislite_put_4bytes(&data[8], page->right_page);
	redislite_put_4bytes(&data[12], page->left_page);

	size_t pos = 16;
	size_t i;
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
	page->db = _db;
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
	page->element_len = redislite_malloc(sizeof(size_t) * page->size);
	if (page->element_len == NULL) {
		goto cleanup;
	}
	size_t i, pos = 16;
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
		for (;; i--) {
			if (page->element[i]) {
				redislite_free(page->element[i]);
			}
			if (i == 0) {
				break;
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

static int grow_list_to_size(redislite_page_list *list, size_t size)
{
	if (list->element_alloced > size) {
		// avoid shrinking... should we?
		return REDISLITE_OK;
	}

	if (list->element_alloced == 0) {
		void *element = redislite_malloc(sizeof(char *) * (size));
		if (element == NULL) {
			return REDISLITE_OOM;
		}
		size_t *element_len = redislite_malloc(sizeof(size_t) * (size));
		if (element_len == NULL) {
			redislite_free(element);
			return REDISLITE_OOM;
		}
		list->element = element;
		list->element_len = element_len;
	}
	else {
		void *element = redislite_realloc(list->element, sizeof(char *) * (size));
		if (element == NULL) {
			return REDISLITE_OOM;
		}
		size_t *element_len = redislite_realloc(list->element_len, sizeof(size_t) * (size));
		if (element_len == NULL) {
			return REDISLITE_OOM;    // TODO: rollback element realloc?
		}
		list->element = element;
		list->element_len = element_len;
	}
	list->element_alloced = size;
	return REDISLITE_OK;
}

static int grow_list(redislite_page_list *list)
{
	if (list->element_alloced == list->size) {
		if (list->element_alloced == 0) {
			void *element = redislite_malloc(sizeof(char *) * (list->size + 1));
			if (element == NULL) {
				return REDISLITE_OOM;
			}
			size_t *element_len = redislite_malloc(sizeof(size_t) * (list->size + 1));
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
			size_t *element_len = redislite_realloc(list->element_len, sizeof(size_t) * (list->size + 1));
			if (element_len == NULL) {
				return REDISLITE_OOM;    // TODO: rollback element realloc?
			}
			list->element = element;
			list->element_len = element_len;
		}
		list->element_alloced++;
	}
	return REDISLITE_OK;
}

static int replace_element(changeset *cs, redislite_page_list *list, size_t free_bytes, int page_num, size_t pos, char *value, size_t value_len)
{
	if (list->element_len[pos] + free_bytes > value_len) {
		redislite_page_list *new_list = redislite_malloc(sizeof(redislite_page_list));
		if (new_list == NULL) {
			return REDISLITE_OOM;
		}

		new_list->left_page = page_num;
		new_list->element_alloced = new_list->size = 0;
		new_list->element_len = 0;

		int status;
		size_t i;
		grow_list_to_size(new_list, list->size - pos + 1);
		for (i = 0; i < list->size - pos; i++) {
			new_list->element[i] = list->element[i + pos];
			new_list->element_len[i] = list->element_len[i + pos];
			list->element[i + pos] = NULL;
			list->element_len[i + pos] = 0;
		}
		new_list->size = list->size - pos;
		list->size = pos;

		char *element = redislite_malloc(sizeof(char) * value_len);
		if (element == NULL) {
			return REDISLITE_OOM;
		}
		redislite_free(new_list->element[0]);
		memcpy(element, value, value_len);
		new_list->element[0] = element;
		new_list->element_len[0] = value_len;

		int new_page_num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_LIST, new_list);
		if (new_page_num < 0) {
			return new_page_num;
		}

		new_list->right_page = list->right_page = new_page_num;
		if (new_list->right_page != page_num) {
			redislite_page_list *right_list = redislite_page_get(cs->db, cs, new_list->right_page, REDISLITE_PAGE_TYPE_LIST);
			right_list->left_page = new_page_num;
			status = redislite_add_modified_page(cs, new_list->right_page, REDISLITE_PAGE_TYPE_LIST, right_list);
			if (status < 0) {
				return status;
			}
		}

		if (status < 0) {
			return status;
		}
		return REDISLITE_OK;
	}
	char *element = redislite_malloc(sizeof(char) * value_len);
	if (element == NULL) {
		return REDISLITE_OOM;
	}
	memcpy(element, value, value_len);

	if (list->element[pos]) {
		redislite_free(list->element[pos]);
	}
	list->element[pos] = element;
	list->element_len[pos] = value_len;
	return REDISLITE_OK;
}

static int add_element(redislite_page_list *list, size_t pos, char *value, size_t value_len)
{
	char *element = redislite_malloc(sizeof(char) * value_len);
	if (element == NULL) {
		return REDISLITE_OOM;
	}
	memcpy(element, value, value_len);

	int status = grow_list(list);
	if (status != REDISLITE_OK) {
		return status;
	}

	size_t i;
	for (i = list->size; i > pos; i--) {
		list->element[i] = list->element[i - 1];
		list->element_len[i] = list->element_len[i - 1];
	}
	list->element[pos] = element;
	list->element_len[pos] = value_len;
	list->size++;
	return REDISLITE_OK;
}

static int rpush(redislite_page_list *list, char *value, size_t value_len)
{
	return add_element(list, list->size, value, value_len);
}

static int lpush(redislite_page_list *list, char *value, size_t value_len)
{
	return add_element(list, 0, value, value_len);
}

size_t redislite_free_bytes(void *_db, redislite_page_list *page, char type)
{
	redislite *db = (redislite *)_db;
	size_t i, pos = type == REDISLITE_PAGE_TYPE_LIST_FIRST ? 20 : 16;
	unsigned char placeholder[9];
	if (page->element_len) {
		for (i = 0; i < page->size; i++) {
			pos += putVarint32(placeholder, page->element_len[i]) + page->element_len[i];
		}
	}
	if (db->page_size < pos) {
		return 0;
	}
	return db->page_size - pos;
}

int redislite_rpushx_by_keyname(void *_cs, char *keyname, size_t keyname_len, char *value, size_t value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0 && page_num != REDISLITE_NOT_FOUND) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	if (page_num == 0) {
		return 0;
	}

	return redislite_rpush_page_num(_cs, &page_num, value, value_len);
}

int redislite_rpush_by_keyname(void *_cs, char *keyname, size_t keyname_len, char *value, size_t value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0 && page_num != REDISLITE_NOT_FOUND) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	int is_new = page_num == REDISLITE_NOT_FOUND;
	int status = redislite_rpush_page_num(_cs, &page_num, value, value_len);
	if (status == REDISLITE_OK) {
		if (is_new) {
			status = redislite_insert_key(cs, cs->db->root, 0, keyname, keyname_len, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
			if (status < 0) {
				return status;
			}
		}
	}
	return status;
}

int redislite_rpush_page_num(void *_cs, int *page_num_p, char *value, size_t value_len)
{
	changeset *cs = (changeset *)_cs;
	int status;
	char type = REDISLITE_PAGE_TYPE_LIST_FIRST;

	redislite_page_list_first *page = NULL;

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

	size_t size = value_len;
	{
		unsigned char length_str[9];
		size += putVarint32(length_str, value_len);
	}

	int enough_space;
	redislite_page_list *list;
	if (page->list->left_page != 0) {
		list = redislite_page_get(cs->db, _cs, page->list->left_page, REDISLITE_PAGE_TYPE_LIST);
		enough_space = redislite_free_bytes(cs->db, list, type) >= size;
	}
	else {
		list = page->list;
		enough_space = redislite_free_bytes(cs->db, page->list, REDISLITE_PAGE_TYPE_LIST_FIRST) >= size;
	}

	if (enough_space) {
		status = rpush(list, value, value_len);
		if (status != REDISLITE_OK) {
			return status;
		}
		if (page->list->left_page != 0) {
			redislite_add_modified_page(cs, page->list->left_page, REDISLITE_PAGE_TYPE_LIST, list);
		}
	}
	else {
		redislite_page_list *new_list = redislite_malloc(sizeof(redislite_page_list));
		if (new_list == NULL) {
			return REDISLITE_OOM;
		}

		new_list->right_page = 0;
		new_list->left_page = list == page->list ? *page_num_p : page->list->left_page;
		new_list->element_alloced = new_list->size = 0;
		new_list->element_len = 0;
		status = lpush(new_list, value, value_len);
		if (status != REDISLITE_OK) {
			redislite_free(new_list);
			return status;
		}

		status = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_LIST, new_list);
		if (status < 0) {
			return status;
		}
		list->right_page = status;
		if (list != page->list) {
			status = redislite_add_modified_page(cs, page->list->left_page, REDISLITE_PAGE_TYPE_LIST, list);
			if (status < 0) {
				return status;
			}
		}
		page->list->left_page = list->right_page;
		status = REDISLITE_OK;
	}

	{
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

int redislite_lpushx_by_keyname(void *_cs, char *keyname, size_t keyname_len, char *value, size_t value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0 && page_num != REDISLITE_NOT_FOUND) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	if (page_num == 0) {
		return 0;
	}

	return redislite_lpush_page_num(_cs, &page_num, value, value_len);
}

int redislite_lpush_by_keyname(void *_cs, char *keyname, size_t keyname_len, char *value, size_t value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0 && page_num != REDISLITE_NOT_FOUND) {
		return page_num;
	}
	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	int is_new = page_num == REDISLITE_NOT_FOUND;
	int status = redislite_lpush_page_num(_cs, &page_num, value, value_len);
	if (status == REDISLITE_OK) {
		if (is_new) {
			status = redislite_insert_key(cs, cs->db->root, 0, keyname, keyname_len, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
			if (status < 0) {
				return status;
			}
		}
	}
	return status;
}

static int make_room(changeset *cs, redislite_page_list *list, size_t size, int page_num)
{
	redislite_page_list *right = NULL;
	redislite_page_list *old_right = NULL;
	int status = REDISLITE_OK;
	while (redislite_free_bytes(cs->db, list, REDISLITE_PAGE_TYPE_LIST) < size) {
		if (list->size == 0) {
			printf("Popped all elements to make space -- value too big?\n");
			redislite_free(right);
			return REDISLITE_ERR;
		}
		if (right == NULL) {
			// We have no space on this page; trying to move things to the next
			// If that fails, we are gonna create a new one in between
			if (list->right_page != 0) {
				old_right = redislite_page_get(cs->db, cs, list->right_page, REDISLITE_PAGE_TYPE_LIST);
				size_t i = 0, tmp_size;
				unsigned char placeholder[9];
				while (list->size > 0) {
					tmp_size = list->element_len[list->size - 1] + putVarint32(placeholder, list->element_len[list->size - 1]);
					if (redislite_free_bytes(cs->db, old_right, REDISLITE_PAGE_TYPE_LIST) < tmp_size) {
						break;
					}
					if (redislite_free_bytes(cs->db, list, REDISLITE_PAGE_TYPE_LIST) >= size) {
						break;
					}
					status = lpush(old_right, list->element[list->size - 1], list->element_len[list->size - 1]);
					list->size--;
					i++;
				}
				if (redislite_free_bytes(cs->db, list, REDISLITE_PAGE_TYPE_LIST) >= size) {
					for (; i > 0; i--) {
						redislite_free(list->element[i + list->size - 1]);
					}
					redislite_add_modified_page(cs, list->right_page, REDISLITE_PAGE_TYPE_LIST, old_right);
					break;
				}
				else {
					list->size += i;
					old_right->size -= i;
				}
			}

			right = redislite_malloc(sizeof(redislite_page_list));
			if (right == NULL) {
				return REDISLITE_OOM;
			}
			right->left_page = page_num;
			right->right_page = list->right_page;
			right->element_alloced = right->size = 0;
			right->element_len = 0;
		}
		status = lpush(right, list->element[list->size - 1], list->element_len[list->size - 1]);
		if (status != REDISLITE_OK) {
			redislite_free(right);
			return status;
		}
		list->size--;
		redislite_free(list->element[list->size]);
	}
	if (right != NULL) {
		list->right_page = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_LIST, right);
		if (old_right != NULL) {
			old_right->left_page = list->right_page;
			redislite_add_modified_page(cs, right->right_page, REDISLITE_PAGE_TYPE_LIST, old_right);
		}
		else {
			list->left_page = list->right_page;
		}
	}
	return status;
}

int redislite_lpush_page_num(void *_cs, int *page_num_p, char *value, size_t value_len)
{
	changeset *cs = (changeset *)_cs;
	int status;
	char type = REDISLITE_PAGE_TYPE_LIST_FIRST;

	redislite_page_list_first *page = NULL;

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

	size_t size = value_len;
	{
		unsigned char length_str[9];
		size += putVarint32(length_str, value_len);
	}

	status = make_room(cs, page->list, size + 4, *page_num_p);
	if (status != REDISLITE_OK) {
		return status;
	}

	{
		status = lpush(page->list, value, value_len);
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

int redislite_rpop_by_keyname(void *_cs, char *keyname, size_t keyname_len, char **value, size_t *value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int status = REDISLITE_OK;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}

	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(cs->db, cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	if (page == NULL) {
		return REDISLITE_OOM;
	}

	redislite_page_list *list;
	if (page->list->left_page == 0) {
		list = page->list;
	}
	else {
		list = redislite_page_get(cs->db, cs, page->list->left_page, REDISLITE_PAGE_TYPE_LIST);
		if (list == NULL) {
			return REDISLITE_OOM;
		}
		while (list->size == 0) {
			list = redislite_page_get(cs->db, cs, list->left_page, REDISLITE_PAGE_TYPE_LIST);
		}
	}

	if (value) {
		*value = list->element[list->size - 1];
	}
	if (value_len) {
		*value_len = list->element_len[list->size - 1];
	}
	list->size--;
	page->total_size--;

	if (page->total_size == 0) {
		char *value_aux = redislite_malloc(sizeof(char) * (*value_len));
		if (value_aux == NULL) {
			return REDISLITE_OOM;
		}
		if (value) {
			memcpy(value_aux, *value, *value_len);
		}
		changeset *cs = (changeset *)_cs;
		redislite_delete_key(_cs, cs->db->root, keyname, keyname_len, 1);
		if (value) {
			redislite_free(*value);
		}
		if (value) {
			*value = value_aux;
		}
		// TODO: avoid double key lookup
	}
	else if (list->size == 0) {
		int old_page = page->list->left_page;
		page->list->left_page = list->left_page;
		list->right_page = 0;
		status = redislite_page_delete(cs, old_page, REDISLITE_PAGE_TYPE_LIST);
		if (status < 0) {
			return status;
		}
		if (page->list->left_page == page_num) {
			page->list->right_page =
			    page->list->left_page = 0;
			status = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
		}
		else {
			status = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
			if (status < 0) {
				// TODO: memory cleanup
				return status;
			}
		}
	}
	else {
		redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
		if (list != page->list) {
			redislite_add_modified_page(cs, page->list->left_page, REDISLITE_PAGE_TYPE_LIST, list);
		}
	}

	return status < 0 ? status : REDISLITE_OK;
}

int redislite_lpop_by_keyname(void *_cs, char *keyname, size_t keyname_len, char **value, size_t *value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	size_t i;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}

	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(cs->db, cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	if (page == NULL) {
		return REDISLITE_OOM;
	}

	redislite_page_list *list = page->list;
	int list_page_num = page_num;
	while (list->size == 0) {
		list_page_num = list->right_page;
		list = redislite_page_get(cs->db, cs, list->right_page, REDISLITE_PAGE_TYPE_LIST);
	}

	if (page->list->size == 0 && list_page_num == page->list->right_page && redislite_free_bytes(cs->db, list, REDISLITE_PAGE_TYPE_FIRST) > 0 && list->size > 0) {
		for (i = 0; i < list->size; i++) {
			lpush(page->list, list->element[list->size - i - 1], list->element_len[list->size - i - 1]);
		}
		page->list->right_page = list->right_page;
		list->right_page = list->left_page = 0;
		redislite_page_delete(_cs, list_page_num, REDISLITE_PAGE_TYPE_LIST);
		if (page->list->left_page == list_page_num) {
			page->list->left_page = 0;
		}
		list_page_num = page_num;
		list = page->list;
		// TODO: delete old page
	}

	if (value) {
		*value = list->element[0];
	}
	if (value_len) {
		*value_len = list->element_len[0];
	}
	for (i = 1; i < list->size; i++) {
		list->element[i - 1] = list->element[i];
		list->element_len[i - 1] = list->element_len[i];
	}
	list->size--;
	page->total_size--;

	if (page->total_size == 0) {
		char *value_aux;
		if (value_len) {
			value_aux = redislite_malloc(sizeof(char) * (*value_len));
			if (value_aux == NULL) {
				return REDISLITE_OOM;
			}
			if (value) {
				memcpy(value_aux, *value, *value_len);
			}
		}
		changeset *cs = (changeset *)_cs;
		redislite_delete_key(_cs, cs->db->root, keyname, keyname_len, 1);
		if (value) {
			redislite_free(*value);
		}
		if (value && value_len) {
			*value = value_aux;
		}
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

int redislite_llen_by_keyname(void *_db, void *_cs, char *keyname, size_t keyname_len, size_t *len)
{
	redislite *db = (redislite *)_db;
	char type;
	int page_num = redislite_value_page_for_key(_db, _cs, db->root, keyname, keyname_len, &type);
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

int redislite_lrange_by_keyname(void *_db, void *_cs, char *keyname, size_t keyname_len, int start, int end, size_t *ret_list_count_p, char ***ret_list_p, size_t **ret_list_len_p)
{
	redislite *db = (redislite *)_db;
	char type;
	int page_num = redislite_value_page_for_key(_db, _cs, db->root, keyname, keyname_len, &type);
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
	int len = (int)page->total_size;

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
		end = len - 1;
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
	size_t *ret_list_len = redislite_malloc(sizeof(size_t) * ret_list_count);
	if (ret_list_len == NULL) {
		redislite_free(ret_list);
		return REDISLITE_OOM;
	}

	redislite_page_list *list = page->list;
	int page_start = 0;
	int pos = 0;
	int i, right_page;
	while (page_start <= end) {
		if (page_start + (int)list->size > start) {
			for (i = 0; i < (int)list->size && pos < ret_list_count; i++) {
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
	for (;; pos--) {
		redislite_free(ret_list[i]);
		if (pos == 0) {
			break;
		}
	}
	redislite_free(ret_list);
	redislite_free(ret_list_len);
	return REDISLITE_OOM;
}

int redislite_lindex_by_keyname(void *_db, void *_cs, char *keyname, size_t keyname_len, int pos, char **value, size_t *value_len)
{
	// We are gonna keep constant time for pos=-1
	// pos=0 is already as fast as possible in lrange
	if (pos != -1) {
		size_t ret_list_count;
		size_t *ret_list_len;
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
	redislite *db = (redislite *)_db;
	char type;
	int page_num = redislite_value_page_for_key(_db, _cs, db->root, keyname, keyname_len, &type);
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

	redislite_page_list *list = NULL;
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

int redislite_lset_by_keyname(void *_cs, char *keyname, size_t keyname_len, int pos, char *value, size_t value_len)
{
	if (pos == 0) {
		redislite_lpop_by_keyname(_cs, keyname, keyname_len, NULL, NULL);
		return redislite_lpush_by_keyname(_cs, keyname, keyname_len, value, value_len);
	}
	else if (pos == -1) {
		redislite_rpop_by_keyname(_cs, keyname, keyname_len, NULL, NULL);
		return redislite_rpush_by_keyname(_cs, keyname, keyname_len, value, value_len);
	}
	changeset *cs = (changeset *)_cs;
	char type;
	int page_num = redislite_value_page_for_key(cs->db, _cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}

	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(cs->db, _cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	if (page == NULL) {
		return REDISLITE_NOT_FOUND;
	}

	int len = (int)page->total_size;
	if (pos >= len || pos < -len) {
		return REDISLITE_INDEX_OUT_OF_RANGE;
	}

	size_t seek_pos;

	if (pos < 0) {
		seek_pos = pos + len + 1;
	}
	else {
		seek_pos = pos;
	}

	redislite_page_list *list = page->list;
	size_t general_pos = 0;
	int status;
	int list_page_num = 0;
	while (1) {
		if (general_pos + list->size <= seek_pos) {
			general_pos += list->size;
			list_page_num = list->right_page;
			list = redislite_page_get(cs->db, _cs, list->right_page, REDISLITE_PAGE_TYPE_LIST);
			continue;
		}
		size_t free_bytes = redislite_free_bytes(cs->db, list, general_pos > 0 ? REDISLITE_PAGE_TYPE_LIST : REDISLITE_PAGE_TYPE_LIST_FIRST);
		status = replace_element(cs, list, free_bytes, list_page_num, seek_pos - general_pos, value, value_len);
		if (status == REDISLITE_OK) {
			if (general_pos > 0) {
				status = redislite_add_modified_page(cs, list_page_num, REDISLITE_PAGE_TYPE_LIST, list);
			}
			else {
				if (list->left_page == 0 && list->right_page != 0) {
					list->left_page = list->right_page;
				}
				status = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
			}
			if (status < 0) {
				return status;
			}
		}
		break;
	}

	if (status > 0) {
		status = REDISLITE_OK;
	}
	return status;
}

int redislite_linsert_by_keyname(void *_cs, char *keyname, size_t keyname_len, int after, char *pivot, size_t pivot_len, char *value, size_t value_len)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int page_num = redislite_value_page_for_key(cs->db, _cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num;
	}

	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(cs->db, _cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	if (page == NULL) {
		return REDISLITE_NOT_FOUND;
	}

	redislite_page_list *list = page->list;
	size_t i;
	int status = REDISLITE_NOT_FOUND;
	int list_page_num = 0;
	int inserted = 0;
	size_t size = value_len;
	{
		unsigned char length_str[9];
		size += putVarint32(length_str, value_len);
	}
	while (list != NULL) {
		for (i = 0; i < list->size; i++) {
			if (list->element_len[i] == pivot_len && memcmp(list->element[i], pivot, pivot_len) == 0) {
				while (redislite_free_bytes(cs->db, list, (list == page->list ? REDISLITE_PAGE_TYPE_LIST_FIRST : REDISLITE_PAGE_TYPE_LIST)) < size) {
					status = make_room(cs, list, size + (list == page->list ? 4 : 0), page_num);
					if (status < 0) {
						return status;
					}
					while (list->size < i) {
						i -= list->size;
						list = redislite_page_get(cs->db, _cs, list->right_page, REDISLITE_PAGE_TYPE_LIST);
					}
				}
				status = add_element(list, (size_t)i + after, value, value_len);
				if (status < 0) {
					return status;
				}
				if (list_page_num > 0) {
					status = redislite_add_modified_page(cs, list_page_num, REDISLITE_PAGE_TYPE_LIST, list);
					if (status < 0) {
						return status;
					}
				}
				status = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
				if (status < 0) {
					return status;
				}
				inserted = 1;
				page->total_size++;
				list = NULL;
				break;
			}
		}
		if (list == NULL) {
			break;
		}
		list_page_num = list->right_page;
		if (list_page_num == 0) {
			break;
		}
		list = redislite_page_get(cs->db, _cs, list->right_page, REDISLITE_PAGE_TYPE_LIST);
	}

	if (status > 0) {
		status = REDISLITE_OK;
	}
	return status == REDISLITE_OK ? (inserted ? (int)page->total_size : REDISLITE_NOT_FOUND) : status;
}

int redislite_rpoplpush_by_keyname(void *_cs, char *source, size_t source_len, char *destination, size_t destination_len, char **value, size_t *value_len)
{
	int status = redislite_rpop_by_keyname(_cs, source, source_len, value, value_len);
	if (status < 0) {
		return status;
	}
	return redislite_lpush_by_keyname(_cs, destination, destination_len, *value, *value_len);
}

int redislite_ltrim_by_keyname(void *_cs, char *keyname, size_t keyname_len, int start, int end)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int status, page_num = redislite_value_page_for_key(cs->db, _cs, cs->db->root, keyname, keyname_len, &type);
	if (page_num < 0) {
		return page_num == REDISLITE_NOT_FOUND ? REDISLITE_OK : page_num;
	}

	if (page_num > 0 && type != REDISLITE_PAGE_TYPE_LIST_FIRST) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_list_first *page = redislite_page_get(cs->db, _cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST);
	if (page == NULL) {
		return REDISLITE_NOT_FOUND;
	}

	int len = (int)page->total_size;
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
		end = len - 1;
	}

	if (start > end || start >= len) {
		redislite_delete_key(_cs, cs->db->root, keyname, keyname_len, 1);
		return REDISLITE_OK;
	}
	redislite_page_list *list = page->list;
	size_t _end = end, _start = start;
	size_t list_orig_size, list_page_num, i, pos = 0;
	while (pos < _end && list != NULL) {
		list_orig_size = list->size;
		if (pos < _start && pos + list->size >= _start) {
			for (i = 0; i < _start - pos; i++) {
				redislite_free(list->element[i]);
			}
			for (i = _start - pos; i < list->size; i++) {
				list->element[i + pos - _start] = list->element[i];
				list->element_len[i + pos - _start] = list->element_len[i];
			}
			list->size -= _start - pos;
			if (list != page->list) {
				status = redislite_add_modified_page(cs, list_page_num, REDISLITE_PAGE_TYPE_LIST, list);
				if (status < 0) {
					return status;
				}
			}
		}
		if (pos + list->size > _end) {
			list->size = _end - MAX(pos, _start) + 1;
			if (list != page->list) {
				status = redislite_add_modified_page(cs, list_page_num, REDISLITE_PAGE_TYPE_LIST, list);
				if (status < 0) {
					return status;
				}
			}
		}
		pos += list_orig_size;
		list_page_num = list->right_page;
		if (pos >= _end) {
			if (list->right_page > 0) {
				redislite_page_delete(_cs, list->right_page, REDISLITE_PAGE_TYPE_LIST);
			}
		}
		else {
			list = redislite_page_get(cs->db, _cs, list->right_page, REDISLITE_PAGE_TYPE_LIST);
		}
	}
	page->total_size = end - start + 1;
	status = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_LIST_FIRST, page);
	if (status < 0) {
		return status;
	}
	return REDISLITE_OK;
}
