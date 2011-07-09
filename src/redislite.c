#include "redislite.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "page.h"
#include "page_index.h"
#include "page_first.h"
#include "page_string.h"
#include "page_freelist.h"
#include "page_list.h"
#include "util.h"

changeset *redislite_create_changeset(redislite *db)
{
	changeset *cs = redislite_malloc(sizeof(changeset));
	if (cs == NULL) {
		return NULL;
	}
	cs->db = db;
	cs->modified_pages_length = 0;
	cs->modified_pages_free = 0;
	cs->modified_pages = NULL;
	cs->opened_pages_length = 0;
	cs->opened_pages_free = 0;
	cs->opened_pages = NULL;
	return cs;
}

redislite_page *redislite_modified_page(changeset *cs, int page_number)
{
	int i;
	for (i = 0; i < cs->modified_pages_length; i++) {
		if (((redislite_page *)cs->modified_pages[i])->number == page_number) {
			return ((redislite_page *)cs->modified_pages[i]);
		}

		if (((redislite_page *)cs->modified_pages[i])->number > page_number) {
			break;
		}
	}
	return NULL;
}

void redislite_free_changeset(changeset *cs)
{
	int i;
	redislite_page *page;
	for (i = 0; i < cs->opened_pages_length; i++) {
		page = cs->opened_pages[i];
		redislite_page *_page = redislite_modified_page(cs, page->number);
		if (_page != NULL && _page->data != page->data) {
			page->type->free_function(cs->db, page->data);
		}
		redislite_free(page);
	}
	redislite_free(cs->opened_pages);
	for (i = 0; i < cs->modified_pages_length; i++) {
		page = cs->modified_pages[i];
		if (page->data != cs->db->root) {
			page->type->free_function(cs->db, page->data);
		}
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
		fclose(file);
		return REDISLITE_ERR;
	}

	int i;
	unsigned char *data = (unsigned char *)redislite_malloc(sizeof(unsigned char) * cs->db->page_size);
	if (data == NULL) {
		fclose(file);
		return REDISLITE_OOM;
	}

	for (i = 0; i < cs->modified_pages_length; ++i) {
		redislite_page *page = cs->modified_pages[i];

		memset(&data[0], '\0', cs->db->page_size); // TODO: we could allow garbage on unused bytes
		page->type->write_function(cs->db, &data[0], page->data);
		fseek(file, cs->db->page_size * page->number, SEEK_SET);
		fwrite(data, cs->db->page_size, sizeof(unsigned char), file);
	}
	redislite_free(data);
	fclose(file);
	if (cs->db->file) {
		fclose(cs->db->file);
		cs->db->file = NULL;
	}
	return REDISLITE_OK;
}

int redislite_add_opened_page(changeset *cs, int page_number, char type, void *page_data)
{
	// TODO: verify binary search
	if (page_number != -1) {
		int min = 0, max = cs->opened_pages_length - 1, i;
		while (min < max) {
			i = (min + max) / 2;
			if (((redislite_page *)cs->opened_pages[i])->number > page_number) {
				max = i - 1;
			}
			else if (((redislite_page *)cs->opened_pages[i])->number < page_number) {
				min = i + 1;
			}
			else {
				return i;
			}
		}
	}

	if (cs->opened_pages == NULL || (cs->opened_pages_length == 0 && cs->opened_pages_free == 0)) {
		cs->opened_pages = redislite_malloc(sizeof(redislite_page) * DEFAULT_OPENED_PAGE_SIZE);
		if (cs->opened_pages == NULL) {
			return REDISLITE_OOM;
		}
		cs->opened_pages_free = DEFAULT_OPENED_PAGE_SIZE;
		cs->opened_pages_length = 0;
	}
	else if (cs->opened_pages_free == 0) {
		void **opened_pages = redislite_realloc(cs->opened_pages, sizeof(redislite_page) * cs->opened_pages_length * 2);
		if (opened_pages == NULL) {
			return REDISLITE_OOM;
		}
		cs->opened_pages = opened_pages;
		cs->opened_pages_free = cs->opened_pages_length;
	}
	redislite_page *page = (redislite_page *)redislite_malloc(sizeof(redislite_page));
	if (page == NULL) {
		return REDISLITE_OOM;
	}
	page->type = redislite_page_get_type(cs->db, type);
	page->number = page_number;
	page->data = page_data;
	int pos = -1;

	// TODO: verify binary search
	{
		int min = 0, max = cs->opened_pages_length - 1, i;
		while (min < max) {
			i = (min + max) / 2;
			if (((redislite_page *)cs->opened_pages[i])->number > page_number) {
				max = i - 1;
			}
			else if (((redislite_page *)cs->opened_pages[i])->number < page_number) {
				min = i + 1;
			}
			else {
				pos = min = max = i;
			}
		}
		pos = min;
	}

	int i;
	for (i = cs->opened_pages_length - 1; i >= pos; i--) {
		cs->opened_pages[i + 1] = cs->opened_pages[i];
	}

	cs->opened_pages[pos] = page;
	cs->opened_pages_length++;
	cs->opened_pages_free--;

	return page_number;
}

