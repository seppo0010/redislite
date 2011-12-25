#include <ctype.h>
#include "core.h"
#include "page_string.h"
#include "page_index.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

void redislite_delete_string(void *_cs, void *_page)
{
	redislite_page_string *page = (redislite_page_string *)_page;
	if (page == NULL) {
		return;
	}
	if (page->right_page != 0) {
		redislite_page_delete(_cs, page->right_page, REDISLITE_PAGE_TYPE_STRING_OVERFLOW);
	}
}

void redislite_free_string(void *_db, void *_page)
{
	_db = _db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_page_string *page = (redislite_page_string *)_page;
	if (page == NULL) {
		return;
	}
	redislite_free(page->value);
	redislite_free(page);
}

void redislite_write_string(void *_db, unsigned char *data, void *_page)
{
	redislite *db = (redislite *)_db;
	redislite_page_string *page = (redislite_page_string *)_page;
	if (page == NULL) {
		return;
	}

	redislite_put_4bytes(&data[0], 0); // reserverd
	redislite_put_4bytes(&data[4], page->size);
	redislite_put_4bytes(&data[8], page->right_page);
	size_t size = db->page_size - 12;
	if (size > page->size) {
		size = page->size;
	}
	memcpy(&data[12], page->value, size);
}

void *redislite_read_string(void *_db, unsigned char *data)
{
	redislite *db = (redislite *)_db;
	redislite_page_string *page = redislite_malloc(sizeof(redislite_page_string));

	page->size = redislite_get_4bytes(&data[4]);
	page->right_page = redislite_get_4bytes(&data[8]);

	size_t size = db->page_size - 12;
	page->value = redislite_malloc(sizeof(char) * size);
	memcpy(page->value, &data[12], size);
	return page;
}


void redislite_delete_string_overflow(void *_cs, void *_page)
{
	redislite_page_string_overflow *page = (redislite_page_string_overflow *)_page;
	if (page == NULL) {
		return;
	}
	if (page->right_page != 0) {
		redislite_page_delete(_cs, page->right_page, REDISLITE_PAGE_TYPE_STRING_OVERFLOW);
	}
}

void redislite_free_string_overflow(void *_db, void *_page)
{
	_db = _db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_page_string_overflow *page = (redislite_page_string_overflow *)_page;
	redislite_free(page->value);
	redislite_free(page);
}

void redislite_write_string_overflow(void *_db, unsigned char *data, void *_page)
{
	redislite *db = (redislite *)_db;
	redislite_page_string_overflow *page = (redislite_page_string_overflow *)_page;

	redislite_put_4bytes(&data[0], 0); // reserved
	redislite_put_4bytes(&data[4], page->right_page);
	memcpy(&data[8], page->value, db->page_size - 8);
}

void *redislite_read_string_overflow(void *_db, unsigned char *data)
{
	redislite *db = (redislite *)_db;
	redislite_page_string_overflow *page = redislite_malloc(sizeof(redislite_page_string_overflow));

	page->right_page = redislite_get_4bytes(&data[4]);

	size_t size = db->page_size - 8;
	page->value = redislite_malloc(sizeof(char) * size);
	memcpy(page->value, &data[8], size);
	return page;
}

static int add_extra_string(void *_cs, char *str, size_t length)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = cs->db;
	size_t total_pages = (size_t)ceil((float)(length) / (db->page_size - 8));
	size_t i, size;
	int next_page = 0;
	for (i = total_pages; i >= 1; i--) {
		redislite_page_string_overflow *overflow_page = redislite_malloc(sizeof(redislite_page_string_overflow));
		if (overflow_page == NULL) {
			return REDISLITE_OOM;
		}
		char *data = redislite_malloc(sizeof(char) * (db->page_size - 8));
		if (data == NULL) {
			redislite_free(overflow_page);
			return REDISLITE_OOM;
		}
		memset(data, 0, db->page_size - 8);
		if (i == total_pages) {
			size = length - (db->page_size - 8) * (total_pages - 1);
		}
		else {
			size = db->page_size - 8;
		}
		memcpy(data, &str[(db->page_size - 8) * (i - 1)], size);
		overflow_page->db = db;
		overflow_page->right_page = next_page;
		overflow_page->value = data;
		next_page = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING_OVERFLOW, overflow_page);
		if (next_page < 0) {
			redislite_free(overflow_page);
			redislite_free(data);
			return next_page;
		}
	}
	return next_page;
}

