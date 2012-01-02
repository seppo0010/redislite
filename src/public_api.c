#include <ctype.h>
#include "public_api.h"
#include "core.h"
#include "redislite.h"
#include "sds.h"
#include "page_index.h"
#include "page_string.h"
#include "page_list.h"
#include "page_set.h"
#include "util.h"
#include "version.h"
#include <math.h>
#include <strings.h>

char *redislite_git_SHA1();
char *redislite_git_dirty();

redislite_reply *redislite_create_reply()
{
	redislite_reply *reply = redislite_malloc(sizeof(redislite_reply));
	if (reply == NULL) {
		return NULL;
	}
	reply->type = REDISLITE_REPLY_NIL;
	return reply;
}

static void redislite_free_reply_value(redislite_reply *reply)
{
	size_t j;

	switch(reply->type) {
		case REDISLITE_REPLY_INTEGER:
			break; /* Nothing to free */

		case REDISLITE_REPLY_ARRAY:
			for (j = 0; j < reply->elements; j++)
				if (reply->element[j]) {
					redislite_free_reply(reply->element[j]);
				}
			if (reply->element) {
				redislite_free(reply->element);
			}
			break;

		case REDISLITE_REPLY_ERROR:
		case REDISLITE_REPLY_STATUS:
		case REDISLITE_REPLY_STRING:
			if (reply->str) {
				redislite_free(reply->str);
			}
			break;
	}
}

void redislite_free_reply(redislite_reply *reply)
{
	redislite_free_reply_value(reply);
	redislite_free(reply);
}

void redislite_free_params(redislite_params *param)
{
	if (param->must_free_argv) {
		if (param->argv != NULL) {
			int i;
			for (i = 0; i < param->argc; i++) {
				if (param->argv[i] != NULL) {
					redislite_free(param->argv[i]);
				}
			}
			redislite_free(param->argv);
		}
		if (param->argvlen != NULL) {
			redislite_free(param->argvlen);
		}
	}
	redislite_free(param);
}

redislite_params *redislite_create_params()
{
	redislite_params *params = redislite_malloc(sizeof(redislite_params));
	params->must_free_argv = 0;
	params->argc = 0;
	params->argv = NULL;
	params->argvlen = NULL;
	return params;
}



static const char *wrong_arity = "wrong number of arguments for '%s' command";
static const char *unknown_command = "unknown command '%s'";
static const char *wrong_type = "Operation against a key holding the wrong kind of value";
static const char *out_of_memory = "Redislite ran out of memory";
static const char *unknown_error = "Unknown error";
static const char *expected_string = "Value is not a string";
static const char *expected_integer = "value is not an integer or out of range";
static const char *expected_double = "Value is not a double";
static const char *invalid_bit_offset = "bit offset is not an integer or out of range";
static const char *invalid_bit = "ERR bit is not an integer or out of range";
static const char *maximum_size = "string exceeds maximum allowed size";
static const char *index_out_of_range = "index out of range";
static const char *key_not_found = "ERR no such key";
static const char *syntax_error = "ERR syntax error";
static const char *invalid_float = "value is not a valid float";
static const char *nan_or_infinity = "increment would produce NaN or Infinity";
static const char *ok = "OK";
static const char *not_implemented_yet = "This command hasn't been implemented on redislite yet";
static const char *implementation_not_planned = "This command hasn't been planned to be implemented on redislite";

static void set_status_message(int status, redislite_reply *reply)
{
	switch (status) {
		case REDISLITE_OK:
			redislite_free_reply_value(reply);
			reply->type = REDISLITE_REPLY_STATUS;
			reply->str = redislite_malloc(strlen(ok) + 1);
			if (reply->str == NULL) {
				// todo: what should we do here?!
				reply->type = REDISLITE_REPLY_NIL;
				return;
			}

			memcpy(reply->str, ok, strlen(ok) + 1);
			reply->len = strlen(ok) + 1;
	}
}

static void set_error_message(int status, redislite_reply *reply)
{
	const char *error = NULL;
	switch (status) {
		case REDISLITE_NOT_FOUND: {
				redislite_free_reply_value(reply);
				reply->type = REDISLITE_REPLY_NIL;
				return;
			}
		case REDISLITE_OOM: {
				redislite_free_reply_value(reply);
				reply->type = REDISLITE_REPLY_ERROR;
				reply->str = redislite_malloc(strlen(out_of_memory) + 1);
				if (reply->str == NULL) {
					// TODO: what should we do here?!
					// may be we should have a static error message for OOM? or a different status?
					// don't do this: set_error_message(REDISLITE_OOM, reply);
					reply->len = 0;
					return;
				}

				memcpy(reply->str, out_of_memory, strlen(out_of_memory) + 1);
				reply->len = strlen(out_of_memory) + 1;
				return;
			}
		case REDISLITE_WRONG_TYPE: {
				error = wrong_type;
				break;
			}
		case REDISLITE_EXPECT_STRING: {
				error = expected_string;
				break;
			}
		case REDISLITE_EXPECT_INTEGER: {
				error = expected_integer;
				break;
			}
		case REDISLITE_MAXIMUM_SIZE: {
				error = maximum_size;
				break;
			}
		case REDISLITE_EXPECT_DOUBLE: {
				error = expected_double;
				break;
			}
		case REDISLITE_NOT_IMPLEMENTED_YET: {
				error = not_implemented_yet;
				break;
			}
		case REDISLITE_IMPLEMENTATION_NOT_PLANNED: {
				error = implementation_not_planned;
				break;
			}
		case REDISLITE_BIT_OFFSET_INVALID: {
				error = invalid_bit_offset;
				break;
			}
		case REDISLITE_BIT_INVALID: {
				error = invalid_bit;
				break;
			}
		case REDISLITE_INDEX_OUT_OF_RANGE: {
				error = index_out_of_range;
				break;
			}
		case REDISLITE_SYNTAX_ERROR: {
				error = syntax_error;
				break;
			}
		case REDISLITE_INVALID_FLOAT: {
				error = invalid_float ;
				break;
			}
		case REDISLITE_NAN_OR_INFINITY : {
				error = nan_or_infinity;
				break;
			}

		default: {
				error = unknown_error;
				break;
			}
	}

	if (error == NULL) {
		// an assert may be?
		redislite_free_reply_value(reply);
		reply->type = REDISLITE_REPLY_NIL;
		return;
	}

	redislite_free_reply_value(reply);
	reply->type = REDISLITE_REPLY_ERROR;
	reply->str = redislite_malloc(strlen(error) + 1);
	if (reply->str == NULL) {
		set_error_message(REDISLITE_OOM, reply);
		return;
	}
	memcpy(reply->str, error, strlen(error) + 1);
	reply->len = strlen(error) + 1;
}

redislite_reply *redislite_strlen_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	int strlen = redislite_page_string_strlen_by_keyname(db, NULL, key, len);
	if (strlen >= 0) {
		reply->type = REDISLITE_REPLY_INTEGER;
		reply->integer = strlen;
	}
	else {
		set_error_message(strlen, reply);
	}
	return reply;
}

