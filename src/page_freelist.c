#include "redislite.h"
#include "page_string.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>


void redislite_free_freelist(void *_db, void *_page)
{
	redislite_page_string* page = (redislite_page_string*)_page;
	if (page == NULL) return;
	redislite_free(page);
}

void redislite_write_freelist(void *_db, unsigned char *data, void *_page)
{
	redislite *db = (redislite*)_db;
	redislite_page_string* page = (redislite_page_string*)_page;
	if (page == NULL) return;

	data[0] = REDISLITE_PAGE_TYPE_FREELIST;
	redislite_put_4bytes(&data[1], 0); // reserverd
	redislite_put_4bytes(&data[5], page->right_page);
	int size = db->page_size-9;
	memset(&data[9], 0, size);
}

void *redislite_read_freelist(void *_db, unsigned char *data)
{
	redislite *db = (redislite*)_db;
	redislite_page_string* page = redislite_malloc(sizeof(redislite_page_string));
	if (page == NULL) return NULL;

	page->right_page = redislite_get_4bytes(&data[5]);

	return page;
}