int redislite_insert_string(void *_cs, char *str, size_t length, int *num)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = cs->db;
	if (db->readonly) {
		return REDISLITE_READONLY;
	}
	redislite_page_string *page = redislite_malloc(sizeof(redislite_page_string));
	if (page == NULL) {
		return REDISLITE_OOM;
	}
	page->size = length;
	size_t first_page_size = db->page_size - 12;
	if (first_page_size < length) {
		page->right_page = add_extra_string(_cs, &str[first_page_size], length - first_page_size);
		if (page->right_page < 0) {
			int right_page = page->right_page;
			redislite_free(page);
			return right_page;
		}
		char *data = redislite_malloc(sizeof(char) * db->page_size - 12);
		if (data == NULL) {
			redislite_free(page);
			return REDISLITE_OOM;
		}
		memcpy(data, str, db->page_size - 12);
		page->value = data;
		*num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING, page);
		if (*num < 0) {
			redislite_free(page);
			redislite_free(data);
			return (*num);
		}
	}
	else {
		page->right_page = 0;
		char *data = redislite_malloc(sizeof(char) * db->page_size - 12);
		if (data == NULL) {
			redislite_free(page);
			return REDISLITE_OOM;
		}
		memcpy(data, str, length);
		page->value = data;
		*num = redislite_add_modified_page(cs, -1, REDISLITE_PAGE_TYPE_STRING, page);
		if (*num < 0) {
			return (*num);
		}
	}
	return REDISLITE_OK;
}

int redislite_page_string_get_by_keyname(void *_db, void *_cs, char *key_name, size_t key_length, char **str, size_t *length)
{
	redislite *db = (redislite *)_db;
	char type;
	void *_page = redislite_page_get_by_keyname(_db, _cs, key_name, key_length, &type);
	if (_page == NULL) {
		*length = 0;
		return REDISLITE_NOT_FOUND;
	}
	if (type != REDISLITE_PAGE_TYPE_STRING) {
		if (_cs == NULL) {
			redislite_page_type *page_type = redislite_page_get_type(db, type);
			page_type->free_function(db, _page);
		}

		*length = 0;
		return REDISLITE_WRONG_TYPE;
	}
	redislite_page_string *page = (redislite_page_string *)_page;
	if (page->size == 0) {
		*str = NULL;
		*length = 0;
		return REDISLITE_OK;
	}
	char *data = redislite_malloc(sizeof(char) * page->size);
	if (data == NULL) {
		if (_cs == NULL) {
			redislite_free_string(db, page);
		}
		return REDISLITE_OOM;
	}
	memcpy(data, page->value, MIN(page->size, db->page_size - 12));

	int next = page->right_page;
	size_t i = 0;
	size_t pos, size;
	while (next != 0) {
		void *_extra = redislite_page_get(_db, _cs, next, REDISLITE_PAGE_TYPE_STRING_OVERFLOW);
		// TODO handle OOM
		redislite_page_string_overflow *extra = (redislite_page_string_overflow *)_extra;
		pos = db->page_size - 12 + i++ * (db->page_size - 8);
		size = MIN(page->size - pos, db->page_size - 8);
		memcpy(&data[pos], extra->value, size);
		next = extra->right_page;
		if (_cs == NULL) {
			redislite_free_string_overflow(db, extra);
		}
	}

	*length = page->size;
	if (_cs == NULL) {
		redislite_free_string(db, page);
	}
	*str = data;
	return REDISLITE_OK;
}