redislite_reply *redislite_getset_command(redislite *db, redislite_params *params)
{
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	changeset *cs = redislite_create_changeset(db);
	size_t len = 0;
	int status = redislite_page_string_getset_key_string(cs, params->argv[1], params->argvlen[1], params->argv[2], params->argvlen[2], &reply->str, &len);
	reply->len = (int)len;
	if (status >= 0 || status == REDISLITE_NOT_FOUND) {
		int _status = redislite_save_changeset(cs);
		if (_status != REDISLITE_OK) {
			status = _status;
		}
	}
	redislite_free_changeset(cs);

	if (status == REDISLITE_NOT_FOUND) {
		reply->type = REDISLITE_REPLY_NIL;
	}
	else if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_STRING;
	}
	else {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_get_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	size_t reply_len = 0;
	int status = redislite_page_string_get_by_keyname(db, NULL, key, len, &reply->str, &reply_len);
	reply->len = (int)reply_len;

	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_STRING;
	}
	else {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_set_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	value = params->argv[2];
	value_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_page_string_set_key_string(cs, key, len, value, value_len);
	if (status >= 0) {
		status = redislite_save_changeset(cs);
	}
	redislite_free_changeset(cs);
	if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		set_status_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_mset_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	int i;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	if ((params->argc & 1) == 0) {
		reply->str = redislite_malloc(sizeof(char) * (strlen(wrong_arity) + params->argvlen[0] - 1));
		char *str = redislite_malloc(sizeof(char) * (params->argvlen[0] + 1));
		memcpy(str, params->argv[0], params->argvlen[0]);
		str[params->argvlen[0]] = '\0';
		sprintf(reply->str, wrong_arity, str);
		redislite_free(str);
		reply->len = strlen(wrong_arity) + params->argvlen[0] - 2;
		reply->type = REDISLITE_REPLY_ERROR;
		return reply;
	}
	changeset *cs = redislite_create_changeset(db);
	int status = REDISLITE_OK;
	for (i = 1; i < params->argc; i += 2) {
		key = params->argv[i];
		len = params->argvlen[i];
		value = params->argv[i + 1];
		value_len = params->argvlen[i + 1];
		status = redislite_page_string_set_key_string(cs, key, len, value, value_len);
		if (status < 0) {
			break;
		}
	}
	if (status >= 0) {
		status = redislite_save_changeset(cs);
	}
	redislite_free_changeset(cs);
	if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		set_status_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_randomkey_command(redislite *db, redislite_params *params)
{
	params = params; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	char *value;
	size_t value_len;
	int status = redislite_get_random_key_name(db, NULL, &value, &value_len);
	if (status == REDISLITE_NOT_FOUND) {
		reply->type = REDISLITE_REPLY_NIL;
	}
	else if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		reply->type = REDISLITE_REPLY_STRING;
		reply->str = value;
		reply->len = value_len;
	}
	return reply;
}

redislite_reply *redislite_keys_command(redislite *db, redislite_params *params)
{
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	int size, i = 0;
	char **values;
	int *values_len;
	int status = redislite_get_keys(db, NULL, params->argv[1], params->argvlen[1], &size, &values, &values_len);
	if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		reply->type = REDISLITE_REPLY_ARRAY;
		reply->elements = size;
		if (size > 0) {
			reply->element = redislite_malloc(sizeof(redislite_reply *) * size);
			if (reply->element == NULL) {
				goto cleanup;
			}

			for (; i < size; i++) {
				reply->element[i] = redislite_create_reply();
				if (reply->element[i] == NULL) {
					goto cleanup;
				}
				reply->element[i]->type = REDISLITE_REPLY_STRING;
				reply->element[i]->str = values[i];
				reply->element[i]->len = values_len[i];
			}
		}
		else {
			reply->element = NULL;
		}
		redislite_free(values);
		redislite_free(values_len);
	}
	return reply;
cleanup:
	if (reply->element != NULL) {
		for (i--;; i--) {
			redislite_free(reply->element[i]);
			if (i == 0) {
				break;
			}
		}
		redislite_free(reply->element);
	}
	redislite_free(values);
	redislite_free(values_len);
	redislite_free(reply);
	return NULL;
}

redislite_reply *redislite_ping_command(redislite *db, redislite_params *params)
{
	db = db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	params = params; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}

	reply->str = redislite_malloc(sizeof(char) * 5);
	if (reply->str == NULL) {
		redislite_free(reply);
		return NULL;
	}
	memcpy(reply->str, "PONG", sizeof("PONG"));
	reply->type = REDISLITE_REPLY_STATUS;
	reply->len = 4;
	return reply;
}

redislite_reply *redislite_dbsize_command(redislite *db, redislite_params *params)
{
	params = params; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	reply->type = REDISLITE_REPLY_INTEGER;
	reply->integer = ((redislite_page_index_first *)db->root)->number_of_keys;
	return reply;
}

redislite_reply *redislite_del_command(redislite *db, redislite_params *params)
{
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_delete_keys(cs, params->argc - 1, &params->argv[1], &params->argvlen[1]);
	if (status >= 0) {
		int ret = redislite_save_changeset(cs);
		if (ret < 0) {
			status = ret;
		}
	}

	if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		reply->type = REDISLITE_REPLY_INTEGER;
		reply->integer = status;
	}
	redislite_free_changeset(cs);
	return reply;
}

redislite_reply *redislite_exists_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	int status = redislite_value_page_for_key(db, NULL, db->root, key, len, NULL);
	reply->type = REDISLITE_REPLY_INTEGER;
	reply->integer = status >= 0;
	return reply;
}

redislite_reply *redislite_type_command(redislite *db, redislite_params *params)
{
	char type;
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	int status = redislite_page_index_type(db, NULL, db->root, key, len, &type);
	if (status == REDISLITE_NOT_FOUND) {
		reply->str = redislite_malloc(sizeof(char) * 5);
		if (reply->str == NULL) {
			redislite_free(reply);
			return NULL;
		}
		memcpy(reply->str, "none", 5);
		reply->type = REDISLITE_REPLY_STATUS;
		reply->len = 4;
	}
	else if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		switch (type) {
			case REDISLITE_PAGE_TYPE_STRING: {
					reply->str = redislite_malloc(sizeof(char) * 7);
					if (reply->str == NULL) {
						redislite_free(reply);
						return NULL;
					}
					memcpy(reply->str, "string", 7);
					reply->type = REDISLITE_REPLY_STATUS;
					reply->len = 6;
					break;
				}
			case REDISLITE_PAGE_TYPE_FIRST: {
					reply->str = redislite_malloc(sizeof(char) * 4);
					if (reply->str == NULL) {
						redislite_free(reply);
						return NULL;
					}
					memcpy(reply->str, "set", 4);
					reply->type = REDISLITE_REPLY_STATUS;
					reply->len = 3;
					break;
				}
		}

	}
	return reply;
}

redislite_reply *redislite_setnx_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	value = params->argv[2];
	value_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_page_string_setnx_key_string(cs, key, len, value, value_len);
	if (status >= 0) {
		int ret = redislite_save_changeset(cs);
		if (ret < 0) {
			status = ret;
		}
	}
	redislite_free_changeset(cs);
	if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		reply->type = REDISLITE_REPLY_INTEGER;
		reply->integer = status;
	}
	return reply;
}

redislite_reply *redislite_msetnx_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	int i;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	if ((params->argc & 1) == 0) {
		reply->str = redislite_malloc(sizeof(char) * (strlen(wrong_arity) + params->argvlen[0] - 1));
		char *str = redislite_malloc(sizeof(char) * (params->argvlen[0] + 1));
		memcpy(str, params->argv[0], params->argvlen[0]);
		str[params->argvlen[0]] = '\0';
		sprintf(reply->str, wrong_arity, str);
		redislite_free(str);
		reply->len = strlen(wrong_arity) + params->argvlen[0] - 2;
		reply->type = REDISLITE_REPLY_ERROR;
		return reply;
	}
	changeset *cs = redislite_create_changeset(db);

	unsigned char *data = malloc(sizeof(unsigned char) * db->page_size);
	if (data == NULL) {
		return reply;
	}
	redislite_write_first(db, data, db->root);
	int status = REDISLITE_OK;
	for (i = 1; i < params->argc; i += 2) {
		key = params->argv[i];
		len = params->argvlen[i];
		value = params->argv[i + 1];
		value_len = params->argvlen[i + 1];
		status = redislite_page_string_setnx_key_string(cs, key, len, value, value_len);
		if (status <= 0) {
			break;
		}
	}

	if (status > 0) {
		int ret = redislite_save_changeset(cs);
		if (ret < 0) {
			status = ret;
		}
	}
	else {
		db->root = redislite_read_first(db, data);
	}
	free(data);
	redislite_free_changeset(cs);
	if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		reply->type = REDISLITE_REPLY_INTEGER;
		reply->integer = status;
	}
	return reply;
}

