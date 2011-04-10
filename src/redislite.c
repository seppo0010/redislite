#include "redislite.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "page.h"
#include "page_index.h"
#include "page_first.h"
#include "page_string.h"
#include "util.h"


changeset *redislite_create_changeset(redislite *db)
{
	changeset *cs = malloc(sizeof(changeset));
	cs->db = db;
	cs->modified_pages_length = 0;
	cs->modified_pages_free = 0;
	cs->modified_pages = NULL;
	return cs;
}

void redislite_free_changeset(changeset *cs)
{
	int i;
	for (i=0;i<cs->modified_pages_length;i++) {
		redislite_page *page = cs->modified_pages[i];
		page->type->free_function(cs->db, page->data);
		free(page);
	}
	free(cs->modified_pages);
	free(cs);
}

int redislite_save_changeset(changeset *cs)
{
	FILE *file = fopen(cs->db->filename, "rb+");
	if (!file) {
		file = fopen(cs->db->filename, "wb+");
	}

	if (!file) {
		return REDISLITE_ERR;
	}

	int i;
	for (i=0;i<cs->modified_pages_length;++i) {
		redislite_page *page = cs->modified_pages[i];
		unsigned char *data = (unsigned char*)malloc(sizeof(unsigned char) * cs->db->page_size);

		memset(&data[0], '\0', cs->db->page_size); // TODO: we could allow garbage on unused bytes
		page->type->write_function(cs->db, &data[0], page->data);
		fseek(file, cs->db->page_size * page->number, SEEK_SET);
		fwrite(data, cs->db->page_size, sizeof(unsigned char), file);
		free(data);
	}
	fclose(file);
	return REDISLITE_OK;
}

int redislite_add_modified_page(changeset *cs, int page_number, char type, void *page_data)
{
	if (cs->db->readonly) return -1; // TODO: error

	int i;
	// TODO: binary search
	if (page_number != -1) {
		for (i=0; i<cs->modified_pages_length;i++) {
			if (((redislite_page*)cs->modified_pages[i])->number == page_number) {
				return page_number;
			}

			if (((redislite_page*)cs->modified_pages[i])->number > page_number) {
				break;
			}
		}
	}

	if (page_number == -1) page_number = cs->db->number_of_pages;

	if (cs->modified_pages == NULL || (cs->modified_pages_length == 0 && cs->modified_pages_free == 0)) {
		cs->modified_pages = malloc(sizeof(redislite_page) * DEFAULT_MODIFIED_PAGE_SIZE);
		if (cs->modified_pages == NULL) return; // TODO: OOM
		cs->modified_pages_free = DEFAULT_MODIFIED_PAGE_SIZE;
		cs->modified_pages_length = 0;
	} else if (cs->modified_pages_free == 0) {
		void **modified_pages = realloc(cs->modified_pages, sizeof(redislite_page) * cs->modified_pages_length * 2);
		if (modified_pages == NULL) return; // TODO: OOM
		cs->modified_pages = modified_pages;
		cs->modified_pages_free = cs->modified_pages_length;
	}
	redislite_page *page = (redislite_page *)malloc(sizeof(redislite_page));
	page->type = redislite_page_get_type(cs->db, type);
	page->number = page_number;
	page->data = page_data;
	int pos = 0;

	// TODO: binary search
	for (i=0; i<cs->modified_pages_length;i++) {
		if (((redislite_page*)cs->modified_pages[i])->number > page_number) {
			break;
		} else {
			pos = i+1;
		}
	}

	for (i = cs->modified_pages_length - 1; i >= pos; i--) {
		cs->modified_pages[i+1] = cs->modified_pages[i];
	}

	cs->modified_pages[pos] = page;
	cs->modified_pages_length++;
	cs->modified_pages_free--;
	if (page_number >= cs->db->number_of_pages) cs->db->number_of_pages = page_number + 1;
	return page_number;
}

static void redislite_set_root(redislite *db, redislite_page_index *page)
{
	db->root = page;
	page->free_space -= 100;
	changeset *cs = redislite_create_changeset(db);
	redislite_add_modified_page(cs, 0, REDISLITE_PAGE_TYPE_FIRST, page);
	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
}