int redislite_page_string_getset_key_string(void *_cs, char *key_name, size_t key_length, char *str, size_t length, char **previous_value, size_t *previous_value_length)
{
	changeset *cs = (changeset *)_cs;
	int status = redislite_page_string_get_by_keyname(cs->db, _cs, key_name, key_length, previous_value, previous_value_length);
	if (status != REDISLITE_OK && status != REDISLITE_NOT_FOUND) {
		return status;
	}
	int _status = redislite_page_string_set_key_string(_cs, key_name, key_length, str, length);
	if (_status != REDISLITE_OK) {
		status = _status;
	}
	// FIXME: there should be a more efficient way to do this instead of two different calls
	return status;
}

int redislite_page_string_set_key_string(void *_cs, char *key_name, size_t key_length, char *str, size_t length)
{
	changeset *cs = (changeset *)_cs;
	int left;
	int status = redislite_insert_string(cs, str, length, &left);
	if (status != REDISLITE_OK) {
		return status;
	}

	status = redislite_insert_key(cs, cs->db->root, 0, key_name, key_length, left, REDISLITE_PAGE_TYPE_STRING);
	if (status < 0) {
		return status;
	}

	return REDISLITE_OK;
}

int redislite_page_string_setnx_key_string(void *_cs, char *key_name, size_t key_length, char *str, size_t length)
{
	changeset *cs = (changeset *)_cs;
	char type;
	int exists = redislite_value_page_for_key(cs->db, cs, cs->db->root, key_name, key_length, &type);
	if (exists == REDISLITE_NOT_FOUND) {
		exists = redislite_page_string_set_key_string(_cs, key_name, key_length, str, length);
		if (exists == REDISLITE_OK) {
			return 1;
		}
		else {
			return exists;    // error
		}
	}
	else {
		return 0; // key already exists
	}
}

int redislite_page_string_strlen_by_keyname(void *_db, void *_cs, char *key_name, size_t key_length)
{
	redislite *db = (redislite *)_db;
	char type;
	int page_num = redislite_value_page_for_key(_db, _cs, db->root, key_name, key_length, &type);
	if (page_num == REDISLITE_NOT_FOUND) {
		return 0;
	}
	if (page_num < 0) {
		return page_num;
	}
	if (type != REDISLITE_PAGE_TYPE_STRING) {
		return REDISLITE_WRONG_TYPE;
	}
	redislite_page_string *page = redislite_page_get(_db, _cs, page_num, type);
	return page->size;
}

int redislite_page_string_append_key_string(void *_cs, char *key_name, size_t key_length, char *str, size_t length, size_t *new_length)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = cs->db;

	if (length == 0) {
		if (new_length) {
			*new_length = redislite_page_string_strlen_key_string(db, _cs, key_name, key_length);
		}
		return REDISLITE_OK;
	}

	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, key_name, key_length, &type);
	if (page_num >= 0) {
		if (type != REDISLITE_PAGE_TYPE_STRING) {
			return REDISLITE_WRONG_TYPE;
		}
		redislite_page_string *page = redislite_page_get(cs->db, _cs, page_num, type);
		size_t previous_length = page->size;
		if (page->right_page > 0) {
			size_t pos = cs->db->page_size - 12;
			int overflow_page_num = page->right_page;
			redislite_page_string_overflow *overflow = redislite_page_get(cs->db, _cs, page->right_page, REDISLITE_PAGE_TYPE_STRING_OVERFLOW);
			while (overflow->right_page != 0) {
				overflow_page_num = overflow->right_page;
				overflow = redislite_page_get(cs->db, _cs, overflow->right_page, REDISLITE_PAGE_TYPE_STRING_OVERFLOW);
				pos += cs->db->page_size - 8;
			}
			size_t page_pos = previous_length - pos;
			size_t free_bytes = 0;
			if (cs->db->page_size > page_pos + 8) {
				free_bytes = cs->db->page_size - page_pos - 8;
			}

			int new_in_page = length;
			if (length > free_bytes) {
				new_in_page = free_bytes;
				overflow->right_page = add_extra_string(_cs, &str[free_bytes], length - free_bytes);
			}
			memcpy(&overflow->value[page_pos], str, new_in_page);
			redislite_add_modified_page(cs, overflow_page_num, REDISLITE_PAGE_TYPE_STRING_OVERFLOW, overflow);
			page->size += length;
			redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_STRING, page);
		}
		else {
			if (cs->db->page_size >= page->size + length + 8) {
				memcpy(&page->value[page->size], str, length);
				page->size += length;
				redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_STRING, page);
			}
			else {
				size_t first_page_size = db->page_size - 12;

				page->right_page = add_extra_string(_cs, &str[first_page_size - page->size], length + page->size - first_page_size);
				if (page->right_page < 0) {
					redislite_free(page);
					return page->right_page;
				}
				memcpy(&page->value[page->size], str, db->page_size - 12 - page->size);
				page->size += length;
				redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_STRING, page);
			}
		}
		if (new_length) {
			*new_length = page->size;
		}
		return REDISLITE_OK;
	}
	else {
		if (new_length) {
			*new_length = length;
		}
		return redislite_page_string_set_key_string(_cs, key_name, key_length, str, length);
	}
}

