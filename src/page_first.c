#include "core.h"
#include "page_first.h"
#include "page_index.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


redislite_page_index_first *create_page_index_first(void *db)
{
	redislite_page_index_first *first = malloc(sizeof(redislite_page_index_first));
	if (first == NULL) {
		return NULL;
	}
	first->number_of_keys = 0;
	redislite_page_index *page = (redislite_page_index *)redislite_page_index_create(db);
	if (page == NULL) {
		return NULL;
	}
	page->free_space -= 4;
	first->page = page;
	return first;
}

void redislite_free_first(void *_db, void *_page)
{
	redislite_page_index_first *page = (redislite_page_index_first *)_page;
	redislite_free_index(_db, page->page);
	redislite_free(page);
}

void redislite_write_first(void *_db, unsigned char *data, void *_page)
{
	redislite *db = (redislite *)_db;
	redislite_page_index_first *page = (redislite_page_index_first *)_page;
	redislite_put_4bytes(&data[0], page->number_of_keys);
	redislite_write_index(db, &data[4], page->page);
}

void *redislite_read_first(void *_db, unsigned char *data)
{
	redislite *db = (redislite *)_db;
	redislite_page_index_first *page = malloc(sizeof(redislite_page_index_first));
	if (page == NULL) {
		return NULL;
	}
	page->number_of_keys = redislite_get_4bytes(&data[0]);
	page->page = (redislite_page_index *)redislite_read_index(db, &data[4]);
	return page;
}