static void init_db(redislite *db)
{
	{
		redislite_page_type* type = malloc(sizeof(redislite_page_type));
		type->identifier = REDISLITE_PAGE_TYPE_INDEX;
		type->write_function = &redislite_write_index;
		type->read_function = &redislite_read_index;
		type->free_function = &redislite_free_index;
		redislite_page_register_type(db, type);
	}
	{
		redislite_page_type* type = malloc(sizeof(redislite_page_type));
		type->identifier = REDISLITE_PAGE_TYPE_STRING;
		type->write_function = &redislite_write_string;
		type->read_function = &redislite_read_string;
		type->free_function = &redislite_free_string;
		redislite_page_register_type(db, type);
	}
	{
		redislite_page_type* type = malloc(sizeof(redislite_page_type));
		type->identifier = REDISLITE_PAGE_TYPE_STRING_OVERFLOW;
		type->write_function = &redislite_write_string_overflow;
		type->read_function = &redislite_read_string_overflow;
		type->free_function = &redislite_free_string_overflow;
		redislite_page_register_type(db, type);
	}
	{
		redislite_page_type* type = malloc(sizeof(redislite_page_type));
		type->identifier = REDISLITE_PAGE_TYPE_FIRST;
		type->write_function = &redislite_write_first;
		type->read_function = &redislite_read_first;
		type->free_function = &redislite_free_first;
		redislite_page_register_type(db, type);
	}
}

redislite* redislite_open_database(const unsigned char *filename) {
	FILE *fp = fopen(filename, "r");
	if (!fp) return redislite_create_database(filename);
	unsigned char header[DEFAULT_PAGE_SIZE]; // TODO: read 100 header and then the rest
	fread(header, sizeof(unsigned char), DEFAULT_PAGE_SIZE, fp);
	redislite* db = NULL;
	if (memcmp(header, HEADER_STRING, sizeof(HEADER_STRING)) != 0) goto cleanup; // file exist, but not as a redislite db
	if (header[23] > READ_FORMAT_VERSION) goto cleanup; // newer format

	db = malloc(sizeof(redislite));
	db->types = NULL;
	init_db(db);
	size_t size = strlen(filename) + 1;
	db->filename = malloc(size);
	memcpy(db->filename, filename, size);

cleanup:
	fclose(fp);
	return db;
}

redislite* redislite_create_database(const unsigned char *filename)
{
	int page_size = DEFAULT_PAGE_SIZE;

	redislite* db = malloc(sizeof(redislite));
	if (db == NULL) return;
	db->types = NULL;
	init_db(db);
	
	size_t size = strlen(filename) + 1;
	db->filename = malloc(size);
	memcpy(db->filename, filename, size);
	db->file = NULL;
	db->page_size = page_size;
	db->file_change_counter = 0;
	db->number_of_pages = 0;
	db->first_freelist_page = 0;
	db->number_of_freelist_pages = 0;
	db->readonly = 0;

	redislite_page_index* page = (redislite_page_index*)redislite_page_index_create(db);
	redislite_set_root(db, page);

	return db;
}

unsigned char *redislite_read_page(redislite *db, changeset *cs, int num)
{
	int i;
	unsigned char *data = malloc(sizeof(unsigned char) * db->page_size);
	if (data == NULL) return NULL;
	// TODO: binary search
	if (cs) {
		for (i=0; i < cs->modified_pages_length; i++) {
			redislite_page *page = cs->modified_pages[i];
			if (page->number == num) {
				redislite_write_index(db, &data[0], page->data);
				return data;
			}
		}
	}

	if (!db->file) {
		db->file = fopen(db->filename, "rb+");
	}
	fseek(db->file, (long)db->page_size * num, SEEK_SET);
	fread(data, sizeof(unsigned char), db->page_size, db->file);
	return data;
}

void redislite_close_database(redislite *db) {
	if (db->filename) free(db->filename);
	redislite_free_index(db, db->root);
	int i;
	if (db->types) {
		for (i=0;i<256;i++)
			if (db->types[i])
				free(db->types[i]);
		free(db->types);
	}
	free(db);
}