int redislite_page_string_incrbyfloat_by_key_string(void *_cs, char *key_name, size_t key_length, long double incr, char **new_value, int *new_value_length)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = cs->db;

	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, key_name, key_length, &type);
	if (page_num < 0) {
		char *s_value = redislite_malloc(256);
		int size = snprintf(s_value, 256, "%.17Lg", incr);
		if (size <= 0) {
			return REDISLITE_EXPECT_DOUBLE;
		}
		s_value = redislite_realloc(s_value, size + 1);
		s_value[size] = '\0';
		if (new_value) {
			*new_value = s_value;
			*new_value_length = size;
		}
		return redislite_page_string_set_key_string(_cs, key_name, key_length, s_value, size);
	}

	if (type != REDISLITE_PAGE_TYPE_STRING) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_string *page = redislite_page_get(cs->db, _cs, page_num, type);
	if (page->right_page || page->size > db->page_size - 13) {
		return REDISLITE_INVALID_FLOAT;
	}

	long double value;
	char *eptr, *strvalue = redislite_malloc(256); // TODO: how long can a long double be?
	int len;
	if (strvalue == NULL) {
		return REDISLITE_OOM;
	}

	value = strtold(page->value, &eptr);
	if (isspace(page->value[0]) || eptr[0] != '\0' || isnan(value)) {
		return REDISLITE_INVALID_FLOAT;
	}
	value += incr;
	if (isnan(value) || isinf(value)) {
		return REDISLITE_NAN_OR_INFINITY;
	}

	len = snprintf(strvalue, 256, "%.17Lg", value);
	strvalue = redislite_realloc(strvalue, len + 1); // shrinking
	strvalue[len] = '\0';
	if (page->size > 0) {
		redislite_page_string_set_key_string(_cs, key_name, key_length, strvalue, len);
		if (new_value) {
			*new_value = strvalue;
			*new_value_length = len;
		}
		return REDISLITE_OK;
	}
	return REDISLITE_ERR;
}

int redislite_page_string_incr_by_key_string(void *_cs, char *key_name, size_t key_length, long long incr, long long *new_value)
{
	changeset *cs = (changeset *)_cs;
	redislite *db = cs->db;

	char type;
	int page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, key_name, key_length, &type);
	if (page_num < 0) {
		char s_value[21];
		if (new_value) {
			*new_value = incr;
		}
		int size = sprintf(s_value, "%lld", incr);
		if (size < 0) {
			return REDISLITE_EXPECT_INTEGER;
		}
		return redislite_page_string_set_key_string(_cs, key_name, key_length, s_value, size);
	}

	if (type != REDISLITE_PAGE_TYPE_STRING) {
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_string *page = redislite_page_get(cs->db, _cs, page_num, type);
	if (page->right_page || page->size > db->page_size - 13) {
		return REDISLITE_EXPECT_INTEGER;
	}

	long long value;
	int status = str_to_long_long(page->value, page->size, &value);
	if (status != REDISLITE_OK) {
		return status;
	}
	value += incr;

	if (new_value) {
		*new_value = value;
	}
	page->size = sprintf(page->value, "%lld", value);
	if (page->size > 0) {
		int ret = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_STRING, page);
		if (ret < 0) {
			return ret;
		}
		return REDISLITE_OK;
	}
	return REDISLITE_ERR;
}