redislite_reply *redislite_append_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	value = params->argv[2];
	value_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);
	size_t new_len;
	int status = redislite_page_string_append_key_string(cs, key, len, value, value_len, &new_len);
	if (status != REDISLITE_OK) {
		set_error_message(status, reply);
		redislite_free_changeset(cs);
		return reply;
	}
	status = redislite_save_changeset(cs);
	if (status != REDISLITE_OK) {
		set_error_message(status, reply);
		redislite_free_changeset(cs);
		return reply;
	}
	redislite_free_changeset(cs);
	reply->type = REDISLITE_REPLY_INTEGER;
	reply->integer = (int)new_len;
	return reply;
}

redislite_reply *redislite_setbit_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	long long bit_offset;
	long long on;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	int status = str_to_long_long(params->argv[2], params->argvlen[2], &bit_offset);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_BIT_OFFSET_INVALID;
		}
		set_error_message(status, reply);
		return reply;
	}

	status = str_to_long_long(params->argv[3], params->argvlen[3], &on);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_BIT_INVALID;
		}
		set_error_message(status, reply);
		return reply;
	}
	if (on & ~1) {
		status = REDISLITE_BIT_INVALID;
		set_error_message(status, reply);
		return reply;
	}

	changeset *cs = redislite_create_changeset(db);
	int response = status = redislite_page_string_setbit_key_string(cs, key, len, bit_offset, on);
	if (status < REDISLITE_OK) {
		set_error_message(status, reply);
		return reply;
	}
	status = redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	if (status < REDISLITE_OK) {
		set_error_message(status, reply);
		return reply;
	}
	reply->type = REDISLITE_REPLY_INTEGER;
	reply->integer = response;
	return reply;
}

redislite_reply *redislite_getbit_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	long long bit_offset;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	int status = str_to_long_long(params->argv[2], params->argvlen[2], &bit_offset);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_BIT_OFFSET_INVALID;
		}
		set_error_message(status, reply);
		return reply;
	}

	status = redislite_page_string_getbit_key_string(db, NULL, key, len, bit_offset);
	if (status < REDISLITE_OK) {
		set_error_message(status, reply);
		return reply;
	}
	reply->type = REDISLITE_REPLY_INTEGER;
	reply->integer = status;
	return reply;
}

redislite_reply *redislite_setrange_command(redislite *db, redislite_params *params)
{

	char *key, *value;
	size_t len, value_len;
	long long start;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	int status = str_to_long_long(params->argv[2], params->argvlen[2], &start);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}
	if (start < 0) {
		set_error_message(REDISLITE_EXPECT_INTEGER, reply);
		return reply;
	}
	if (start > 512 * 1024) {
		set_error_message(REDISLITE_MAXIMUM_SIZE, reply);
		return reply;
	}
	value = params->argv[3];
	value_len = params->argvlen[3];

	size_t reply_len = 0;
	changeset *cs = redislite_create_changeset(db);
	status = redislite_page_string_setrange_key_string(cs, key, len, start, value, value_len, &reply_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}
	redislite_free_changeset(cs);
	if (status < REDISLITE_OK) {
		set_error_message(status, reply);
		return reply;
	}
	reply->type = REDISLITE_REPLY_INTEGER;
	reply->integer = (long)reply_len;
	return reply;
}

redislite_reply *redislite_getrange_command(redislite *db, redislite_params *params)
{

	char *key;
	size_t len;
	long long start;
	long long end;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	int status = str_to_long_long(params->argv[2], params->argvlen[2], &start);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}
	status = str_to_long_long(params->argv[3], params->argvlen[3], &end);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}

	size_t reply_len = 0;
	status = redislite_page_string_getrange_key_string(db, NULL, key, len, start, end, &reply->str, &reply_len);
	reply->len = (int)reply_len;
	if (status < REDISLITE_OK) {
		set_error_message(status, reply);
		return reply;
	}
	reply->type = REDISLITE_REPLY_STRING;
	return reply;
}

redislite_reply *redislite_incr_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_page_string_incr_key_string(cs, key, len, &reply->integer);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_INTEGER;
	}
	else {
		set_error_message(status, reply);
	}
	redislite_free_changeset(cs);
	return reply;
}

redislite_reply *redislite_decr_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_page_string_decr_key_string(cs, key, len, &reply->integer);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_INTEGER;
	}
	else {
		set_error_message(status, reply);
	}
	redislite_free_changeset(cs);
	return reply;
}

redislite_reply *redislite_incrbyfloat_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);

	char *eptr;
	long double incr = strtold(params->argv[2], &eptr);
	if (isspace(((char *)params->argv[2])[0]) || eptr[0] != '\0' || isnan(incr)) {
		set_error_message(REDISLITE_EXPECT_DOUBLE, reply);
		return reply;
	}
	int status = redislite_page_string_incrbyfloat_by_key_string(cs, key, len, incr, &reply->str, &reply->len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_STRING;
	}
	else {
		set_error_message(status, reply);
	}
	redislite_free_changeset(cs);
	return reply;
}

redislite_reply *redislite_incrby_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);
	long long incr;
	int status = str_to_long_long(params->argv[2], params->argvlen[2], &incr);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}
	status = redislite_page_string_incr_by_key_string(cs, key, len, incr, &reply->integer);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_INTEGER;
	}
	else {
		set_error_message(status, reply);
	}
	redislite_free_changeset(cs);
	return reply;
}


redislite_reply *redislite_rename_command(redislite *db, redislite_params *params)
{
	char *src, *target;
	size_t src_len, target_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	src = params->argv[1];
	src_len = params->argvlen[1];
	target = params->argv[2];
	target_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);

	int status = redislite_page_index_rename_key(cs, cs->db->root, src, src_len, target, target_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status == REDISLITE_OK) {
		set_status_message(status, reply);
	}
	else if (status == REDISLITE_NOT_FOUND) {
		reply->type = REDISLITE_REPLY_ERROR;
		reply->str = redislite_malloc(strlen(key_not_found) + 1);
		memcpy(reply->str, key_not_found, strlen(key_not_found) + 1);
		reply->len = strlen(key_not_found) + 1;
	}
	else {
		set_error_message(status, reply);
	}
	redislite_free_changeset(cs);
	return reply;
}

redislite_reply *redislite_renamenx_command(redislite *db, redislite_params *params)
{
	char *src, *target;
	size_t src_len, target_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	src = params->argv[1];
	src_len = params->argvlen[1];
	target = params->argv[2];
	target_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);

	int status = redislite_page_index_renamenx_key(cs, cs->db->root, src, src_len, target, target_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status == REDISLITE_OK) {
		reply->integer = 1;
		reply->type = REDISLITE_REPLY_INTEGER;
	}
	else if (status == REDISLITE_NOT_FOUND) {
		reply->type = REDISLITE_REPLY_ERROR;
		reply->str = redislite_malloc(strlen(key_not_found) + 1);
		memcpy(reply->str, key_not_found, strlen(key_not_found) + 1);
		reply->len = strlen(key_not_found) + 1;
	}
	else if (status == REDISLITE_ALREADY_EXISTS) {
		reply->integer = 0;
		reply->type = REDISLITE_REPLY_INTEGER;
	}
	else {
		set_error_message(status, reply);
	}
	redislite_free_changeset(cs);
	return reply;
}

