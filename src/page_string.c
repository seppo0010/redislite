#include "redislite.h"
#include "page_string.h"
#include "page_index.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>


void redislite_delete_string(void *_cs, void *_page)
{
	redislite_page_string* page = (redislite_page_string*)_page;
	if (page == NULL) return;
	if (page->right_page != 0) {
		redislite_page_delete(_cs, page->right_page);
	}
}

void redislite_free_string(void *_db, void *_page)
{
	redislite_page_string* page = (redislite_page_string*)_page;
	if (page == NULL) return;
	redislite_free(page->value);
	redislite_free(page);
}

void redislite_write_string(void *_db, unsigned char *data, void *_page)
{
	redislite *db = (redislite*)_db;
	redislite_page_string* page = (redislite_page_string*)_page;
	if (page == NULL) return;

	data[0] = REDISLITE_PAGE_TYPE_STRING;
	redislite_put_4bytes(&data[1], 0); // reserverd
	redislite_put_4bytes(&data[5], page->size);
	redislite_put_4bytes(&data[9], page->right_page);
	int size = db->page_size-13;
	if (size > page->size) size = page->size;
	memcpy(&data[13], page->value, size);
}

void *redislite_read_string(void *_db, unsigned char *data)
{
	redislite *db = (redislite*)_db;
	redislite_page_string* page = redislite_malloc(sizeof(redislite_page_string));

	page->size = redislite_get_4bytes(&data[5]);
	page->right_page = redislite_get_4bytes(&data[9]);

	int size = db->page_size-13;
	if (size > page->size) size = page->size;
	page->value = redislite_malloc(sizeof(char) * size);
	memcpy(page->value, &data[13], size);
	return page;
}


void redislite_delete_string_overflow(void *_db, void *_page)
{
	redislite_page_string_overflow* page = (redislite_page_string_overflow*)_page;
	if (page == NULL) return;
	if (page->right_page != 0) {
		redislite_page_delete(_db, page->right_page);
	}
}

void redislite_free_string_overflow(void *_db, void *_page)
{
	redislite_page_string_overflow* page = (redislite_page_string_overflow*)_page;
	redislite_free(page->value);
	redislite_free(page);
}

void redislite_write_string_overflow(void *_db, unsigned char *data, void *_page)
{
	redislite *db = (redislite*)_db;
	redislite_page_string_overflow* page = (redislite_page_string_overflow*)_page;

	data[0] = REDISLITE_PAGE_TYPE_STRING_OVERFLOW;
	redislite_put_4bytes(&data[1], 0); // reserved
	redislite_put_4bytes(&data[5], page->right_page);
	memcpy(&data[9], page->value, db->page_size-9);
}

void *redislite_read_string_overflow(void *_db, unsigned char *data)
{
	redislite *db = (redislite*)_db;
	redislite_page_string_overflow* page = redislite_malloc(sizeof(redislite_page_string_overflow));

	page->right_page = redislite_get_4bytes(&data[5]);

	int size = db->page_size-9;
	page->value = redislite_malloc(sizeof(char) * size);
	memcpy(page->value, &data[9], size);
	return page;
}