int redislite_page_string_incr_key_string(void *_cs, char *key_name, size_t key_length, long long *new_value)
{
	return redislite_page_string_incr_by_key_string(_cs, key_name, key_length, 1, new_value);
}
int redislite_page_string_decr_key_string(void *_cs, char *key_name, size_t key_length, long long *new_value)
{
	return redislite_page_string_incr_by_key_string(_cs, key_name, key_length, -1, new_value);
}
int redislite_page_string_decr_by_key_string(void *_cs, char *key_name, size_t key_length, long long decr, long long *new_value)
{
	return redislite_page_string_incr_by_key_string(_cs, key_name, key_length, -decr, new_value);
}

int redislite_echo(char *src_name, size_t src_length, char **dst_name, size_t *dst_length)
{
	if (src_length > 0 && dst_name && dst_length) {
		char *dst = redislite_malloc(sizeof(char) * src_length);
		if (dst == NULL) {
			return REDISLITE_OOM;
		}
		memcpy(dst, src_name, src_length);
		*dst_name = dst;
		*dst_length = src_length;
		return REDISLITE_OK;
	}
	return REDISLITE_ERR;
}

int redislite_page_string_strlen_key_string(void *_db, void *_cs, char *key_name, size_t key_length)
{
	redislite *db = (redislite *)_db;
	char type;
	void *_page = redislite_page_get_by_keyname(_db, _cs, key_name, key_length, &type);
	if (_page == NULL) {
		return 0;    // this is what redis returns for strlen on unexisting keys
	}
	if (type != REDISLITE_PAGE_TYPE_STRING) {
		if (_cs == NULL) {
			redislite_page_type *page_type = redislite_page_get_type(db, type);
			page_type->free_function(db, _page);
		}
		return REDISLITE_WRONG_TYPE;
	}
	redislite_page_string *page = (redislite_page_string *)_page;
	size_t len = page->size;
	if (_cs == NULL) {
		redislite_free_string(db, page);
	}
	return len;
}

int redislite_page_string_getrange_key_string(void *_db, void *_cs, char *key_name, size_t key_length, int _start, int _end, char **str, size_t *str_length)
{
	redislite *db = (redislite *)_db;
	char type;
	void *_page = redislite_page_get_by_keyname(_db, _cs, key_name, key_length, &type);
	if (_page == NULL) {
		// redis returns an empty string
		if (str) {
			*str = 0;
		}
		if (str_length) {
			*str_length = 0;
		}
		return REDISLITE_OK;
	}
	if (type != REDISLITE_PAGE_TYPE_STRING) {
		if (_cs == NULL) {
			redislite_page_type *page_type = redislite_page_get_type(db, type);
			page_type->free_function(db, _page);
		}
		return REDISLITE_WRONG_TYPE;
	}
	redislite_page_string *page = (redislite_page_string *)_page;
	size_t len = page->size;

	long __start = _start, __end = _end;
	if (__start < 0) {
		__start = len + __start;
	}
	if (__end < 0) {
		__end = len + __end;
	}
	if (__start < 0) {
		__start = 0;
	}
	if (__end < 0) {
		__end = 0;
	}

	size_t start = __start;
	size_t end = __end;

	if (len > 0 && end >= len) {
		end = len - 1;
	}

	if (len == 0 || start > end) {
		if (str) {
			*str = NULL;
		}
		if (str_length) {
			*str_length = 0;
		}
		return REDISLITE_OK;
	}

	char *response = redislite_malloc(sizeof(char) * (end - start + 1));
	if (response == NULL) {
		if (_cs == NULL) {
			redislite_free_string(db, page);
		}
		return REDISLITE_OOM;
	}

	size_t page_end = db->page_size - 12;
	size_t copied = 0;
	if (start < page_end) {
		copied += MIN(end + 1, page_end) - start;
		memcpy(response, &page->value[start], copied);
	}

	int next = page->right_page;

	int p = 0;
	while (copied < end + 1 - start) {
		redislite_page_string_overflow *page_overflow = redislite_page_get(_db, _cs, next, REDISLITE_PAGE_TYPE_STRING_OVERFLOW);
		if (page_overflow == NULL) {
			if (_cs == NULL) {
				redislite_free_string(_db, page);
			}
			redislite_free(response);
			return REDISLITE_OOM;
		}
		page_end = db->page_size - 8;

		if (start < copied + page_end * (1 + p) + db->page_size + 12) {
			int pos = 0;
			if (copied == 0) {
				pos = start - page_end * p - db->page_size + 12;
			}
			int size = MIN(end - copied - start + 1, page_end - pos);
			memcpy(&response[copied], &page_overflow->value[pos], size);
			copied += size;
		}

		next = page_overflow->right_page;
		if (_cs == NULL) {
			redislite_free_string_overflow(_db, page_overflow);
		}

		if (next == 0) {
			break;
		}
		p++;
	}
	*str = response;
	*str_length = copied;
	return REDISLITE_OK;
}

