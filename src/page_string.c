#include "redislite.h"
#include "page_string.h"
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
	redislite_put_4bytes(&data[1], page->size);
	redislite_put_4bytes(&data[5], page->right_page);
	int size = db->page_size-1-4-4;
	if (size > page->size) size = page->size;
	memcpy(&data[9], page->value, size);
}

void *redislite_read_string(void *_db, unsigned char *data)
{
	return NULL;
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

	data[0] = REDISLITE_PAGE_TYPE_STRING;
	redislite_put_4bytes(&data[1], page->right_page);
	memcpy(&data[9], page->value, db->page_size-1-4);
}

void *redislite_read_string_overflow(void *_db, unsigned char *data)
{
	return NULL;
}

int redislite_insert_string(void *_db, char *str, int length, int* num)
{
	redislite *db = (redislite*)_db;
	redislite_page_string* page = malloc(sizeof(redislite_page_string));
	if (page == NULL) return REDISLITE_OOM;
	page->size = length;
	int first_page_size = db->page_size - 1 - 4 - 4;
	int total_pages;
	if (first_page_size < length) {
		total_pages = (int)ceil((float)(length-first_page_size)/(db->page_size - 1 - 4));
		int i, size, next_page=0,left=0;
		for (i=total_pages-1; i>=1;i--) {
			redislite_page_string_overflow* overflow_page = malloc(sizeof(redislite_page_string_overflow));
			if (overflow_page == NULL){ free(page); return REDISLITE_OOM; }
			char *data = malloc(sizeof(char) * (db->page_size - 1 - 4));
			if (data == NULL) { free(page); free(overflow_page); return REDISLITE_OOM; }
			if (i == total_pages - 1) size = length - db->page_size -1 - 4 - 4 - (db->page_size - 1 - 4) * (total_pages - 1);
			else size = db->page_size - 1 - 4;
			memcpy(data, &str[(db->page_size - 1 - 4) * i], size);
			overflow_page->db = db;
			overflow_page->right_page = next_page;
			overflow_page->value = data;
			next_page = redislite_add_modified_page(db, -1, REDISLITE_PAGE_TYPE_STRING_OVERFLOW, data);
			if (page == 0) return REDISLITE_OOM;
		}
		page->right_page = next_page;
		char *data = malloc(sizeof(char) * (length - (total_pages*db->page_size - 1 - 4)));
		if (data == NULL) { free(page); return REDISLITE_OOM; }
		memcpy(data, str, db->page_size - 1 - 4 - 4);
		page->value = data;
		*num = redislite_add_modified_page(db, -1, REDISLITE_PAGE_TYPE_STRING, page);
	} else {
		total_pages = 1;
		page->right_page = 0;
		char *data = malloc(sizeof(char) * length);
		if (data == NULL) { free(page); return REDISLITE_OOM; }
		memcpy(data, str, length);
		page->value = data;
		*num = redislite_add_modified_page(db, -1, REDISLITE_PAGE_TYPE_STRING, page);
	}
}