static int redislite_close_opened_page(changeset *cs, int page_number)
{
	int i;
	redislite_page *page;
	for (i = 0; i < cs->opened_pages_length; i++) {
		page = cs->opened_pages[i];
		if (page->number == page_number) {
			page->type->free_function(cs->db, page->data);
			redislite_free(page);
			for (; i < cs->opened_pages_length - 1; i++) {
				cs->opened_pages[i] = cs->opened_pages[i+1];
			}
			cs->opened_pages_length--;
			cs->opened_pages_free++;
			break;
		}
		if (page->number > page_number) break;
	}
	return REDISLITE_OK;
}

int redislite_add_modified_page(changeset *cs, int page_number, char type, void *page_data)
{
	if (cs->db->readonly) {
		return REDISLITE_READONLY;
	}

	int i;
	if (page_number != -1) {
		redislite_page *page = redislite_modified_page(cs, page_number);
		if (page) {
			if (page->data != page_data) {
				redislite_close_opened_page(cs, page_number);
				page->type = redislite_page_get_type(cs->db, type);
				page->data = page_data;
			}
			return page_number;
		}
	}

	if (page_number == -1) {
		if (cs->db->first_freelist_page) {
			page_number = cs->db->first_freelist_page;
			redislite_page_freelist *freelist_page = redislite_page_get(cs->db, cs, page_number, REDISLITE_PAGE_TYPE_FREELIST);
			if (freelist_page == NULL) {
				return REDISLITE_OOM;
			}
			cs->db->first_freelist_page = freelist_page->right_page;
		}
		else {
			page_number = cs->db->number_of_pages;
		}
	}

	if (cs->modified_pages == NULL || (cs->modified_pages_length == 0 && cs->modified_pages_free == 0)) {
		cs->modified_pages = redislite_malloc(sizeof(redislite_page) * DEFAULT_MODIFIED_PAGE_SIZE);
		if (cs->modified_pages == NULL) {
			return REDISLITE_OOM;    // TODO: OOM
		}
		cs->modified_pages_free = DEFAULT_MODIFIED_PAGE_SIZE;
		cs->modified_pages_length = 0;
	}
	else if (cs->modified_pages_free == 0) {
		void **modified_pages = redislite_realloc(cs->modified_pages, sizeof(redislite_page) * cs->modified_pages_length * 2);
		if (modified_pages == NULL) {
			return REDISLITE_OOM;    // TODO: OOM
		}
		cs->modified_pages = modified_pages;
		cs->modified_pages_free = cs->modified_pages_length;
	}
	redislite_page *page = (redislite_page *)redislite_malloc(sizeof(redislite_page));
	if (page == NULL) {
		return REDISLITE_OOM;
	}
	page->type = redislite_page_get_type(cs->db, type);
	page->number = page_number;
	page->data = page_data;
	int pos = 0;

	// TODO: binary search
	for (i = 0; i < cs->modified_pages_length; i++) {
		if (((redislite_page *)cs->modified_pages[i])->number > page_number) {
			break;
		}
		else {
			pos = i + 1;
		}
	}

	for (i = cs->modified_pages_length - 1; i >= pos; i--) {
		cs->modified_pages[i + 1] = cs->modified_pages[i];
	}

	cs->modified_pages[pos] = page;
	cs->modified_pages_length++;
	cs->modified_pages_free--;
	if (page_number >= cs->db->number_of_pages) {
		cs->db->number_of_pages = page_number + 1;
	}
	return page_number;
}

