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
	changeset *cs = redislite_malloc(sizeof(changeset));
	if (cs == NULL) return NULL;
	cs->db = db;
	cs->modified_pages_length = 0;
	cs->modified_pages_free = 0;
	cs->modified_pages = NULL;
	cs->opened_pages_length = 0;
	cs->opened_pages_free = 0;
	cs->opened_pages = NULL;
	return cs;
}

static int redislite_is_modified_page(changeset *cs, int page_number) {
	int i;
	for (i=0; i<cs->modified_pages_length;i++) {
		if (((redislite_page*)cs->modified_pages[i])->number == page_number) {
			return 1;
		}

		if (((redislite_page*)cs->modified_pages[i])->number > page_number) {
			break;
		}
	}
	return 0;
}

void redislite_free_changeset(changeset *cs)
{
	int i;
	for (i=0;i<cs->opened_pages_length;i++) {
		redislite_page *page = cs->opened_pages[i];
		if (redislite_is_modified_page(cs, page->number)) continue;
		page->type->free_function(cs->db, page->data);
		redislite_free(page);
	}
	redislite_free(cs->opened_pages);
	for (i=0;i<cs->modified_pages_length;i++) {
		redislite_page *page = cs->modified_pages[i];
		page->type->free_function(cs->db, page->data);
		redislite_free(page);
	}
	redislite_free(cs->modified_pages);
	redislite_free(cs);
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
		unsigned char *data = (unsigned char*)redislite_malloc(sizeof(unsigned char) * cs->db->page_size);

		memset(&data[0], '\0', cs->db->page_size); // TODO: we could allow garbage on unused bytes
		page->type->write_function(cs->db, &data[0], page->data);
		fseek(file, cs->db->page_size * page->number, SEEK_SET);
		fwrite(data, cs->db->page_size, sizeof(unsigned char), file);
		redislite_free(data);
	}
	fclose(file);
	return REDISLITE_OK;
}

int redislite_add_opened_page(changeset *cs, int page_number, char type, void *page_data)
{
	int i;
	// TODO: binary search
	if (page_number != -1) {
		for (i=0; i<cs->opened_pages_length;i++) {
			if (((redislite_page*)cs->opened_pages[i])->number == page_number) {
				return page_number;
			}

			if (((redislite_page*)cs->opened_pages[i])->number > page_number) {
				break;
			}
		}
	}

	if (cs->opened_pages == NULL || (cs->opened_pages_length == 0 && cs->opened_pages_free == 0)) {
		cs->opened_pages = redislite_malloc(sizeof(redislite_page) * DEFAULT_MODIFIED_PAGE_SIZE);
		if (cs->opened_pages == NULL) return; // TODO: OOM
		cs->opened_pages_free = DEFAULT_OPENED_PAGE_SIZE;
		cs->opened_pages_length = 0;
	} else if (cs->opened_pages_free == 0) {
		void **opened_pages = redislite_realloc(cs->opened_pages, sizeof(redislite_page) * cs->opened_pages_length * 2);
		if (opened_pages == NULL) return; // TODO: OOM
		cs->opened_pages = opened_pages;
		cs->opened_pages_free = cs->opened_pages_length;
	}
	redislite_page *page = (redislite_page *)redislite_malloc(sizeof(redislite_page));
	page->type = redislite_page_get_type(cs->db, type);
	page->number = page_number;
	page->data = page_data;
	int pos = 0;

	// TODO: binary search
	for (i=0; i<cs->opened_pages_length;i++) {
		if (((redislite_page*)cs->opened_pages[i])->number > page_number) {
			break;
		} else {
			pos = i+1;
		}
	}

	for (i = cs->opened_pages_length - 1; i >= pos; i--) {
		cs->opened_pages[i+1] = cs->opened_pages[i];
	}

	cs->opened_pages[pos] = page;
	cs->opened_pages_length++;
	cs->opened_pages_free--;

	return page_number;
}

