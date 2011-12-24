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

int redislite_set_root(redislite *db, redislite_page_index_first *page)
{
	db->root = page;
	page->page->free_space -= 100;
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
	size_t i;
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
	size_t i;
	redislite_page *page;
	for (i = 0; i < cs->opened_pages_length; i++) {
		page = cs->opened_pages[i];
		redislite_page *_page = redislite_modified_page(cs, page->number);
		if (_page == NULL || _page->data != page->data) {
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
		fprintf(stderr, "Unable to write to file '%s'\n", cs->db->filename);
		return REDISLITE_ERR;
	}

	size_t i;
	unsigned char *data = (unsigned char *)redislite_malloc(sizeof(unsigned char) * cs->db->page_size);
	if (data == NULL) {
		fclose(file);
		return REDISLITE_OOM;
	}

	int status = REDISLITE_OK;
	for (i = 0; i < cs->modified_pages_length; ++i) {
		redislite_page *page = cs->modified_pages[i];

		memset(&data[0], '\0', cs->db->page_size); // TODO: we could allow garbage on unused bytes
		if (i == 0 && page->number == 0) {
			memcpy(data, HEADER_STRING, sizeof(HEADER_STRING));
			data[20] = (cs->db->page_size >> 8); // page size
			data[21] = (cs->db->page_size); // page size
			data[22] = WRITE_FORMAT_VERSION; // write format version
			data[23] = READ_FORMAT_VERSION; // read format version
			redislite_put_4bytes(&data[24], 0); // reserved
			redislite_put_4bytes(&data[28], cs->db->number_of_pages);
			redislite_put_4bytes(&data[32], cs->db->first_freelist_page);
			redislite_put_4bytes(&data[36], cs->db->number_of_freelist_pages);
			redislite_write_first(cs->db, &data[100], (redislite_page_index_first *)cs->db->root);
		}
		else {
			page->type->write_function(cs->db, &data[0], page->data);
		}
		fseek(file, cs->db->page_size * page->number, SEEK_SET);
		if (fwrite(data, sizeof(unsigned char), cs->db->page_size, file) < cs->db->page_size) {
			fprintf(stderr, "Unable to write to file '%s'\n", cs->db->filename);
			status = REDISLITE_ERR;
			break;
		}
	}
	redislite_free(data);
	fclose(file);
	return status;
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
	size_t pos;

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
				min = max = i;
			}
		}
		pos = min;
	}

	size_t i;
	if (cs->opened_pages_length > 0) {
		for (i = cs->opened_pages_length - 1; i >= pos; i--) {
			cs->opened_pages[i + 1] = cs->opened_pages[i];
			if (i == 0) {
				break;
			}
		}
	}

	cs->opened_pages[pos] = page;
	cs->opened_pages_length++;
	cs->opened_pages_free--;

	return page_number;
}

int redislite_close_opened_page(changeset *cs, int page_number)
{
	size_t i;
	redislite_page *page;
	for (i = 0; i < cs->opened_pages_length; i++) {
		page = cs->opened_pages[i];
		if (page->number == page_number) {
			page->type->free_function(cs->db, page->data);
			redislite_free(page);
			for (; i < cs->opened_pages_length - 1; i++) {
				cs->opened_pages[i] = cs->opened_pages[i + 1];
			}
			cs->opened_pages_length--;
			cs->opened_pages_free++;
			break;
		}
		if (page->number > page_number) {
			break;
		}
	}
	return REDISLITE_OK;
}

int redislite_add_modified_page(changeset *cs, int page_number, char type, void *page_data)
{
	if (cs->db->readonly) {
		return REDISLITE_READONLY;
	}

	if (page_number != -1) {
		redislite_page *page = redislite_modified_page(cs, page_number);
		if (page) {
			if (page->data != page_data) {
				redislite_close_opened_page(cs, page_number);
				page->type->free_function(cs->db, page->data);
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
			redislite_add_modified_page(cs, 0, REDISLITE_PAGE_TYPE_FIRST, cs->db->root);
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
	size_t pos = 0;

	// TODO: binary search
	size_t i;
	for (i = 0; i < cs->modified_pages_length; i++) {
		if (((redislite_page *)cs->modified_pages[i])->number > page_number) {
			break;
		}
		else {
			pos = i + 1;
		}
	}

	if (cs->modified_pages_length > 0) {
		for (i = cs->modified_pages_length - 1; i >= pos; i--) {
			cs->modified_pages[i + 1] = cs->modified_pages[i];
			if (i == 0) {
				break;
			}
		}
	}

	cs->modified_pages[pos] = page;
	cs->modified_pages_length++;
	cs->modified_pages_free--;
	if (page_number >= cs->db->number_of_pages) {
		cs->db->number_of_pages = page_number + 1;
		redislite_add_modified_page(cs, 0, REDISLITE_PAGE_TYPE_FIRST, cs->db->root);
	}
	return page_number;
}

unsigned char *redislite_read_page(redislite *db, changeset *cs, int num)
{
	unsigned char *data = redislite_malloc(sizeof(unsigned char) * db->page_size);
	if (data == NULL) {
		return NULL;
	}
	// TODO: binary search
	size_t i;
	if (cs) {
		for (i = 0; i < cs->modified_pages_length; i++) {
			redislite_page *page = cs->modified_pages[i];
			if (page->number == num) {
				redislite_write_index(db, &data[0], page->data);
				return data;
			}
		}
	}

	FILE *fp = fopen(db->filename, "rb");
	if (!fp) {
		fprintf(stderr, "Unable to open file '%s'\n", db->filename);
		return NULL;
	}
#ifdef DEBUG
	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);
	/*
		if (size < db->page_size * (num + 1)) {
			redislite_free(data);
			return NULL;
		}
	*/
#endif
	i = fseek(fp, (long)db->page_size * num, SEEK_SET);
	if (i != 0) {
		fprintf(stdout, "fseek returned %lu\n", (unsigned long)i);
	}
	size_t read = fread(data, sizeof(unsigned char), db->page_size, fp);
	if (read < db->page_size && ferror(fp)) {
		printf("Error reading\n");
	}
#ifdef DEBUG
	if (read < db->page_size && feof(fp)) {
		printf("Early EOF (seek to pos %ld, size was %ld, attempt to read %d)\n", (long)db->page_size * num, size, db->page_size);
	}
#else
	if (read < db->page_size && feof(fp)) {
		printf("Early EOF (seek to pos %lu, attempt to read %lu)\n", (unsigned long)db->page_size * num, (unsigned long)db->page_size);
	}
#endif
	fclose(fp);
	if (read < db->page_size) {
		redislite_free(data);
		return NULL;
	}

	return data;
}
