#include "redislite.h"
#include <string.h>
#include <stdlib.h>

void redislite_free_data(void *_db, void *page)
{
	free(page);
}

void redislite_write_data(void *_db, unsigned char *data, void *page)
{
	redislite *db = (redislite*)_db;
	data[0] = 'D';
	memcpy(&data[1], page, db->page_size-1);
}

void *redislite_read_data(void *_db, unsigned char *data)
{
	return NULL;
}