static int redislite_set_root(redislite *db, redislite_page_index *page)
{
	db->root = page;
	page->free_space -= 100;
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		return REDISLITE_OOM;
	}
	int ret = redislite_add_modified_page(cs, 0, REDISLITE_PAGE_TYPE_FIRST, page);
	if (ret != REDISLITE_OK) {
		redislite_free_changeset(cs);
		return ret;
	}
	ret = redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	return ret;
}

static int init_db(redislite *db)
{
	{
		redislite_page_type *type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) {
			redislite_close_database(db);
			return REDISLITE_OOM;
		}
		type->identifier = REDISLITE_PAGE_TYPE_INDEX;
		type->write_function = &redislite_write_index;
		type->read_function = &redislite_read_index;
		type->free_function = &redislite_free_index;
		type->delete_function = NULL;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) {
			free(type);
			return status;
		}
	}
	{
		redislite_page_type *type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) {
			redislite_close_database(db);
			return REDISLITE_OOM;
		}
		type->identifier = REDISLITE_PAGE_TYPE_STRING;
		type->write_function = &redislite_write_string;
		type->read_function = &redislite_read_string;
		type->free_function = &redislite_free_string;
		type->delete_function = &redislite_delete_string;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) {
			free(type);
			return status;
		}
	}
	{
		redislite_page_type *type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) {
			redislite_close_database(db);
			return REDISLITE_OOM;
		}
		type->identifier = REDISLITE_PAGE_TYPE_STRING_OVERFLOW;
		type->write_function = &redislite_write_string_overflow;
		type->read_function = &redislite_read_string_overflow;
		type->free_function = &redislite_free_string_overflow;
		type->delete_function = &redislite_delete_string_overflow;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) {
			free(type);
			return status;
		}
	}
	{
		redislite_page_type *type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) {
			redislite_close_database(db);
			return REDISLITE_OOM;
		}
		type->identifier = REDISLITE_PAGE_TYPE_FIRST;
		type->write_function = &redislite_write_first;
		type->read_function = &redislite_read_first;
		type->free_function = &redislite_free_first;
		type->delete_function = NULL;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) {
			free(type);
			return status;
		}
	}
	{
		redislite_page_type *type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) {
			redislite_close_database(db);
			return REDISLITE_OOM;
		}
		type->identifier = REDISLITE_PAGE_TYPE_FREELIST;
		type->write_function = &redislite_write_freelist;
		type->read_function = &redislite_read_freelist;
		type->free_function = &redislite_free_freelist;
		type->delete_function = NULL;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) {
			free(type);
			return status;
		}
	}
	{
		redislite_page_type *type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) {
			redislite_close_database(db);
			return REDISLITE_OOM;
		}
		type->identifier = REDISLITE_PAGE_TYPE_LIST;
		type->write_function = &redislite_write_list;
		type->read_function = &redislite_read_list;
		type->free_function = &redislite_free_list;
		type->delete_function = NULL;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) {
			free(type);
			return status;
		}
	}
	{
		redislite_page_type *type = redislite_malloc(sizeof(redislite_page_type));
		if (type == NULL) {
			redislite_close_database(db);
			return REDISLITE_OOM;
		}
		type->identifier = REDISLITE_PAGE_TYPE_LIST_FIRST;
		type->write_function = &redislite_write_list_first;
		type->read_function = &redislite_read_list_first;
		type->free_function = &redislite_free_list_first;
		type->delete_function = NULL;
		int status = redislite_page_register_type(db, type);
		if (status != REDISLITE_OK) {
			free(type);
			return status;
		}
	}
	return REDISLITE_OK;
}

