#include "redislite.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "page.h"
#include "page_index.h"
#include "util.h"


#define HEADER_STRING "Redislite format 1"
#define DEFAULT_PAGE_SIZE 512
#define DEFAULT_MODIFIED_PAGE_SIZE 4
#define WRITE_FORMAT_VERSION 1
#define READ_FORMAT_VERSION 1

static void redislite_add_modified_page(redislite *db, int page_number, redislite_page_type type, void *page_data)
{
	if (db->readonly) return; // TODO: error
	if (page_number == -1) page_number = db->number_of_pages;

	if (db->modified_pages != NULL) {
		int i;
		redislite_page *page;
		for (i=0; i < db->modified_pages_length; i++) {
			page = db->modified_pages[i];
				if (page->number == page_number) return; // TODO: is this ok?
		}
	}

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
	if (page_number >= db->number_of_pages) db->number_of_pages = page_number + 1;
}

static void redislite_set_root(redislite *db, redislite_page_index *page)
{
	db->root = page;
	redislite_add_modified_page(db, 0, redislite_page_type_first, page);
}

static void redislite_write_index(unsigned char *data, redislite_page_index *page)
{
	redislite_put_4bytes(&data[0], page->free_space);
	redislite_put_2bytes(&data[4], page->number_of_keys);
	redislite_put_4bytes(&data[6], page->right_page);
	int i;
	int pos = 10;
	for (i=0; i < page->number_of_keys; i++) {
		redislite_page_index_key* key = page->keys[i];
		pos += putVarint32(&data[pos], key->keyname_size);
		memcpy(&data[pos], key->keyname, key->keyname_size);
		pos += key->keyname_size;
		redislite_put_4bytes(&data[pos], key->left_page);
		pos += 4;
	}
}

static void redislite_save_db(redislite *db)
{
	if (!db->file) {
		db->file = fopen(db->filename, "ab");
	}

	int i;
	for (i=0;i<db->modified_pages_length;++i) {
		redislite_page *page = db->modified_pages[i];
		unsigned char *data = (unsigned char*)malloc(sizeof(unsigned char) * db->page_size);

		switch (page->type) {
			case redislite_page_type_first:
			{
				memcpy(data, HEADER_STRING, sizeof(HEADER_STRING));
				data[20] = (db->page_size>>8); // page size
				data[21] = (db->page_size); // page size
				data[22] = WRITE_FORMAT_VERSION; // write format version
				data[23] = READ_FORMAT_VERSION; // read format version
				redislite_put_4bytes(&data[24], 0); // TODO: implement me
				redislite_put_4bytes(&data[28], db->number_of_pages);
				redislite_put_4bytes(&data[32], db->first_freelist_page);
				redislite_put_4bytes(&data[36], db->number_of_freelist_pages);
				memset(&data[40], 0, 100-40); // reserved
				redislite_write_index(&data[100], page->data);
				break;
			}

			case redislite_page_type_index:
			{
				redislite_write_index(&data[0], page->data);
				break;
			}

			default:
			{
				break;
			}
		}
		fseek(db->file, db->page_size * page->number, SEEK_SET);
		fwrite(data, db->page_size, sizeof(unsigned char), db->file);
		free(data);
	}
	fclose(db->file);
	db->file = NULL;
}

static void test_add_key(redislite *db, redislite_page_index *page)
{
	int rnd = arc4random();
	char key[14];
	sprintf(key, "%d", rnd);
	int size = strlen(key);

	int pos = 0;
	int i;
	int cmp_result;
	for (i=0; i < page->number_of_keys; i++) {
		cmp_result = (memcmp(page->keys[i]->keyname, key, MIN(page->keys[i]->keyname_size, size)));
		if (cmp_result == 0) {
			cmp_result = (page->keys[i]->keyname_size > size ? 1 : -1);
			/* assert != size */
		}
		if (cmp_result < 0) {
			pos = i+1;
		} else {
			break;
		}
	}

	if (REDISLITE_ERR == redislite_page_index_add_key(page, pos, 0, key, size)) {
return;
		page = redislite_page_index_create(db);
		if (REDISLITE_ERR == redislite_page_index_add_key(page, 0, 0, key, size)) {
			printf("Ouh, key too big\n");
			exit(1);
		}
		redislite_add_modified_page(db, -1, redislite_page_type_index, page);
	}
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
	redislite_set_root(db, page);

	int i;
	for (i=0;i<5;i++)
		test_add_key(db, page);

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
	db->modified_pages = NULL;
	db->modified_pages_length = 0;
	db->modified_pages_free = 0;

cleanup:
	fclose(fp);
	return db;
}

void redislite_close_database(redislite *db) {
	redislite_save_db(db);
	if (db->filename) free(db->filename);
	free(db);
}
