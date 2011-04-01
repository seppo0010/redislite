#include "redislite.h"
#include <string.h>

void redislite_write_data(void *_db, unsigned char *data, void *page)
{
	redislite *db = (redislite*)_db;
	data[0] = 'D';
	memset(&data[1], 0, db->page_size-1);
}

void *redislite_read_data(void *_db, unsigned char *data)
{
	return NULL;
}
