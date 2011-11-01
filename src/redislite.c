#include "redislite.h"
#include "core.h"
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
		type->delete_function = &redislite_delete_list;
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
		type->delete_function = &redislite_delete_list_first;
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
		type->identifier = REDISLITE_PAGE_TYPE_SET;
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
	db->root = redislite_read_first(db, &header[100]);
	if (db->root == NULL) {
		free(db);
		db = NULL;
		goto cleanup;
	}
	db->page_size = header[21] + (header[20] << 8);
	db->readonly = (header[22] > WRITE_FORMAT_VERSION);
	db->number_of_pages = redislite_get_4bytes(&header[28]);
	db->first_freelist_page = redislite_get_4bytes(&header[32]);
	db->number_of_freelist_pages = redislite_get_4bytes(&header[36]);
	db->types = NULL;
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
	size_t page_size = DEFAULT_PAGE_SIZE;

	redislite *db = redislite_malloc(sizeof(redislite));
	if (db == NULL) {
		return NULL;
	}
	db->root = NULL;
	db->types = NULL;
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
	db->page_size = page_size;
	db->file_change_counter = 0;
	db->number_of_pages = 0;
	db->first_freelist_page = 0;
	db->number_of_freelist_pages = 0;
	db->readonly = 0;

	redislite_page_index_first *first = (redislite_page_index_first *)create_page_index_first(db);
	if (first == NULL) {
		redislite_close_database(db);
		return NULL;
	}
	int ret = redislite_set_root(db, first);
	if (ret != REDISLITE_OK) {
		redislite_close_database(db);
		return NULL;
	}

	return db;
}


void redislite_close_database(redislite *db)
{
	if (db->filename) {
		redislite_free(db->filename);
	}
	redislite_free_first(db, db->root);
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