redislite_reply *redislite_decrby_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);
	long long decr;
	int status = str_to_long_long(params->argv[2], params->argvlen[2], &decr);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}
	status = redislite_page_string_decr_by_key_string(cs, key, len, decr, &reply->integer);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_INTEGER;
	}
	else {
		set_error_message(status, reply);
	}
	redislite_free_changeset(cs);
	return reply;
}

redislite_reply *redislite_llen_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	size_t llen = 0;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	int status = redislite_llen_by_keyname(db, NULL, key, len, &llen);
	if (status == REDISLITE_OK || status == REDISLITE_NOT_FOUND) {
		reply->type = REDISLITE_REPLY_INTEGER;
		reply->integer = llen;
	}
	else {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_lpush_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);
	int i, status = REDISLITE_OK;
	for (i = 2; i < params->argc; i++) {
		value = params->argv[i];
		value_len = params->argvlen[i];
		status = redislite_lpush_by_keyname(cs, key, len, value, value_len);
		if (status != REDISLITE_OK) {
			break;
		}
	}
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}
	redislite_free_changeset(cs);

	if (status != REDISLITE_OK) {
		redislite_reply *reply = redislite_create_reply();
		if (reply == NULL) {
			return NULL;
		}
		set_error_message(status, reply);
		return reply;
	}
	return redislite_llen_command(db, params);
}

redislite_reply *redislite_rpush_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);
	int i, status = REDISLITE_OK;
	for (i = 2; i < params->argc; i++) {
		value = params->argv[i];
		value_len = params->argvlen[i];
		status = redislite_rpush_by_keyname(cs, key, len, value, value_len);
		if (status != REDISLITE_OK) {
			break;
		}
	}
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status != REDISLITE_OK) {
		redislite_reply *reply = redislite_create_reply();
		if (reply == NULL) {
			return NULL;
		}
		set_error_message(status, reply);
		return reply;
	}
	redislite_free_changeset(cs);
	return redislite_llen_command(db, params);
}

redislite_reply *redislite_rpushx_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	key = params->argv[1];
	len = params->argvlen[1];
	value = params->argv[2];
	value_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_rpushx_by_keyname(cs, key, len, value, value_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status != REDISLITE_OK) {
		redislite_reply *reply = redislite_create_reply();
		if (reply == NULL) {
			return NULL;
		}
		set_error_message(status, reply);
		return reply;
	}
	redislite_free_changeset(cs);
	return redislite_llen_command(db, params);
}

redislite_reply *redislite_lpushx_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	key = params->argv[1];
	len = params->argvlen[1];
	value = params->argv[2];
	value_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_lpushx_by_keyname(cs, key, len, value, value_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	if (status != REDISLITE_OK) {
		redislite_reply *reply = redislite_create_reply();
		if (reply == NULL) {
			return NULL;
		}
		set_error_message(status, reply);
		return reply;
	}
	redislite_free_changeset(cs);
	return redislite_llen_command(db, params);
}

redislite_reply *redislite_rpop_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_rpop_by_keyname(cs, key, len, &value, &value_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	redislite_free_changeset(cs);
	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_STRING;
		reply->str = value;
		reply->len = value_len;
	}
	else if (status != REDISLITE_NOT_FOUND) {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_rpoplpush_command(redislite *db, redislite_params *params)
{
	char *source, *destination, *value;
	size_t source_len, destination_len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	source = params->argv[1];
	source_len = params->argvlen[1];
	destination = params->argv[2];
	destination_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_rpoplpush_by_keyname(cs, source, source_len, destination, destination_len, &value, &value_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	redislite_free_changeset(cs);
	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_STRING;
		reply->str = value;
		reply->len = value_len;
	}
	else if (status != REDISLITE_NOT_FOUND) {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_lpop_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_lpop_by_keyname(cs, key, len, &value, &value_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	redislite_free_changeset(cs);
	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_STRING;
		reply->str = value;
		reply->len = value_len;
	}
	else if (status != REDISLITE_NOT_FOUND) {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_lrange_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	int status;
	long long start, end;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];

	status = str_to_long_long(params->argv[2], params->argvlen[2], &start);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}
	status = str_to_long_long(params->argv[3], params->argvlen[3], &end);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}

	size_t size;
	char **values = NULL;
	size_t *values_len = NULL;
	status = redislite_lrange_by_keyname(db, NULL, key, len, start, end, &size, &values, &values_len);
	size_t i = 0;
	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_ARRAY;
		reply->elements = size;
		reply->element = redislite_malloc(sizeof(redislite_reply *) * size);
		if (reply->element == NULL) {
			goto cleanup;
		}

		for (; i < size; i++) {
			reply->element[i] = redislite_create_reply();
			if (reply->element[i] == NULL) {
				goto cleanup;
			}
			reply->element[i]->type = REDISLITE_REPLY_STRING;
			reply->element[i]->str = values[i];
			reply->element[i]->len = values_len[i];
		}
		if (values) {
			redislite_free(values);
		}
		if (values_len) {
			redislite_free(values_len);
		}
	}
	else if (status != REDISLITE_NOT_FOUND) {
		set_error_message(status, reply);
	}
	return reply;
cleanup:
	if (reply->element != NULL) {
		for (i--;; i--) {
			redislite_free(reply->element[i]);
			if (i == 0) {
				break;
			}
		}
		redislite_free(reply->element);
	}
	redislite_free(values);
	redislite_free(values_len);
	redislite_free(reply);
	return NULL;
}

redislite_reply *redislite_lindex_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	long long pos;

	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}

	int status = str_to_long_long(params->argv[2], params->argvlen[2], &pos);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}

	key = params->argv[1];
	len = params->argvlen[1];

	status = redislite_lindex_by_keyname(db, NULL, key, len, (int)pos, &value, &value_len);
	if (status == REDISLITE_OK) {
		if (value_len > 0) {
			reply->type = REDISLITE_REPLY_STRING;
			reply->str = value;
			reply->len = value_len;
		}
	}
	else {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_lset_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	long long pos;

	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}

	int status = str_to_long_long(params->argv[2], params->argvlen[2], &pos);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}

	key = params->argv[1];
	len = params->argvlen[1];
	value = params->argv[3];
	value_len = params->argvlen[3];

	changeset *cs = redislite_create_changeset(db);
	status = redislite_lset_by_keyname(cs, key, len, (int)pos, value, value_len);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	redislite_free_changeset(cs);
	if (status == REDISLITE_OK) {
		set_status_message(status, reply);
	}
	else {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_ltrim_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len;
	long long start, end;

	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}

	int status = str_to_long_long(params->argv[2], params->argvlen[2], &start);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}

	status = str_to_long_long(params->argv[3], params->argvlen[3], &end);
	if (status != REDISLITE_OK) {
		if (status == REDISLITE_ERR) {
			status = REDISLITE_EXPECT_INTEGER;
		}
		set_error_message(status, reply);
		return reply;
	}

	key = params->argv[1];
	len = params->argvlen[1];

	changeset *cs = redislite_create_changeset(db);
	status = redislite_ltrim_by_keyname(cs, key, len, (int)start, (int)end);
	if (status == REDISLITE_OK) {
		status = redislite_save_changeset(cs);
	}

	redislite_free_changeset(cs);
	if (status == REDISLITE_OK) {
		set_status_message(status, reply);
	}
	else {
		set_error_message(status, reply);
	}
	return reply;
}