int redislite_page_string_setrange_key_string(void *_cs, char *key_name, size_t key_length, size_t start, char *str, size_t length, size_t *new_length)
{
	changeset *cs = (changeset *)_cs;
	size_t len_in_page;

	char type;
	int status, page_num = redislite_value_page_for_key(cs->db, cs, cs->db->root, key_name, key_length, &type);

	if (page_num < 0) {
		if (start > 0) {
			void *zerofill = redislite_malloc(sizeof(char) * start);
			memset(zerofill, '\0', start);
			status = redislite_page_string_set_key_string(_cs, key_name, key_length, zerofill, start);
			if (status < 0) {
				return status;
			}
			redislite_free(zerofill);
		}
		return redislite_page_string_append_key_string(_cs, key_name, key_length, str, length, new_length);
	}

	if (type != REDISLITE_PAGE_TYPE_STRING) {
		return REDISLITE_WRONG_TYPE;
	}

	size_t copied = 0;
	redislite_page_string *page = redislite_page_get(cs->db, _cs, page_num, type);
	if (start < cs->db->page_size - 12) {
		len_in_page = cs->db->page_size - 12 - start;
		if (len_in_page > length) {
			len_in_page = length;
		}
		memcpy(&page->value[start], str, len_in_page);
		copied += len_in_page;
	}

	size_t page_pos, pos = cs->db->page_size - 12;
	int next = page->right_page;
	redislite_page_string_overflow *overflow = NULL;

	while ((pos < start + length) && (next > 0) && (overflow = redislite_page_get(cs->db, _cs, next, REDISLITE_PAGE_TYPE_STRING_OVERFLOW))) {
		if (pos + cs->db->page_size - 8 > start) {
			if (pos >= start) {
				page_pos = 0;
			}
			else {
				page_pos = start - pos;
			}
			len_in_page = cs->db->page_size - 8;
			if (pos + cs->db->page_size - 8 > length + start - pos) {
				len_in_page = length + start - pos;
			}
			memcpy(&overflow->value[page_pos], &str[copied], len_in_page);
			copied += len_in_page;
		}
		pos += cs->db->page_size - 8;
		status = redislite_add_modified_page(cs, next, REDISLITE_PAGE_TYPE_STRING_OVERFLOW, overflow);
		if (status < 0) {
			return status;
		}
		next = overflow->right_page;
	}
	if (pos < start + length) {
		next = add_extra_string(cs, &str[copied], length - copied);
		if (overflow != NULL) {
			overflow->right_page = next;
		}
		else {
			page->right_page = next;
		}
	}

	if (start + length > page->size) {
		page->size = start + length;
	}

	status = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_STRING, page);
	if (new_length) {
		*new_length = page->size;
	}
	return status > 0 ? REDISLITE_OK : status;
}

