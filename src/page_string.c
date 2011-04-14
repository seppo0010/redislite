#include "redislite.h"
#include "page_string.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>


void redislite_free_string(void *_db, void *_page)
{
	redislite_page_string* page = (redislite_page_string*)_page;
	free(page->value);
	free(page);
}

void redislite_write_string(void *_db, unsigned char *data, void *_page)
{
	redislite *db = (redislite*)_db;
	redislite_page_string* page = (redislite_page_string*)_page;

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
	redislite_page_string* page = malloc(sizeof(redislite_page_string));

	page->size = redislite_get_4bytes(&data[5]);
	page->right_page = redislite_get_4bytes(&data[9]);

	int size = db->page_size-13;
	if (size > page->size) size = page->size;
	page->value = malloc(sizeof(char) * size);
	memcpy(page->value, &data[13], size);
	return page;
}


void redislite_free_string_overflow(void *_db, void *_page)
{
	redislite_page_string_overflow* page = (redislite_page_string_overflow*)_page;
	free(page->value);
	free(page);
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
	redislite_page_string_overflow* page = malloc(sizeof(redislite_page_string_overflow));

	page->right_page = redislite_get_4bytes(&data[5]);

	int size = db->page_size-9;
	page->value = malloc(sizeof(char) * size);
	memcpy(page->value, &data[9], size);
	return page;
}

int redislite_insert_string(void *_cs, char *str, int length, int* num)
{
	changeset *cs = (changeset*)_cs;
	redislite *db = cs->db;
	if (db->readonly) return -1;
	redislite_page_string* page = malloc(sizeof(redislite_page_string));
	if (page == NULL) return REDISLITE_OOM;
	page->size = length;
	int first_page_size = db->page_size - 13;
	int total_pages;
	if (first_page_size < length) {
		total_pages = (int)ceil((float)(length-first_page_size)/(db->page_size - 9));
		int i, size, next_page=0,left=0;
		for (i=total_pages; i>=1;i--) {
			redislite_page_string_overflow* overflow_page = malloc(sizeof(redislite_page_string_overflow));
			if (overflow_page == NULL){ free(page); return REDISLITE_OOM; }
			char *data = malloc(sizeof(char) * (db->page_size - 9));
			if (data == NULL) { free(page); free(overflow_page); return REDISLITE_OOM; }
			memset(data, 0, db->page_size - 9);
			if (i == total_pages) size = length - (db->page_size - 13) - (db->page_size - 9) * (total_pages-1);
			else size = db->page_size - 9;
			memcpy(data, &str[(db->page_size - 13) + (db->page_size - 9) * (i-1)], size);
			overflow_page->db = db;
			overflow_page->right_page = next_page;
			overflow_page->value = data;
			next_page = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING_OVERFLOW, overflow_page);
			if (page == 0) return REDISLITE_OOM;
		}
		page->right_page = next_page;
		char *data = malloc(sizeof(char) * db->page_size - 13);
		if (data == NULL) { free(page); return REDISLITE_OOM; }
		memcpy(data, str, db->page_size - 13);
		page->value = data;
		*num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING, page);
	} else {
		total_pages = 1;
		page->right_page = 0;
		char *data = malloc(sizeof(char) * length);
		if (data == NULL) { free(page); return REDISLITE_OOM; }
		memcpy(data, str, length);
		page->value = data;
		*num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING, page);
	}
}

char *redislite_page_string_get_by_keyname(void *_db, void *_cs, char *key_name, int key_length, int* length) {
	redislite *db = (redislite*)_db;
	char type;
	void *_page = redislite_page_get_by_keyname(_db, _cs, key_name, key_length, &type);
	if (type != REDISLITE_PAGE_TYPE_STRING) return NULL;
	redislite_page_string* page = (redislite_page_string*)_page;
	char *data = malloc(sizeof(char) * page->size);
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
	return data;
cancel:
	if (_cs == NULL) redislite_free_string(db, page);
	free(data);
	return NULL;
}