redislite_reply *redislite_linsert_command(redislite *db, redislite_params *params)
{
	char *key, *value, *pivot;
	size_t len, value_len, pivot_len;
	int after, status;

	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}

	if (params->argvlen[2] == 5 && strcasecmp(params->argv[2], "after") == 0) {
		after = 1;
	}
	else if (params->argvlen[2] == 6 && strcasecmp(params->argv[2], "before") == 0) {
		after = 0;
	}
	else {
		set_error_message(REDISLITE_SYNTAX_ERROR, reply);
		return reply;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	pivot = params->argv[3];
	pivot_len = params->argvlen[3];
	value = params->argv[4];
	value_len = params->argvlen[4];

	changeset *cs = redislite_create_changeset(db);
	reply->integer = redislite_linsert_by_keyname(cs, key, len, after, pivot, pivot_len, value, value_len);
	if (reply->integer >= 0) {
		status = redislite_save_changeset(cs);
	}
	else if (reply->integer == REDISLITE_NOT_FOUND) {
		status = REDISLITE_OK;
		reply->integer = -1;
	}
	else {
		status = reply->integer;
	}

	redislite_free_changeset(cs);
	if (status == REDISLITE_OK) {
		reply->type = REDISLITE_REPLY_INTEGER;
	}
	else {
		set_error_message(status, reply);
	}
	return reply;
}
static redislite_reply *init_multibulk(size_t size)
{
	size_t i, j;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	reply->type = REDISLITE_REPLY_ARRAY;
	reply->elements = size;
	reply->element = redislite_malloc(sizeof(redislite_reply *) * (size));
	if (reply->element == NULL) {
		redislite_free(reply);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		reply->element[i] = redislite_malloc(sizeof(redislite_reply));
		if (reply->element[i] == NULL) {
			for (j = i - 1;; j--) {
				redislite_free(reply->element[j]);
				if (j == 0) {
					break;
				}
			}
			redislite_free(reply->element);
			redislite_free(reply);
			return NULL;
		}
	}
	return reply;
}

redislite_reply *redislite_mget_command(redislite *db, redislite_params *params)
{
	char *key;
	size_t len, reply_len;
	int i, status;
	redislite_reply *reply = init_multibulk(params->argc - 1);
	if (reply == NULL) {
		return NULL;
	}

	for (i = 1; i < params->argc; i++) {
		key = params->argv[i];
		len = params->argvlen[i];
		reply_len = 0;
		status = redislite_page_string_get_by_keyname(db, NULL, key, len, &reply->element[i - 1]->str, &reply_len);
		reply->element[i - 1]->len = (int)reply_len;
		if (status == REDISLITE_OK) {
			reply->element[i - 1]->type = REDISLITE_REPLY_STRING;
		}
		else {
			reply->element[i - 1]->type = REDISLITE_REPLY_NIL;
		}
	}
	return reply;
}


redislite_reply *redislite_flushall_command(redislite *db, redislite_params *params)
{
	params = params; // XXX: avoid unused-parameter warning; we are implementing a prototype
	changeset *cs = redislite_create_changeset(db);

	redislite_reply *reply = redislite_create_reply();
	int status = redislite_flush(cs);
	if (status < 0) {
		set_error_message(status, reply);
		goto cleanup;
	}
	status = redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	set_status_message(status, reply);
cleanup:
	return reply;
}

redislite_reply *redislite_info_command(redislite *db, redislite_params *params)
{
	params = params; // XXX: avoid unused-parameter warning; we are implementing a prototype
	db = db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_reply *reply = redislite_create_reply();
	reply->type = REDISLITE_REPLY_STRING;
	const char *format =
	    "redislite_version:%s\r\n"
	    "redislite_git_sha1:%s\r\n"
	    "redislite_git_dirty:%d\r\n";
	int len = strlen(format) + 1 /* NULL-terminated */ + 1 /* version */ + 6 /* sha */ - 1 /* dirty */;

	reply->str = redislite_malloc(sizeof(char) * len);
	sprintf(reply->str, format,
	        REDISLITE_VERSION,
	        redislite_git_SHA1(),
	        strtol(redislite_git_dirty(), NULL, 10) > 0
	       );

	reply->len = len - 1;
	reply->str[len - 1] = '\0';
	return reply;
}

redislite_reply *redislite_sadd_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	value = params->argv[2];
	value_len = params->argvlen[2];
	changeset *cs = redislite_create_changeset(db);
	int status = redislite_page_set_add(cs, key, len, value, value_len);
	if (status >= 0) {
		int _status = redislite_save_changeset(cs);
		if (_status < 0) {
			status = _status;
		}
	}
	redislite_free_changeset(cs);
	if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		reply->type = REDISLITE_REPLY_INTEGER;
		reply->integer = status;
	}
	return reply;
}

redislite_reply *redislite_sismember_command(redislite *db, redislite_params *params)
{
	char *key, *value;
	size_t len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) {
		return NULL;
	}
	key = params->argv[1];
	len = params->argvlen[1];
	value = params->argv[2];
	value_len = params->argvlen[2];
	int status = redislite_page_set_contains(db, NULL, key, len, value, value_len);
	if (status < 0) {
		set_error_message(status, reply);
	}
	else {
		reply->type = REDISLITE_REPLY_INTEGER;
		reply->integer = status;
	}
	return reply;
}

redislite_reply *redislite_command_not_implemented_yet(redislite *db, redislite_params *params)
{
	params = params; // XXX: avoid unused-parameter warning; we are implementing a prototype
	db = db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_reply *reply = redislite_create_reply();
	set_error_message(REDISLITE_NOT_IMPLEMENTED_YET, reply);
	return reply;
}

redislite_reply *redislite_command_implementation_not_planned(redislite *db, redislite_params *params)
{
	params = params; // XXX: avoid unused-parameter warning; we are implementing a prototype
	db = db; // XXX: avoid unused-parameter warning; we are implementing a prototype
	redislite_reply *reply = redislite_create_reply();
	set_error_message(REDISLITE_IMPLEMENTATION_NOT_PLANNED, reply);
	return reply;
}

