#include "redislite.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "page.h"
#include "page_index.h"


#define HEADER_STRING "Redislite format 1"
#define DEFAULT_PAGE_SIZE 512
#define DEFAULT_MODIFIED_PAGE_SIZE 4
#define WRITE_FORMAT_VERSION 1
#define READ_FORMAT_VERSION 1

static void redislite_add_modified_page(redislite *db, int page_number, redislite_page_type type, void *page_data)
{
	if (db->readonly) return; // TODO: error
	if (db->modified_pages == NULL || (db->modified_pages_length == 0 && db->modified_pages_free == 0)) {
		db->modified_pages = malloc(sizeof(redislite_page) * DEFAULT_MODIFIED_PAGE_SIZE);
		if (db->modified_pages == NULL) return; // TODO: OOM
		db->modified_pages_free = DEFAULT_MODIFIED_PAGE_SIZE;
		db->modified_pages_length = 0;
	} else if (db->modified_pages_free == 0) {
		void **modified_pages = realloc(db->modified_pages, sizeof(redislite_page) * db->modified_pages_length * 2);
		if (modified_pages == NULL) return; // TODO: OOM
		db->modified_pages = modified_pages;
		db->modified_pages_free = db->modified_pages_length;
	}
	redislite_page *page = (redislite_page *)malloc(sizeof(redislite_page *));
	page->type = type;
	page->number = page_number;
	page->data = page_data;
	db->modified_pages[db->modified_pages_length++] = page;
	db->modified_pages_free--;
	if (page_number > db->number_of_pages) db->number_of_pages = page_number;
}

static void redislite_set_root(redislite *db, redislite_page_index *page)
{
	db->root = page;
	redislite_add_modified_page(db, 0, redislite_page_type_first, page);
}

static void redislite_save_db(redislite *db)
{
	if (!db->file) {
		db->file = fopen(db->filename, "ab");
	}

	int i;
	for (i=0;i<db->modified_pages_length;++i) {
		redislite_page *page = db->modified_pages[i];
		switch (page->type) {
			case redislite_page_type_first:
			{
				unsigned char header[100];
				memcpy(header, HEADER_STRING, sizeof(HEADER_STRING));
				header[20] = (db->page_size>>8); // page size
				header[21] = (db->page_size); // page size
				header[22] = WRITE_FORMAT_VERSION; // write format version
				header[23] = READ_FORMAT_VERSION; // read format version
				redislite_put_4bytes(&header[24], 0); // TODO: implement me
				redislite_put_4bytes(&header[28], db->number_of_pages);
				redislite_put_4bytes(&header[32], db->first_freelist_page);
				redislite_put_4bytes(&header[36], db->number_of_freelist_pages);
				memset(&header[40], 0, 100-40); // reserved
				fwrite(header, 100, sizeof(unsigned char), db->file);
			}
		}
	}
	fclose(db->file);
	db->file = NULL;
}

redislite* redislite_create_database(const unsigned char *filename)
{
	int page_size = DEFAULT_PAGE_SIZE;

	redislite* db = malloc(sizeof(redislite));
	size_t size = strlen(filename) + 1;
	db->filename = malloc(size);
	memcpy(db->filename, filename, size);
	db->file = NULL;
	db->page_size = page_size;
	db->file_change_counter = 0;
	db->number_of_pages = 0;
	db->first_freelist_page = 0;
	db->number_of_freelist_pages = 0;
	db->modified_pages = NULL;
	db->modified_pages_length = 0;
	db->modified_pages_free = 0;
	db->readonly = 0;
	
	redislite_page_index* page = (redislite_page_index*)redislite_page_index_create(db);
	redislite_page_index_add_key(page, "test", 4);
	redislite_set_root(db, page);

	return db;
}

redislite* redislite_open_database(const unsigned char *filename) {
	FILE *fp = fopen(filename, "r");
	if (!fp) return redislite_create_database(filename);
	unsigned char header[100];
	fread(header, sizeof(unsigned char), 100, fp);
	redislite* db = NULL;
	if (memcmp(header, HEADER_STRING, sizeof(HEADER_STRING)) != 0) goto cleanup; // file exist, but not as a redislite db
	if (header[23] > READ_FORMAT_VERSION) goto cleanup; // newer format

	db = malloc(sizeof(redislite));
	size_t size = strlen(filename) + 1;
	db->filename = malloc(size);
	memcpy(db->filename, filename, size);
	db->file = NULL;
	db->page_size = header[21] + (header[20]<<8);
	db->readonly = (header[22] > WRITE_FORMAT_VERSION);
	db->number_of_pages = redislite_get_4bytes(&header[28]);
	db->first_freelist_page = redislite_get_4bytes(&header[32]);
	db->number_of_freelist_pages = redislite_get_4bytes(&header[36]);

cleanup:
	fclose(fp);
	return db;
}

void redislite_close_database(redislite *db) {
	redislite_save_db(db);
	if (db->filename) free(db->filename);
	free(db);
}