int redislite_page_string_getbit_key_string(void *_db, void *_cs, char *key_name, size_t key_length, long long bitoffset)
{
	if ((bitoffset < 0) || ((unsigned long long)bitoffset >> 3) >= (512 * 1024 * 1024)) {
		return REDISLITE_BIT_OFFSET_INVALID;
	}

	redislite *db = (redislite *)_db;
	char type;
	void *_page = redislite_page_get_by_keyname(_db, _cs, key_name, key_length, &type);
	if (_page == NULL) {
		// redis returns an empty string
		return 0;
	}
	if (type != REDISLITE_PAGE_TYPE_STRING) {
		if (_cs == NULL) {
			redislite_page_type *page_type = redislite_page_get_type(_db, type);
			page_type->free_function(_db, _page);
		}
		return REDISLITE_WRONG_TYPE;
	}

	redislite_page_string *page = (redislite_page_string *)_page;
	size_t byte = bitoffset >> 3;
	size_t bit = 7 - (bitoffset & 0x7);
	size_t bitval = 0;

	if (byte < page->size) {
		if (byte <= db->page_size - 12) {
			bitval = ((char *)page->value)[byte] & (1 << bit);
		}
		else {
			int next = page->right_page;
			redislite_page_string_overflow *overflow;
			int pos = db->page_size - 12;
			while (next > 0) {
				overflow = redislite_page_get(_db, _cs, next, REDISLITE_PAGE_TYPE_STRING_OVERFLOW);
				if (byte <= pos + db->page_size - 8) {
					bitval = ((char *)overflow->value)[byte - pos] & (1 << bit);
					break;
				}
				pos += db->page_size - 8;
				if (_cs == NULL) {
					redislite_free_string_overflow(_db, overflow);
				}
			}
		}
	}

	return bitval == 0 ? 0 : 1;
}

int redislite_page_string_setbit_key_string(void *_cs, char *key_name, size_t key_length, long long bitoffset, long on)
{
	changeset *cs = (changeset *)_cs;
	if ((bitoffset < 0) || (unsigned long long)bitoffset >= (cs->db->page_size - 12) << 3) {
		return REDISLITE_BIT_OFFSET_INVALID;
	}

	redislite_page_string *page;
	char type;
	size_t byte = bitoffset >> 3;
	int page_num = redislite_value_page_for_key(cs->db, _cs, cs->db->root, key_name, key_length, &type);
	if (page_num == REDISLITE_NOT_FOUND) {
		page_num = -1;
		page = redislite_malloc(sizeof(redislite_page_string));
		if (page == NULL) {
			return REDISLITE_OOM;
		}
		char *value = redislite_malloc(sizeof(char) * cs->db->page_size - 12);
		if (value == NULL) {
			redislite_free(page);
			return REDISLITE_OOM;
		}
		memset(value, '\0', cs->db->page_size - 12);
		page->value = value;
		page->size = byte + 1;
		page->db = cs->db;
		page->right_page = 0;
	}
	else {
		if (page_num < 0) {
			return page_num;
		}
		if (type != REDISLITE_PAGE_TYPE_STRING) {
			return REDISLITE_WRONG_TYPE;
		}

		page = redislite_page_get(cs->db, _cs, page_num, type);
		if (page == NULL) {
			return REDISLITE_OOM;
		}
	}

	if (byte >= page->size) {
		memset(page->value + page->size, '\0', byte - page->size);
		page->size = byte + 1;
	}
	int byteval = page->value[byte];
	size_t bit = 7 - (bitoffset & 0x7);
	size_t bitval = byteval & (1 << bit);
	byteval &= ~(1 << bit);
	byteval |= ((on & 0x1) << bit);
	page->value[byte] = byteval;
	int left = redislite_add_modified_page(cs, page_num, REDISLITE_PAGE_TYPE_STRING, page);

	if (page_num == -1) {
		int status = redislite_insert_key(cs, cs->db->root, 0, key_name, key_length, left, REDISLITE_PAGE_TYPE_STRING);
		if (status < 0) {
			return status;
		}
	}

	return bitval ? 1 : 0;
}