struct redislite_command redislite_command_table[] = {
	{"get", redislite_get_command, 2, 0},
	{"set", redislite_set_command, 3, 0},
	{"setnx", redislite_setnx_command, 3, 0},
	{"setex", redislite_command_implementation_not_planned, 4, 0},
	{"append", redislite_append_command, 3, 0},
	{"strlen", redislite_strlen_command, 2, 0},
	{"del", redislite_del_command, -2, 0},
	{"exists", redislite_exists_command, 2, 0},
	{"setbit", redislite_setbit_command, 4, 0},
	{"getbit", redislite_getbit_command, 3, 0},
	{"setrange", redislite_setrange_command, 4, 0},
	{"getrange", redislite_getrange_command, 4, 0},
	{"substr", redislite_getrange_command, 4, 0},
	{"incr", redislite_incr_command, 2, 0},
	{"decr", redislite_decr_command, 2, 0},
	{"mget", redislite_mget_command, -2, 0},
	{"rpush", redislite_rpush_command, -3, 0},
	{"lpush", redislite_lpush_command, -3, 0},
	{"rpushx", redislite_rpushx_command, 3, 0},
	{"lpushx", redislite_lpushx_command, 3, 0},
	{"linsert", redislite_linsert_command, 5, 0},
	{"rpop", redislite_rpop_command, 2, 0},
	{"lpop", redislite_lpop_command, 2, 0},
	{"brpop", redislite_command_implementation_not_planned, 3, 0},
	{"brpoplpush", redislite_command_implementation_not_planned, 4, 0},
	{"blpop", redislite_command_implementation_not_planned, 3, 0},
	{"llen", redislite_llen_command, 2, 0},
	{"lindex", redislite_lindex_command, 3, 0},
	{"lset", redislite_lset_command, 4, 0},
	{"lrange", redislite_lrange_command, 4, 0},
	{"ltrim", redislite_ltrim_command, 4, 0},
	{"lrem", redislite_command_not_implemented_yet, 4, 0},
	{"rpoplpush", redislite_rpoplpush_command, 3, 0},
	{"sadd", redislite_sadd_command, 3, 0},
	{"srem", redislite_command_not_implemented_yet, 3, 0},
	{"smove", redislite_command_implementation_not_planned, 4, 0},
	{"sismember", redislite_sismember_command, 3, 0},
	{"scard", redislite_command_not_implemented_yet, 2, 0},
	{"spop", redislite_command_not_implemented_yet, 2, 0},
	{"srandmember", redislite_command_not_implemented_yet, 2, 0},
	{"sinter", redislite_command_not_implemented_yet, 2, 0},
	{"sinterstore", redislite_command_not_implemented_yet, 3, 0},
	{"sunion", redislite_command_not_implemented_yet, 2, 0},
	{"sunionstore", redislite_command_not_implemented_yet, 3, 0},
	{"sdiff", redislite_command_not_implemented_yet, 2, 0},
	{"sdiffstore", redislite_command_not_implemented_yet, 3, 0},
	{"smembers", redislite_command_not_implemented_yet, 2, 0},
	{"zadd", redislite_command_not_implemented_yet, 4, 0},
	{"zincrby", redislite_command_not_implemented_yet, 4, 0},
	{"zrem", redislite_command_not_implemented_yet, 3, 0},
	{"zremrangebyscore", redislite_command_not_implemented_yet, 4, 0},
	{"zremrangebyrank", redislite_command_not_implemented_yet, 4, 0},
	{"zunionstore", redislite_command_not_implemented_yet, 4, 0},
	{"zinterstore", redislite_command_not_implemented_yet, 4, 0},
	{"zrange", redislite_command_not_implemented_yet, 4, 0},
	{"zrangebyscore", redislite_command_not_implemented_yet, 4, 0},
	{"zrevrangebyscore", redislite_command_not_implemented_yet, 4, 0},
	{"zcount", redislite_command_not_implemented_yet, 4, 0},
	{"zrevrange", redislite_command_not_implemented_yet, 4, 0},
	{"zcard", redislite_command_not_implemented_yet, 2, 0},
	{"zscore", redislite_command_not_implemented_yet, 3, 0},
	{"zrank", redislite_command_not_implemented_yet, 3, 0},
	{"zrevrank", redislite_command_not_implemented_yet, 3, 0},
	{"hset", redislite_command_not_implemented_yet, 4, 0},
	{"hsetnx", redislite_command_not_implemented_yet, 4, 0},
	{"hget", redislite_command_not_implemented_yet, 3, 0},
	{"hmset", redislite_command_not_implemented_yet, 4, 0},
	{"hmget", redislite_command_not_implemented_yet, 3, 0},
	{"hincrby", redislite_command_not_implemented_yet, 4, 0},
	{"hdel", redislite_command_not_implemented_yet, 3, 0},
	{"hvals", redislite_command_not_implemented_yet, 2, 0},
	{"hgetall", redislite_command_not_implemented_yet, 2, 0},
	{"hexists", redislite_command_not_implemented_yet, 3, 0},
	{"incrby", redislite_incrby_command, 3, 0},
	{"decrby", redislite_decrby_command, 3, 0},
	{"getset", redislite_getset_command, 3, 0},
	{"mset", redislite_mset_command, -3, 0},
	{"msetnx", redislite_msetnx_command, -3, 0},
	{"randomkey", redislite_randomkey_command, 1, 0},
	{"select", redislite_command_implementation_not_planned, 2, 0},
	{"move", redislite_command_implementation_not_planned, 3, 0},
	{"rename", redislite_rename_command, 3, 0},
	{"renamenx", redislite_renamenx_command, 3, 0},
	{"expire", redislite_command_implementation_not_planned, 3, 0},
	{"expireat", redislite_command_implementation_not_planned, 3, 0},
	{"keys", redislite_keys_command, 2, 0},
	{"dbsize", redislite_dbsize_command, 1, 0},
	{"auth", redislite_command_implementation_not_planned, 2, 0},
	{"ping", redislite_ping_command, 1, 0},
	{"echo", redislite_command_not_implemented_yet, 2, 0},
	{"save", redislite_command_implementation_not_planned, 1, 0},
	{"bgsave", redislite_command_implementation_not_planned, 1, 0},
	{"bgrewriteaof", redislite_command_implementation_not_planned, 1, 0},
	{"shutdown", redislite_command_implementation_not_planned, 1, 0},
	{"lastsave", redislite_command_implementation_not_planned, 1, 0},
	{"type", redislite_type_command, 2, 0},
	{"multi", redislite_command_not_implemented_yet, 1, 0},
	{"exec", redislite_command_not_implemented_yet, 1, 0},
	{"discard", redislite_command_not_implemented_yet, 1, 0},
	{"sync", redislite_command_implementation_not_planned, 1, 0},
	{"flushdb", redislite_command_implementation_not_planned, 1, 0},
	{"flushall", redislite_flushall_command, 1, 0},
	{"sort", redislite_command_not_implemented_yet, 2, 0},
	{"info", redislite_info_command, 1, 0},
	{"monitor", redislite_command_implementation_not_planned, 1, 0},
	{"ttl", redislite_command_implementation_not_planned, 2, 0},
	{"persist", redislite_command_implementation_not_planned, 2, 0},
	{"slaveof", redislite_command_implementation_not_planned, 3, 0},
	{"debug", redislite_command_implementation_not_planned, 2, 0},
	{"config", redislite_command_implementation_not_planned, 2, 0},
	{"subscribe", redislite_command_implementation_not_planned, 2, 0},
	{"unsubscribe", redislite_command_implementation_not_planned, 1, 0},
	{"psubscribe", redislite_command_implementation_not_planned, 2, 0},
	{"punsubscribe", redislite_command_implementation_not_planned, 1, 0},
	{"publish", redislite_command_implementation_not_planned, 3, 0},
	{"watch", redislite_command_not_implemented_yet, 2, 0},
	{"unwatch", redislite_command_not_implemented_yet, 1, 0},
	{"incrbyfloat", redislite_incrbyfloat_command, 3, 0}
};

static int memcaseequal(const char *str1, const char *str2, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++) {
		if (str1[i] == str2[i]) {
			continue;
		}
		if (str1[i] < str2[i]) {
			if (str1[i] < 'A' || str1[i] > 'Z') {
				return 0;
			}
			if (str2[i] < 'a' || str2[i] > 'z') {
				return 0;
			}
			if (str1[i] + 'a' - 'A' != str2[i]) {
				return 0;
			}
		}
		if (str1[i] > str2[i]) {
			if (str2[i] < 'A' || str2[i] > 'Z') {
				return 0;
			}
			if (str1[i] < 'a' || str1[i] > 'z') {
				return 0;
			}
			if (str1[i] + 'A' - 'a' != str2[i]) {
				return 0;
			}
		}
	}
	return 1;
}

