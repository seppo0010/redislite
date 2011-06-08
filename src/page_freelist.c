#include "redislite.h"
#include "page_string.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>


void redislite_free_freelist(void *_db, void *_page)
{
	redislite_page_string *page = (redislite_page_string *)_page;
	if (page == NULL) {
		return;
	}
	redislite_free(page);
}

void redislite_write_freelist(void *_db, unsigned char *data, void *_page)
{
	redislite *db = (redislite *)_db;
	redislite_page_string *page = (redislite_page_string *)_page;
	if (page == NULL) {
		return;
	}

	redislite_put_4bytes(&data[0], 0); // reserverd
	redislite_put_4bytes(&data[4], page->right_page);
	int size = db->page_size - 8;
	memset(&data[8], 0, size);
}

void *redislite_read_freelist(void *_db, unsigned char *data)
{
	redislite_page_string *page = redislite_malloc(sizeof(redislite_page_string));
	if (page == NULL) {
		return NULL;
	}

	page->right_page = redislite_get_4bytes(&data[5]);

	return page;
}