int redislite_add_modified_page(changeset *cs, int page_number, char type, void *page_data)
{
	if (cs->db->readonly) return REDISLITE_READONLY;

	int i;
	// TODO: binary search
	if (page_number != -1) {
		if (redislite_is_modified_page(cs, page_number)) return page_number;
	}

	if (page_number == -1) page_number = cs->db->number_of_pages;

	if (cs->modified_pages == NULL || (cs->modified_pages_length == 0 && cs->modified_pages_free == 0)) {
		cs->modified_pages = redislite_malloc(sizeof(redislite_page) * DEFAULT_MODIFIED_PAGE_SIZE);
		if (cs->modified_pages == NULL) return REDISLITE_OOM; // TODO: OOM
		cs->modified_pages_free = DEFAULT_MODIFIED_PAGE_SIZE;
		cs->modified_pages_length = 0;
	} else if (cs->modified_pages_free == 0) {
		void **modified_pages = redislite_realloc(cs->modified_pages, sizeof(redislite_page) * cs->modified_pages_length * 2);
		if (modified_pages == NULL) return REDISLITE_OOM; // TODO: OOM
		cs->modified_pages = modified_pages;
		cs->modified_pages_free = cs->modified_pages_length;
	}
	redislite_page *page = (redislite_page *)redislite_malloc(sizeof(redislite_page));
	if (page == NULL) return REDISLITE_OOM;
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

static int init_db(redislite *db)
{
	{
		redislite_page_type* type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) { redislite_close_database(db); return REDISLITE_OOM; }
		type->identifier = REDISLITE_PAGE_TYPE_INDEX;
		type->write_function = &redislite_write_index;
		type->read_function = &redislite_read_index;
		type->free_function = &redislite_free_index;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) { free(type); return status; }
	}
	{
		redislite_page_type* type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) { redislite_close_database(db); return REDISLITE_OOM; }
		type->identifier = REDISLITE_PAGE_TYPE_STRING;
		type->write_function = &redislite_write_string;
		type->read_function = &redislite_read_string;
		type->free_function = &redislite_free_string;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) { free(type); return status; }
	}
	{
		redislite_page_type* type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) { redislite_close_database(db); return REDISLITE_OOM; }
		type->identifier = REDISLITE_PAGE_TYPE_STRING_OVERFLOW;
		type->write_function = &redislite_write_string_overflow;
		type->read_function = &redislite_read_string_overflow;
		type->free_function = &redislite_free_string_overflow;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) { free(type); return status; }
	}
	{
		redislite_page_type* type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) { redislite_close_database(db); return REDISLITE_OOM; }
		type->identifier = REDISLITE_PAGE_TYPE_FIRST;
		type->write_function = &redislite_write_first;
		type->read_function = &redislite_read_first;
		type->free_function = &redislite_free_first;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) { free(type); return status; }
	}
	return REDISLITE_OK;
}

redislite* redislite_open_database(const unsigned char *filename) {
	FILE *fp = fopen(filename, "r");
	if (!fp) return redislite_create_database(filename);
	unsigned char header[DEFAULT_PAGE_SIZE]; // TODO: read 100 header and then the rest
	fread(header, sizeof(unsigned char), DEFAULT_PAGE_SIZE, fp);
	redislite* db = NULL;
	if (memcmp(header, HEADER_STRING, sizeof(HEADER_STRING)) != 0) goto cleanup; // file exist, but not as a redislite db
	if (header[23] > READ_FORMAT_VERSION) goto cleanup; // newer format

	db = redislite_malloc(sizeof(redislite));
	db->root = NULL;
	db->types = NULL;
	db->file = NULL;
	db->filename = NULL;
	int init = init_db(db);
	if (init != 0) goto cleanup;
	size_t size = strlen(filename) + 1;
	db->filename = redislite_malloc(size);
	memcpy(db->filename, filename, size);

cleanup:
	fclose(fp);
	return db;
}

redislite* redislite_create_database(const unsigned char *filename)
{
	int page_size = DEFAULT_PAGE_SIZE;

	redislite* db = redislite_malloc(sizeof(redislite));
	if (db == NULL) return;
	db->root = NULL;
	db->types = NULL;
	db->file = NULL;
	db->filename = NULL;
	int init = init_db(db);
	if (init != 0) return;
	
	size_t size = strlen(filename) + 1;
	db->filename = redislite_malloc(size);
	if (db->filename == NULL) { redislite_close_database(db); return NULL; }
	memcpy(db->filename, filename, size);
	db->file = NULL;
	db->page_size = page_size;
	db->file_change_counter = 0;
	db->number_of_pages = 0;
	db->first_freelist_page = 0;
	db->number_of_freelist_pages = 0;
	db->readonly = 0;

	redislite_page_index* page = (redislite_page_index*)redislite_page_index_create(db);
	if (page == NULL) { redislite_close_database(db); return; }
	redislite_set_root(db, page);

	return db;
}

unsigned char *redislite_read_page(redislite *db, changeset *cs, int num)
{
	int i;
	unsigned char *data = redislite_malloc(sizeof(unsigned char) * db->page_size);
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
	fseek(db->file, 0L, SEEK_END);
	int size = ftell(db->file);
	if (size < db->page_size * (num+1)) { redislite_free(data); return NULL; }
	fseek(db->file, (long)db->page_size * num, SEEK_SET);
	size_t read = fread(data, sizeof(unsigned char), db->page_size, db->file);
	if (read < db->page_size && ferror(db->file)) printf("Error reading\n");
	if (read < db->page_size && feof(db->file)) printf("Early EOF (seek to pos %ld, size was %d, attempt to read %d)\n", (long)db->page_size * num, size, db->page_size);
	if (read < db->page_size) { redislite_free(data); return NULL; }

	return data;
}

void redislite_close_database(redislite *db) {
	if (db->file) fclose(db->file);
	if (db->filename) redislite_free(db->filename);
	redislite_free_index(db, db->root);
	int i;
	if (db->types) {
		for (i=0;i<256;i++)
			if (db->types[i])
				redislite_free(db->types[i]);
		redislite_free(db->types);
	}
	redislite_free(db);
}