struct redislite_command *redislite_command_lookup(char *command, size_t length) {
	if (length < 2) {
		return NULL;
	}
	//redislite_command_table
	if (command[0] > 122 || command[0] < 65) {
		return NULL;
	}
	if (command[1] > 122 || command[1] < 65) {
		return NULL;
	}
	if (command[2] > 122 || command[2] < 65) {
		return NULL;
	}
	int sum = (int)command[0] + (int)command[1] + (int)command[2];
	if (command[0] > 90) {
		sum += 'A' - 'a';
	}
	if (command[1] > 90) {
		sum += 'A' - 'a';
	}
	if (command[2] > 90) {
		sum += 'A' - 'a';
	}
	switch (sum) {

		case 204: // 'D'+'E'+'C'
			if (length == 4 && memcaseequal(command, "decr", 4)) {
				return &redislite_command_table[14];
			}
			if (length == 6 && memcaseequal(command, "decrby", 6)) {
				return &redislite_command_table[74];
			}
			break;

		case 213: // 'D'+'E'+'L'
			if (length == 3 && memcaseequal(command, "del", 3)) {
				return &redislite_command_table[6];
			}
			break;

		case 216: // 'S'+'A'+'D'
			if (length == 4 && memcaseequal(command, "sadd", 4)) {
				return &redislite_command_table[33];
			}
			break;

		case 217: // 'M'+'G'+'E'
			// 'D'+'B'+'S'
			if (length == 4 && memcaseequal(command, "mget", 4)) {
				return &redislite_command_table[15];
			}
			else if (length == 6 && memcaseequal(command, "dbsize", 6)) {
				return &redislite_command_table[86];
			}
			break;

		case 218: // 'I'+'N'+'C'
			if (length == 4 && memcaseequal(command, "incr", 4)) {
				return &redislite_command_table[13];
			}
			if (length == 6 && memcaseequal(command, "incrby", 6)) {
				return &redislite_command_table[73];
			}
			if (length == 11 && memcaseequal(command, "incrbyfloat", 11)) {
				return &redislite_command_table[117];
			}
			break;

		case 221: // 'L'+'L'+'E'
			// 'I'+'N'+'F'
			if (length == 4 && memcaseequal(command, "llen", 4)) {
				return &redislite_command_table[26];
			}
			else if (length == 4 && memcaseequal(command, "info", 4)) {
				return &redislite_command_table[103];
			}
			break;

		case 223: // 'L'+'R'+'A'
			if (length == 6 && memcaseequal(command, "lrange", 6)) {
				return &redislite_command_table[29];
			}
			break;

		case 224: // 'G'+'E'+'T'
			if (length == 3 && memcaseequal(command, "get", 3)) {
				return &redislite_command_table[0];
			}
			else if (length == 6 && memcaseequal(command, "getbit", 6)) {
				return &redislite_command_table[9];
			}
			else if (length == 6 && memcaseequal(command, "getset", 6)) {
				return &redislite_command_table[75];
			}
			else if (length == 8 && memcaseequal(command, "getrange", 8)) {
				return &redislite_command_table[11];
			}
			break;

		case 225: // 'A'+'P'+'P'
			// 'R' + 'A' + 'N'
			if (length == 6 && memcaseequal(command, "append", 6)) {
				return &redislite_command_table[4];
			}
			else if (length == 9 && memcaseequal(command, "randomkey", 9)) {
				return &redislite_command_table[78];
			}
			break;

		case 227: // 'L'+'I'+'N'
			if (length == 6 && memcaseequal(command, "lindex", 6)) {
				return &redislite_command_table[27];
			}
			else if (length == 7 && memcaseequal(command, "linsert", 7)) {
				return &redislite_command_table[20];
			}
			break;

		case 228: // 'L'+'S'+'E'
			if (length == 4 && memcaseequal(command, "lset", 4)) {
				return &redislite_command_table[28];
			}
			break;

		case 229: // 'R'+'E'+'N'
			// 'M'+'S'+'E'
			if (length == 6 && memcaseequal(command, "rename", 6)) {
				return &redislite_command_table[81];
			}
			else if (length == 8 && memcaseequal(command, "renamenx", 8)) {
				return &redislite_command_table[82];
			}
			else if (length == 4 && memcaseequal(command, "mset", 4)) {
				return &redislite_command_table[76];
			}
			else if (length == 6 && memcaseequal(command, "msetnx", 6)) {
				return &redislite_command_table[77];
			}
			break;

		case 230: // 'E'+'X'+'I'
			if (length == 6 && memcaseequal(command, "exists", 6)) {
				return &redislite_command_table[7];
			}
			break;

		case 231: // 'P'+'I'+'N'
			// 'F'+'L'+'U'
			if (length == 4 && memcaseequal(command, "ping", 4)) {
				return &redislite_command_table[88];
			}
			else if (length == 8 && memcaseequal(command, "flushall", 8)) {
				return &redislite_command_table[101];
			}
			break;

		case 233: // 'K'+'E'+'Y'
			if (length == 4 && memcaseequal(command, "keys", 4)) {
				return &redislite_command_table[85];
			}
			break;

		case 234: // 'S'+'U'+'B'
			if (length == 6 && memcaseequal(command, "substr", 6)) {
				return &redislite_command_table[11];
			}
			break;

		case 235: // 'L'+'P'+'O'
			if (length == 4 && memcaseequal(command, "lpop", 4)) {
				return &redislite_command_table[22];
			}
			break;

		case 236: // 'S'+'E'+'T'
			if (length == 3 && memcaseequal(command, "set", 3)) {
				return &redislite_command_table[1];
			}
			else if (length == 8 && memcaseequal(command, "setrange", 8)) {
				return &redislite_command_table[10];
			}
			else if (length == 6 && memcaseequal(command, "setbit", 6)) {
				return &redislite_command_table[8];
			}
			else if (length == 5 && memcaseequal(command, "setnx", 5)) {
				return &redislite_command_table[2];
			}
			break;

		case 239: // 'S'+'I'+'S'
			if (length == 9 && memcaseequal(command, "sismember", 9)) {
				return &redislite_command_table[36];
			}
			break;

		case 241: // 'L'+'P'+'U'
			// 'R'+'P'+'O'
			if (length == 5 && memcaseequal(command, "lpush", 5)) {
				return &redislite_command_table[17];
			}
			else if (length == 6 && memcaseequal(command, "lpushx", 6)) {
				return &redislite_command_table[19];
			}
			else if (length == 4 && memcaseequal(command, "rpop", 4)) {
				return &redislite_command_table[21];
			}
			else if (length == 9 && memcaseequal(command, "rpoplpush", 9)) {
				return &redislite_command_table[32];
			}
			break;

		case 242: // 'L'+'T'+'R'
			if (length == 5 && memcaseequal(command, "ltrim", 5)) {
				return &redislite_command_table[30];
			}
			break;

		case 247: // 'R'+'P'+'U'
			if (length == 5 && memcaseequal(command, "rpush", 5)) {
				return &redislite_command_table[16];
			}
			else if (length == 6 && memcaseequal(command, "rpushx", 6)) {
				return &redislite_command_table[18];
			}
			break;

		case 249: // 'S'+'T'+'R'
			if (length == 6 && memcaseequal(command, "strlen", 6)) {
				return &redislite_command_table[5];
			}
			break;

		case 253: // 'T'+'Y'+'P'
			if (length == 4 && memcaseequal(command, "type", 4)) {
				return &redislite_command_table[95];
			}
			break;
	}
	return NULL;
}

/* Helper function for redislitev_format_command(). */
static int add_argument(redislite_params *target, char *str, size_t len)
{
	if (target->argc == 0) {
		target->argvlen = redislite_malloc(sizeof(char *) * 1);
		if (target->argvlen == NULL) {
			redislite_free_params(target);
			return REDISLITE_OOM;
		}
		target->argv = redislite_malloc(sizeof(char *) * 1);
		if (target->argv == NULL) {
			redislite_free_params(target);
			return REDISLITE_OOM;
		}
	}
	else {
		char **argv = redislite_realloc(target->argv, sizeof(redislite_params *) * (target->argc + 1));
		if (argv == NULL) {
			redislite_free_params(target);
			return REDISLITE_OOM;
		}
		target->argv = argv;
		size_t *argvlen = redislite_realloc(target->argvlen, sizeof(size_t) * (target->argc + 1));
		if (argvlen == NULL) {
			redislite_free_params(target);
			return REDISLITE_OOM;
		}
		target->argvlen = argvlen;
	}
	target->argv[target->argc] = redislite_malloc(sizeof(char) * len);
	if (target->argv[target->argc] == NULL) {
		redislite_free_params(target);
		return REDISLITE_OOM;
	}
	memcpy(target->argv[target->argc], str, len);
	target->argvlen[target->argc++] = len;
	return REDISLITE_OK;
}