redislite *redislite_open_database(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		return redislite_create_database(filename);
	}
	unsigned char header[DEFAULT_PAGE_SIZE]; // TODO: read 100 header and then the rest
	fread(header, sizeof(unsigned char), DEFAULT_PAGE_SIZE, fp);
	// TODO: check fread result
	redislite *db = NULL;
	if (memcmp(header, HEADER_STRING, sizeof(HEADER_STRING)) != 0) {
		goto cleanup;    // file exist, but not as a redislite db
	}
	if (header[23] > READ_FORMAT_VERSION) {
		goto cleanup;    // newer format
	}

	db = redislite_malloc(sizeof(redislite));
	if (db == NULL) {
		goto cleanup;
	}
	redislite_read_first(db, header);
	db->types = NULL;
	db->file = NULL;
	db->filename = NULL;
	int init = init_db(db);
	if (init != 0) {
		goto cleanup;
	}
	size_t size = strlen(filename) + 1;
	db->filename = redislite_malloc(size);
	memcpy(db->filename, filename, size);

cleanup:
	fclose(fp);
	return db;
}

redislite *redislite_create_database(const char *filename)
{
	int page_size = DEFAULT_PAGE_SIZE;

	redislite *db = redislite_malloc(sizeof(redislite));
	if (db == NULL) {
		return NULL;
	}
	db->root = NULL;
	db->types = NULL;
	db->file = NULL;
	db->filename = NULL;
	int init = init_db(db);
	if (init != 0) {
		return NULL;
	}

	size_t size = strlen(filename) + 1;
	db->filename = redislite_malloc(size);
	if (db->filename == NULL) {
		redislite_close_database(db);
		return NULL;
	}
	memcpy(db->filename, filename, size);
	db->file = NULL;
	db->page_size = page_size;
	db->file_change_counter = 0;
	db->number_of_pages = 0;
	db->first_freelist_page = 0;
	db->number_of_freelist_pages = 0;
	db->readonly = 0;

	redislite_page_index *page = (redislite_page_index *)redislite_page_index_create(db);
	if (page == NULL) {
		redislite_close_database(db);
		return NULL;
	}
	int ret = redislite_set_root(db, page);
	if (ret != REDISLITE_OK) {
		redislite_close_database(db);
		return NULL;
	}

	return db;
}

unsigned char *redislite_read_page(redislite *db, changeset *cs, int num)
{
	int i;
	unsigned char *data = redislite_malloc(sizeof(unsigned char) * db->page_size);
	if (data == NULL) {
		return NULL;
	}
	// TODO: binary search
	if (cs) {
		for (i = 0; i < cs->modified_pages_length; i++) {
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
#ifdef DEBUG
	fseek(db->file, 0L, SEEK_END);
	long size = ftell(db->file);
	/*
		if (size < db->page_size * (num + 1)) {
			redislite_free(data);
			return NULL;
		}
	*/
#endif
	i = fseek(db->file, (long)db->page_size * num, SEEK_SET);
	if (i != 0) {
		fprintf(stdout, "fseek returned %d\n", i);
	}
	size_t read = fread(data, sizeof(unsigned char), db->page_size, db->file);
	if (read < db->page_size && ferror(db->file)) {
		printf("Error reading\n");
	}
#ifdef DEBUG
	if (read < db->page_size && feof(db->file)) {
		printf("Early EOF (seek to pos %ld, size was %ld, attempt to read %d)\n", (long)db->page_size * num, size, db->page_size);
	}
#else
	if (read < db->page_size && feof(db->file)) {
		printf("Early EOF (seek to pos %ld, attempt to read %d)\n", (long)db->page_size * num, db->page_size);
	}
#endif
	if (read < db->page_size) {
		redislite_free(data);
		return NULL;
	}

	return data;
}

void redislite_close_database(redislite *db)
{
	if (db->file) {
		fclose(db->file);
	}
	if (db->filename) {
		redislite_free(db->filename);
	}
	redislite_free_index(db, db->root);
	int i;
	if (db->types) {
		for (i = 0; i < 256; i++)
			if (db->types[i]) {
				redislite_free(db->types[i]);
			}
		redislite_free(db->types);
	}
	redislite_free(db);
}
