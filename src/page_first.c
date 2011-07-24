#include "core.h"
#include "page_index.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


void redislite_free_first(void *_db, void *page)
{
	_db = _db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	page = page; // XXX: avoid unused-parameter warning; we are implementing a prototype
}

void redislite_write_first(void *_db, unsigned char *data, void *page)
{
	page = page; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite *db = (redislite *)_db;
	memcpy(data, HEADER_STRING, sizeof(HEADER_STRING));
	data[20] = (db->page_size >> 8); // page size
	data[21] = (db->page_size); // page size
	data[22] = WRITE_FORMAT_VERSION; // write format version
	data[23] = READ_FORMAT_VERSION; // read format version
	redislite_put_4bytes(&data[24], 0); // reserved
	redislite_put_4bytes(&data[28], db->number_of_pages);
	redislite_put_4bytes(&data[32], db->first_freelist_page);
	redislite_put_4bytes(&data[36], db->number_of_freelist_pages);
	redislite_put_4bytes(&data[40], db->number_of_keys);
	redislite_write_index(_db, &data[100], db->root);
	memset(&data[44], 0, 100 - 44); // reserved
}

void *redislite_read_first(void *_db, unsigned char *data)
{
	redislite *db = (redislite *)_db;
	db->file = NULL;
	db->page_size = data[21] + (data[20] << 8);
	db->readonly = (data[22] > WRITE_FORMAT_VERSION);
	db->number_of_pages = redislite_get_4bytes(&data[28]);
	db->first_freelist_page = redislite_get_4bytes(&data[32]);
	db->number_of_freelist_pages = redislite_get_4bytes(&data[36]);
	db->number_of_keys = redislite_get_4bytes(&data[40]);

	db->root = (redislite_page_index *)redislite_read_index(db, &data[100]);
	return db->root;
}