int redislitev_format_command(redislite_params **target, const char *format, va_list ap)
{
	size_t size;
	const char *arg, *c = format;
	redislite_params *cmd = NULL; /* final command */
	sds current; /* current argument */
	int interpolated = 0; /* did we do interpolation on an argument? */
	int totlen = 0;

	/* Abort if there is not target to set */
	if (target == NULL) {
		return -1;
	}

	/* Build the command string accordingly to protocol */
	cmd = redislite_create_params();
	cmd->must_free_argv = 1;
	current = sdsempty();
	if (current == NULL) {
		return REDISLITE_OOM;
	}
	while(*c != '\0') {
		if (*c != '%' || c[1] == '\0') {
			if (*c == ' ') {
				if (sdslen(current) != 0) {
					add_argument(cmd, current, sdslen(current));
					sdsfree(current);
					current = sdsempty();
					interpolated = 0;
				}
			}
			else {
				current = sdscatlen(current, c, 1);
			}
		}
		else {
			switch(c[1]) {
				case 's':
					arg = va_arg(ap, char *);
					size = strlen(arg);
					if (size > 0) {
						current = sdscatlen(current, arg, size);
					}
					interpolated = 1;
					break;
				case 'b':
					arg = va_arg(ap, char *);
					size = va_arg(ap, size_t);
					if (size > 0) {
						current = sdscatlen(current, arg, size);
					}
					interpolated = 1;
					break;
				case '%':
					current = sdscat(current, "%");
					break;
				default:
					/* Try to detect printf format */
					{
						char _format[16];
						const char *_p = c + 1;
						size_t _l = 0;
						va_list _cpy;

						/* Flags */
						if (*_p != '\0' && *_p == '#') {
							_p++;
						}
						if (*_p != '\0' && *_p == '0') {
							_p++;
						}
						if (*_p != '\0' && *_p == '-') {
							_p++;
						}
						if (*_p != '\0' && *_p == ' ') {
							_p++;
						}
						if (*_p != '\0' && *_p == '+') {
							_p++;
						}

						/* Field width */
						while (*_p != '\0' && isdigit(*_p)) {
							_p++;
						}

						/* Precision */
						if (*_p == '.') {
							_p++;
							while (*_p != '\0' && isdigit(*_p)) {
								_p++;
							}
						}

						/* Modifiers */
						if (*_p != '\0') {
							if (*_p == 'h' || *_p == 'l') {
								/* Allow a single repetition for these modifiers */
								if (_p[0] == _p[1]) {
									_p++;
								}
								_p++;
							}
						}

						/* Conversion specifier */
						if (*_p != '\0' && strchr("diouxXeEfFgGaA", *_p) != NULL) {
							_l = (_p + 1) - c;
							if (_l < sizeof(_format) - 2) {
								memcpy(_format, c, _l);
								_format[_l] = '\0';
								va_copy(_cpy, ap);
								current = sdscatvprintf(current, _format, _cpy);
								interpolated = 1;
								va_end(_cpy);

								/* Update current position (note: outer blocks
								 * increment c twice so compensate here) */
								c = _p - 1;
							}
						}

						/* Consume and discard vararg */
						va_arg(ap, void);
					}
			}
			c++;
		}
		c++;
	}

	/* Add the last argument if needed */
	if (interpolated || sdslen(current) != 0) {
		add_argument(cmd, current, sdslen(current));
	}
	sdsfree(current);

	*target = cmd;
	return totlen;
}

/* Format a command according to the Redis protocol. This function
 * takes a format similar to printf:
 *
 * %s represents a C null terminated string you want to interpolate
 * %b represents a binary safe string
 *
 * When using %b you need to provide both the pointer to the string
 * and the length in bytes. Examples:
 *
 * len = redislite_format_command(target, "GET %s", mykey);
 * len = redislite_format_command(target, "SET %s %b", mykey, myval, myvallen);
 */
int redislite_format_command(redislite_params **target, const char *format, ...)
{
	va_list ap;
	int len;
	va_start(ap, format);
	len = redislitev_format_command(target, format, ap);
	va_end(ap);
	return len;
}

/* Format a command according to the Redis protocol. This function takes the
 * number of arguments, an array with arguments and an array with their
 * lengths. If the latter is set to NULL, strlen will be used to compute the
 * argument lengths.
 */
int redislite_format_command_argv(char **target, int argc, const char **argv, const size_t *argvlen)
{
	char *cmd = NULL; /* final command */
	int pos; /* position in final command */
	size_t len;
	int totlen, j;

	/* Calculate number of bytes needed for the command */
	totlen = 1 + intlen(argc) + 2;
	for (j = 0; j < argc; j++) {
		len = argvlen ? argvlen[j] : strlen(argv[j]);
		totlen += 1 + intlen(len) + 2 + len + 2;
	}

	/* Build the command at protocol level */
	cmd = malloc(totlen + 1);
	if (!cmd) {
		return REDISLITE_OOM;
	}
	pos = sprintf(cmd, "*%d\r\n", argc);
	for (j = 0; j < argc; j++) {
		len = argvlen ? argvlen[j] : strlen(argv[j]);
		pos += sprintf(cmd + pos, "$%zu\r\n", len);
		memcpy(cmd + pos, argv[j], len);
		pos += len;
		cmd[pos++] = '\r';
		cmd[pos++] = '\n';
	}
	cmd[totlen] = '\0';
	*target = cmd;
	return totlen;
}

redislite_reply *redislite_execute_command(redislite *db, redislite_params *params)
{
	if (params->argc < 1) {
		redislite_reply *reply = redislite_create_reply();
		set_error_message(REDISLITE_ERR, reply); // this is more like an assert than an expected error
		return reply;
	}

	struct redislite_command *cmd = redislite_command_lookup(params->argv[0], params->argvlen[0]);
	if (cmd == NULL) {
		redislite_reply *reply = redislite_create_reply();
		if (reply == NULL) {
			return NULL;
		}
		reply->str = redislite_malloc(sizeof(char) * (strlen(unknown_command) + params->argvlen[0] - 1));
		char *str = redislite_malloc(sizeof(char) * (params->argvlen[0] + 1));
		memcpy(str, params->argv[0], params->argvlen[0]);
		str[params->argvlen[0]] = '\0';
		sprintf(reply->str, unknown_command, str);
		redislite_free(str);
		reply->len = strlen(unknown_command) + params->argvlen[0] - 2;
		reply->type = REDISLITE_REPLY_ERROR;
		return reply;
	}

	if ((cmd->arity > 0 && cmd->arity != params->argc) || ((int)params->argc < -cmd->arity)) {
		redislite_reply *reply = redislite_create_reply();
		reply->str = redislite_malloc(sizeof(char) * (strlen(wrong_arity) + params->argvlen[0] - 1));
		char *str = redislite_malloc(sizeof(char) * (params->argvlen[0] + 1));
		memcpy(str, params->argv[0], params->argvlen[0]);
		str[params->argvlen[0]] = '\0';
		sprintf(reply->str, wrong_arity, str);
		redislite_free(str);
		reply->len = strlen(wrong_arity) + params->argvlen[0] - 2;
		reply->type = REDISLITE_REPLY_ERROR;
		return reply;
	}
	redislite_reply *reply = cmd->proc(db, params);
	return reply;
}

redislite_reply *redislite_command(redislite *db, char *command)
{
	redislite_params *params;
	int status = redislite_format_command(&params, command);
	if (status != REDISLITE_OK) {
		redislite_reply *reply = redislite_create_reply();
		set_error_message(status, reply);
		return reply;
	}

	redislite_reply *reply = redislite_execute_command(db, params);
	redislite_free_params(params);
	return reply;
}

redislite_reply *redislite_command_argv(redislite *db, int argc, const char **argv, const size_t *argvlen)
{
	redislite_params *params = redislite_malloc(sizeof(redislite_params));
	params->argc = argc;
	params->argv = (char **)argv;
	params->argvlen = (size_t *)argvlen;
	params->must_free_argv = 0;
	redislite_reply *reply = redislite_execute_command(db, params);

	redislite_free_params(params);
	return reply;
}
