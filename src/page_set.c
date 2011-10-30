#include "core.h"
#include "page_set.h"
#include "page_index.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static redislite_page_index_first *create_index_page(void *_db)
{
	redislite *db = (redislite *)_db;
	redislite_page_index_first *first = create_page_index_first(_db);
	if (first == NULL) {
		return NULL;
	}
	return first;
}

static int fetch_index_page(void *_db, void *_cs, char *key_name, size_t key_length, redislite_page_index_first **page)
{
	redislite *db = (redislite *)_db;
	redislite_page_index_first *root = NULL;
	char type;
	int status = redislite_value_page_for_key(db, _cs, db->root, key_name, key_length, &type);
	if (status < 0) {
		goto cleanup;
	}
	if (type != REDISLITE_PAGE_TYPE_FIRST) {
		status = REDISLITE_WRONG_TYPE;
		goto cleanup;
	}
	root = redislite_page_get(_db, _cs, status, type);
	if (!root) {
		status = REDISLITE_OOM;
	}
	else {
		*page = root;
	}

	return status;

cleanup:
	if (root) {
		redislite_free_index(_db, root);
	}
	return status;
}

int redislite_page_set_add(void *_cs, char *key_name, size_t key_length, char *str, size_t length)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = (redislite *)cs->db;
	redislite_page_index_first *page = NULL;
	int page_num;
	int status = page_num = fetch_index_page(db, _cs, key_name, key_length, &page);
	if (status < 0 && status != REDISLITE_NOT_FOUND) {
		return status;
	}

	if (page == NULL) {
		page = create_index_page(db);
		if (page == NULL) {
			return REDISLITE_OOM;
		}
		page_num = status = redislite_add_modified_page(_cs, -1, REDISLITE_PAGE_TYPE_FIRST, page);
		if (status < 0) {
			redislite_free(page);
			return status;
		}
		status = redislite_insert_key(_cs, db->root, key_name, key_length, status, REDISLITE_PAGE_TYPE_FIRST);
		if (status < 0) {
			// page was added to the changeset, no need to free
			return status;
		}
	}
	status = redislite_insert_key(_cs, page->page, str, length, 1, REDISLITE_PAGE_TYPE_FIRST);
	if (status < 0) {
		return status;
	}
	status = redislite_add_modified_page(_cs, page_num, REDISLITE_PAGE_TYPE_FIRST, page);

	return status;
}

int redislite_page_set_contains(void *_db, void *_cs, char *key_name, size_t key_length, char *str, size_t length)
{
	redislite *db = (redislite *)_db;
	redislite_page_index_first *page = NULL;
	char type;
	int page_num;
	int status = page_num = fetch_index_page(_db, _cs, key_name, key_length, &page);
	if (status < 0) {
		return status;
	}

	status = redislite_value_page_for_key(db, _cs, page->page, str, length, &type);
	return status == 1;
}