int redislite_insert_string(void *_cs, char *str, int length, int* num)
{
	changeset *cs = (changeset*)_cs;
	redislite *db = cs->db;
	if (db->readonly) return REDISLITE_READONLY;
	redislite_page_string* page = redislite_malloc(sizeof(redislite_page_string));
	if (page == NULL) return REDISLITE_OOM;
	page->size = length;
	int first_page_size = db->page_size - 13;
	if (first_page_size < length) {
		int total_pages = (int)ceil((float)(length-first_page_size)/(db->page_size - 9));
		int i, size, next_page=0;
		for (i=total_pages; i>=1;i--) {
			redislite_page_string_overflow* overflow_page = redislite_malloc(sizeof(redislite_page_string_overflow));
			if (overflow_page == NULL){ redislite_free(page); return REDISLITE_OOM; }
			char *data = redislite_malloc(sizeof(char) * (db->page_size - 9));
			if (data == NULL) { redislite_free(page); redislite_free(overflow_page); return REDISLITE_OOM; }
			memset(data, 0, db->page_size - 9);
			if (i == total_pages) size = length - (db->page_size - 13) - (db->page_size - 9) * (total_pages-1);
			else size = db->page_size - 9;
			memcpy(data, &str[(db->page_size - 13) + (db->page_size - 9) * (i-1)], size);
			overflow_page->db = db;
			overflow_page->right_page = next_page;
			overflow_page->value = data;
			next_page = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING_OVERFLOW, overflow_page);
			if (next_page < 0) {
				redislite_free(page);
				redislite_free(overflow_page);
				redislite_free(data);
				return next_page;
			}
			if (page == 0) return REDISLITE_OOM;
		}
		page->right_page = next_page;
		char *data = redislite_malloc(sizeof(char) * db->page_size - 13);
		if (data == NULL) { redislite_free(page); return REDISLITE_OOM; }
		memcpy(data, str, db->page_size - 13);
		page->value = data;
		*num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING, page);
		if (*num < 0) {
			redislite_free(page);
			redislite_free(data);
			return (*num);
		}
	} else {
		page->right_page = 0;
		char *data = redislite_malloc(sizeof(char) * length);
		if (data == NULL) { redislite_free(page); return REDISLITE_OOM; }
		memcpy(data, str, length);
		page->value = data;
		*num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING, page);
		if (*num < 0) return (*num);
	}
	return REDISLITE_OK;
}

int redislite_page_string_get_by_keyname(void *_db, void *_cs, char *key_name, int key_length, char **str, int* length) {
	redislite *db = (redislite*)_db;
	char type;
	void *_page = redislite_page_get_by_keyname(_db, _cs, key_name, key_length, &type);
	if (_page == NULL) return REDISLITE_OOM;
	if (type != REDISLITE_PAGE_TYPE_STRING) {
		redislite_page_type * page_type = redislite_page_get_type(db, type);
		page_type->free_function(db, _page);
		return REDISLITE_ERR;
	}
	redislite_page_string* page = (redislite_page_string*)_page;
	char *data = redislite_malloc(sizeof(char) * page->size);
	if (data == NULL) {
		if (_cs == NULL) redislite_free_string(db, page);
		return REDISLITE_OOM;
	}
	memcpy(data, page->value, MIN(page->size, db->page_size-13));

	int next = page->right_page;
	int i = 0;
	int pos, size;
	while (next != 0) {
		void *_extra = redislite_page_get(_db, _cs, next, &type);
		if (type != REDISLITE_PAGE_TYPE_STRING_OVERFLOW) goto cancel;
		redislite_page_string_overflow* extra = (redislite_page_string_overflow*)_extra;
		pos = db->page_size - 13 + i++ * (db->page_size - 9);
		size = MIN(page->size - pos, db->page_size - 9);
		memcpy(&data[pos], extra->value, size);
		next = extra->right_page;
		if (_cs == NULL) redislite_free_string_overflow(db, extra);
	}

	*length = page->size;
	if (_cs == NULL) redislite_free_string(db, page);
	*str = data;
	return REDISLITE_OK;
cancel:
	if (_cs == NULL) redislite_free_string(db, page);
	redislite_free(data);
	return REDISLITE_ERR;
}


int redislite_page_string_set_key_string(void *_cs, char *key_name, int key_length, char *str, int length) {
	changeset *cs = (changeset*)_cs;
	int left;
	int status = redislite_insert_string(cs, str, length, &left);
	if (status != REDISLITE_OK) {
		return status;
	}

	status = redislite_insert_key(cs, key_name, key_length, left);
	if (status < 0) {
		return status;
	}

	return REDISLITE_OK;
}

int redislite_page_string_setnx_key_string(void *_cs, char *key_name, int key_length, char *str, int length) {
	changeset *cs = (changeset*)_cs;
	int exists = redislite_value_page_for_key(cs->db, cs, key_name, key_length);
	if (exists < 0) {
		return exists; // error
	} else if (exists == 0) {
		exists = redislite_page_string_set_key_string(_cs, key_name, key_length, str, length);
		if (exists == REDISLITE_OK) return 1;
		else return exists; // error
	} else {
		return 0; // key already exists
	}
}
