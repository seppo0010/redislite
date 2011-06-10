#include "redislite.h"
#include "page_string.h"
#include "page_index.h"
#include "public_api.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *dummy = NULL;
static char *test_add_key(changeset *cs, int *left)
{
	int rnd = rand();
	char *key = (char *)malloc(sizeof(char) * 14);
	sprintf(key, "%d", rnd);
	int size = (int)strlen(key);

	if (dummy == NULL) {
		dummy = malloc(sizeof(char) * cs->db->page_size + 1);
		memset(dummy, 'a', cs->db->page_size + 1);
		int i;
		for (i = 0; i < cs->db->page_size + 1; i++) {
			dummy[i] = (char)(('a' + i) % 128);
		}
	}
	dummy[0] = key[0];
	dummy[cs->db->page_size] = key[1];

	int insert = redislite_page_string_set_key_string(cs, key, size, dummy, cs->db->page_size + 1);
	if (insert != REDISLITE_OK) {
		free(key);
		key = NULL;
	}
	return key;
}

#define SIZE 500

int test_insert_and_find()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	int i;

	char *key[SIZE];
	int value[SIZE];
	for (i = 0; i < SIZE; i++) {
		key[i] = test_add_key(cs, &value[i]);
	}

	for (i = 0; i < SIZE; i++) {
		if (key[i] == NULL) {
			continue;
		}
		int length = 0;
		char *value = NULL;
		int size = (int)strlen(key[i]);
		size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], size, &value, &length);
		if (found == REDISLITE_OOM) {
			continue;
		}
		if (found == REDISLITE_NOT_FOUND) {
			printf("Key '%s' not found on line %d\n", key[i], __LINE__);
			continue;
		}

		if (length != cs->db->page_size + 1) {
			printf("Wrong length (%d) should be %d\n", length, cs->db->page_size + 1);
		}

		if (value[0] != key[i][0] || value[length - 1] != key[i][1]) {
			printf("Content mismatch on line %d\n", __LINE__);
			break;
		}
		free(value);
	}
	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	cs = NULL; // using stored values

	if (1) {
		for (i = 0; i < SIZE; i++) {
			if (key[i] == NULL) {
				continue;
			}
			int length = 0;
			char *value = NULL;
			int size = (int)strlen(key[i]);
			size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], size, &value, &length);
			if (found == REDISLITE_OOM) {
				continue;
			}

			if (found == REDISLITE_NOT_FOUND) {
				printf("Key '%s' not found on line %d\n", key[i], __LINE__);
				continue;
			}

			if (!value) {
				printf("Unable to find key: '%s'\n", key[i]);
				continue;
			}
			if (length != db->page_size + 1) {
				printf("Wrong length (%d) should be %d\n", length, db->page_size + 1);
			}
			if (value[0] != key[i][0] || value[length - 1] != key[i][1]) {
				printf("Content mismatch on line %d\n", __LINE__);
			}

			if (!value) {
				printf("Unable to find key: '%s'\n", key[i]);
				continue;
			}
			if (length != db->page_size + 1) {
				printf("Wrong length (%d) should be %d\n", length, db->page_size + 1);
			}

			free(value);
		}
	}

	for (i = 0; i < SIZE; i++) {
		if (key[i] != NULL) {
			free(key[i]);
		}
	}
	redislite_close_database(db);
	return REDISLITE_OK;
}

int test_insert_middle_and_find()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char key[412];
	memset(key, 'a', 412);
	key[0] = 'a';
	int r = redislite_page_string_set_key_string(cs, key, 200, "1", 1);
	if (r < 0) {
		return REDISLITE_SKIP;
	}
	key[0] = 'c';
	r = redislite_page_string_set_key_string(cs, key, 200, "1", 1);
	if (r < 0) {
		return REDISLITE_SKIP;
	}
	key[0] = 'b';
	r = redislite_page_string_set_key_string(cs, key, 200, "1", 1);
	if (r < 0) {
		return REDISLITE_SKIP;
	}
	r = redislite_page_string_set_key_string(cs, key, 2, "1", 1);
	if (r < 0) {
		return REDISLITE_SKIP;
	}

	char *value;
	int length;
	size_t found = redislite_page_string_get_by_keyname(db, cs, key, 200, &value, &length);
	int status;
	if (found == REDISLITE_OK) {
		free(value);
		status = REDISLITE_OK;
	}
	else {
		status = REDISLITE_ERR;
	}

	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	redislite_close_database(db);

	return status;
}

int test_setnx()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char key[10];
	memset(key, 'a', 10);
	int r = redislite_page_string_set_key_string(cs, key, 10, "1", 1);
	int status = REDISLITE_OK;
	if (r < 0) {
		status = REDISLITE_SKIP;
	}
	r = redislite_page_string_setnx_key_string(cs, key, 10, "1", 1);
	if (r < 0) {
		status = REDISLITE_SKIP;
	}
	else if (r > 0) {
		status = REDISLITE_ERR;
	}
	key[0] = 'b';
	r = redislite_page_string_setnx_key_string(cs, key, 10, "1", 1);
	if (r < 0) {
		status = REDISLITE_SKIP;
	}
	else if (r == 0) {
		status = REDISLITE_ERR;
	}

	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	redislite_close_database(db);

	return status;
}

int test_delete_and_find()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	int status = REDISLITE_OK;
	int i, j;

	char *key[SIZE];
	int value[SIZE];
	for (i = 0; i < SIZE; i++) {
		key[i] = test_add_key(cs, &value[i]);
	}

	char **keys = malloc(sizeof(char *) * (SIZE / 2));
	size_t *lengths = malloc(sizeof(size_t) * (SIZE / 2));
	for (i = 0; i < SIZE / 2; i++) {
		int size = (int)strlen(key[i * 2]);
		keys[i] = key[i * 2];
		lengths[i] = (size_t)size;
	}
	redislite_delete_keys(cs, SIZE / 2, keys, lengths);

	if (0)
		for (i = 0; i < SIZE; i++) {
			if (key[i] == NULL) {
				continue;
			}
			int length = 0;
			char *value = NULL;
			int size = (int)strlen(key[i]);
			if (memcmp(key[i], "154142252", size) == 0) {
				printf("a");
			}
			int found = redislite_page_string_get_by_keyname(db, cs, key[i], size, &value, &length);
			if (found == REDISLITE_OOM) {
				continue;
			}

			if (i % 2 == 0 && found != REDISLITE_NOT_FOUND) {
				printf("Key '%s' found after deleted\n", key[i]);
				status = REDISLITE_ERR;
			}
			else if (i % 2 == 1 && found == REDISLITE_NOT_FOUND) {
				printf("Key '%s' not found on line %d\n", key[i], __LINE__);
				status = REDISLITE_ERR;
			}
			free(value);
		}

	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	cs = NULL; // using stored values

	for (i = 0; i < SIZE; i++) {
		if (key[i] == NULL) {
			continue;
		}
		int length = 0;
		char *value = NULL;
		int size = (int)strlen(key[i]);
		int found = redislite_page_string_get_by_keyname(db, cs, key[i], size, &value, &length);
		if (found == REDISLITE_OOM) {
			continue;
		}

		if (i % 2 == 0 && found != REDISLITE_NOT_FOUND) {
			printf("Key '%s' found after deleted %d\n", key[i], found);
			status = REDISLITE_ERR;
		}
		else if (i % 2 == 1 && found == REDISLITE_NOT_FOUND) {
			printf("Key '%s' not found on line %d\n", key[i], __LINE__);
			status = REDISLITE_ERR;
		}
		free(value);
	}

	cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return 0;
	}
	for (i = 1; i < SIZE; i += 2) {
		if (key[i] != NULL) {
			free(key[i]);
		}
		key[i] = test_add_key(cs, &value[i]);
	}
	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	cs = NULL; // using stored values

	if (0)
		for (i = 0; i < SIZE; i++) {
			if (key[i] == NULL) {
				continue;
			}
			int length = 0;
			char *value = NULL;
			int found = redislite_page_string_get_by_keyname(db, cs, key[i], strlen(key[i]), &value, &length);
			if (found == REDISLITE_OOM) {
				continue;
			}

			if (found == REDISLITE_NOT_FOUND) {
				printf("Key '%s' not found on line %d\n", key[i], __LINE__);
				continue;
			}

			if (!value) {
				printf("Unable to find key: '%s'\n", key[i]);
				continue;
			}
			if (length != db->page_size + 1) {
				printf("Wrong length (%d) should be %d\n", length, db->page_size + 1);
			}
			for (j = 0; j < SIZE; j++) {
				if (value[j] != (char)(((int)key[i][0] + j) % 256)) {
					printf("Content mismatch on line %d\n", __LINE__);
				}
			}

			free(value);
		}

	for (i = 0; i < SIZE; i++)
		if (key[i] != NULL) {
			free(key[i]);
		}

	free(keys);
	free(lengths);
	redislite_close_database(db);

	return status;
}

int test_append()
{
	char key[10];
	char value[1026];
	memset(key, 'a', 10);
	value[0] = 'b';
	memset(&value[1], 'c', 999);
	value[9] = 'e';
	value[499] = 'f';
	value[1026] = 'g';

	redislite *db;
	changeset *cs;
	int status = REDISLITE_OK;
	int i;
	for (i = 0; i < 5; i++) {
		remove("test.db");

		db = redislite_open_database("test.db");
		if (db == NULL) {
			printf("OOM on test.c, on line %d\n", __LINE__);
			return REDISLITE_SKIP;
		}
		cs = redislite_create_changeset(db);
		if (cs == NULL) {
			redislite_close_database(db);
			printf("OOM on test.c, on line %d\n", __LINE__);
			return REDISLITE_SKIP;
		}

		int r = redislite_page_string_set_key_string(cs, key, 10, value, 9);
		if (r < 0) {
			status = REDISLITE_SKIP;
		}

		redislite_page_string_append_key_string(cs, key, 10, value, 10, NULL);
		if (i == 0) {
			redislite_save_changeset(cs);
			redislite_free_changeset(cs);
			cs = NULL;
		}

		char *lookup_value;
		int lookup_length;
		size_t found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
		if (found == REDISLITE_OOM) {
			status = REDISLITE_SKIP;
		}
		else if (found == REDISLITE_OK) {
			if (value[0] != lookup_value[0] || lookup_value[lookup_length - 1] != value[9]) {
				printf("Content mismatch on line %d\n", __LINE__);
				status = REDISLITE_ERR;
				redislite_free(lookup_value);
				goto cleanup;
			}
			redislite_free(lookup_value);
		}
		if (cs == NULL) {
			redislite_close_database(db);
			db = NULL;
			continue;
		}

		redislite_page_string_append_key_string(cs, key, 10, value, 500, NULL);
		if (i == 1) {
			redislite_save_changeset(cs);
			redislite_free_changeset(cs);
			cs = NULL;
		}
		found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
		if (found == REDISLITE_OOM) {
			status = REDISLITE_SKIP;
		}
		else {
			if (value[0] != lookup_value[0] || lookup_value[lookup_length - 1] != value[499]) {
				printf("Content mismatch on line %d\n", __LINE__);
				status = REDISLITE_ERR;
				redislite_free(lookup_value);
				goto cleanup;
			}
			redislite_free(lookup_value);
		}
		if (cs == NULL) {
			redislite_close_database(db);
			db = NULL;
			continue;
		}

		/* TODO
		redislite_page_string_append_key_string(cs, key, 10, value, 1025, NULL);
		if (i == 2) {
			redislite_save_changeset(cs);
			redislite_free_changeset(cs);
			cs = NULL;
		}
		found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
		if (found == REDISLITE_OOM) status = REDISLITE_SKIP;
		else {
			if (value[0] != lookup_value[0] || lookup_value[lookup_length-1] != value[1025]) {
				printf("Content mismatch on line %d\n", __LINE__);
				status = REDISLITE_ERR;
				goto cleanup;
			}
		}
		if (cs == NULL) {
			redislite_close_database(db);
			db = NULL;
			continue;
		}
		 */

		redislite_page_string_append_key_string(cs, key, 10, value, 1, NULL);
		if (i == 3) {
			redislite_save_changeset(cs);
			redislite_free_changeset(cs);
			cs = NULL;
		}
		found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
		if (found == REDISLITE_OOM) {
			status = REDISLITE_SKIP;
		}
		else {
			if (value[0] != lookup_value[0] || lookup_value[lookup_length - 1] != value[0]) {
				printf("Content mismatch on line %d\n", __LINE__);
				status = REDISLITE_ERR;
				redislite_free(lookup_value);
				goto cleanup;
			}
			redislite_free(lookup_value);
		}
		if (cs == NULL) {
			redislite_close_database(db);
			db = NULL;
			continue;
		}
		redislite_free_changeset(cs);
		cs = NULL;
		redislite_close_database(db);
		db = NULL;
	}
cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	return status;
}

int test_incr()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char *key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "1123", 4);
	if (status != REDISLITE_OK) {
		printf("Failed to create a numeric random key\n");
		goto cleanup;
	}

	long long new_value;
	status = redislite_page_string_incr_key_string(cs, key, 7, &new_value);
	if (status != REDISLITE_OK) {
		printf("Failed to increment a numeric key\n");
		goto cleanup;
	}
	if (new_value != 1124) {
		printf("After incr 1123, result should be 1124 but it is '%lld'\n", new_value);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_incr_key_string(cs, key, 6, &new_value);
	if (status != REDISLITE_OK) {
		printf("Failed to incr an unexisting key\n");
		goto cleanup;
	}
	if (new_value != 1) {
		printf("After incr unexisting key, result should be 1 but it is '%lld'\n", new_value);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_set_key_string(cs, key, 6, "asd", 3);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}
	status = redislite_page_string_incr_key_string(cs, key, 6, &new_value);
	if (status == REDISLITE_ERR) {
		status = REDISLITE_OK;
	}
	else if (status == REDISLITE_OK) {
		printf("Able to incr a non-numeric key\n");
		status = REDISLITE_ERR;
	}

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_decr()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char *key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a numeric random key\n");
		goto cleanup;
	}

	long long new_value;
	status = redislite_page_string_decr_by_key_string(cs, key, 7, 10, &new_value);
	if (status != REDISLITE_OK) {
		printf("Failed to increment a numeric key\n");
		goto cleanup;
	}
	if (new_value != -7) {
		printf("After decr 3 by 10, result should be -7 but it is '%lld'\n", new_value);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_decr_key_string(cs, key, 6, &new_value);
	if (status != REDISLITE_OK) {
		printf("Failed to decr an unexisting key\n");
		goto cleanup;
	}
	if (new_value != -1) {
		printf("After incr unexisting key, result should be -1 but it is '%lld'\n", new_value);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_set_key_string(cs, key, 6, "asd", 3);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}
	status = redislite_page_string_decr_key_string(cs, key, 6, &new_value);
	if (status == REDISLITE_ERR) {
		status = REDISLITE_OK;
	}
	else if (status == REDISLITE_OK) {
		printf("Able to decr a non-numeric key\n");
		status = REDISLITE_ERR;
	}

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_exists()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char *key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	status = redislite_exists_key(db, cs, key, 7);
	if (status == 0) {
		printf("Failed to find existing key\n");
		status = REDISLITE_ERR;
		goto cleanup;
	}
	else if (status != 1) {
		goto cleanup;
	}

	status = redislite_exists_key(db, cs, key, 6);
	if (status != REDISLITE_NOT_FOUND) {
		printf("Failed to not-find non existing key\n");
		goto cleanup;
	}
	else {
		status = REDISLITE_OK;
	}

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_type()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char *key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	char type;
	status = redislite_page_index_type(db, cs, key, 7, &type);
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	else if (status != REDISLITE_OK) {
		printf("Failed to get type of existing key\n");
		goto cleanup;
	}

	if (type != REDISLITE_PAGE_TYPE_STRING) {
		status = REDISLITE_ERR;
		printf("Wrong type: expecting '%c', got '%c' instead", REDISLITE_PAGE_TYPE_STRING, type);
		goto cleanup;
	}

	status = redislite_page_index_type(db, cs, key, 6, &type);
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	else if (status != REDISLITE_NOT_FOUND) {
		status = REDISLITE_ERR;
		printf("Getting type of non-existing key\n");
		goto cleanup;
	}
	else {
		status = REDISLITE_OK;
	}

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_getrange()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char *key = "testkey";
	char value[1024];
	memset(value, 'a', 1024);
	value[0] = 'b';
	value[550] = 'c';
	value[1023] = 'd';
	int status = redislite_page_string_set_key_string(cs, key, 7, value, 1024);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	char *str;
	int str_length;
	status = redislite_page_string_getrange_key_string(db, cs, key, 7, 0, 0, &str, &str_length);
	if (status != REDISLITE_OK) {
		printf("Failed to getrange\n");
		goto cleanup;
	}
	else if (str_length != 1) {
		printf("getrange mismatch; expecting length %d, but got %d instead\n", 1, str_length);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	else if (str[0] != value[0]) {
		printf("getrange mismatch; character expected to be '%c' but got '%c' instead\n", value[0], str[0]);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	redislite_free(str);

	status = redislite_page_string_getrange_key_string(db, cs, key, 7, 0, 1023, &str, &str_length);
	if (status != REDISLITE_OK) {
		printf("Failed to getrange\n");
		goto cleanup;
	}
	else if (str_length != 1024) {
		printf("getrange mismatch; expecting length %d, but got %d instead\n", 1024, str_length);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	else if (memcmp(value, str, 1024)) {
		printf("getrange mismatch; full range select does not match strings\n");
		status = REDISLITE_ERR;
		goto cleanup;
	}
	redislite_free(str);

	status = redislite_page_string_getrange_key_string(db, cs, key, 7, -1, -1, &str, &str_length);
	if (status != REDISLITE_OK) {
		printf("Failed to getrange\n");
		goto cleanup;
	}
	else if (str_length != 1) {
		printf("getrange mismatch; expecting length %d, but got %d instead\n", 1, str_length);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	else if (str[0] != value[1023]) {
		printf("getrange mismatch; expecting last char to be '%c' but got '%c' instead\n", value[1023], str[0]);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	redislite_free(str);

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_echo()
{
	char *test_str = malloc(sizeof(char) * 100);
	memset(test_str, 'a', 100);
	char *dst;
	int dst_length;
	int status = redislite_echo(test_str, 100, &dst, &dst_length);
	if (status == REDISLITE_OOM) {
		dst = NULL;
	}
	if (status == REDISLITE_OK) {
		status = memcmp(dst, test_str, 100) == 0 ? REDISLITE_OK : REDISLITE_ERR;
	}
	if (dst) {
		redislite_free(dst);
	}
	free(test_str);
	return status;
}

int test_strlen()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char *key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	status = redislite_page_string_strlen_key_string(db, cs, key, 7);
	if (status == 1) {
		status = REDISLITE_OK;
	}
	else {
		printf("Unable to get string length (expected %d, got %d)", 1, status);
		if (status > 1) {
			status = REDISLITE_ERR;
		}
		goto cleanup;
	}

	status = redislite_page_string_strlen_key_string(db, cs, key, 7);
	if (status == 0) {
		status = REDISLITE_OK;
	}
	else {
		printf("Unable to get unexisting string length (expected %d, got %d)", 0, status);
		if (status > 1) {
			status = REDISLITE_ERR;
		}
		goto cleanup;
	}

	// TODO: add test for different key type

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_getset()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char *key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	char *previous_value;
	int previous_value_length;
	status = redislite_page_string_getset_key_string(cs, key, 7, "41", 2, &previous_value, &previous_value_length);
	if (status !=  REDISLITE_OK) {
		printf("Unable to getset string on line %d - received %d\n", __LINE__, status);
		goto cleanup;
	}
	if (previous_value_length != 1) {
		printf("Expecting getset previous length %d, got %d instead\n", 1, previous_value_length);
		goto cleanup;
	}
	if (previous_value[0] != '3') {
		printf("Expecting getset previous value '%c', got '%c' instead\n", '3', previous_value[0]);
		goto cleanup;
	}

	redislite_free(previous_value);
	status = redislite_page_string_getset_key_string(cs, key, 7, "41", 2, &previous_value, &previous_value_length);
	if (status !=  REDISLITE_OK) {
		printf("Unable to getset string on line %d - received %d\n", __LINE__, status);
		goto cleanup;
	}
	if (previous_value_length != 2) {
		printf("Expecting getset previous length %d, got %d instead\n", 2, previous_value_length);
		goto cleanup;
	}
	if (previous_value[0] != '4' || previous_value[1] != '1') {
		printf("Expecting getset previous value '%c%c', got '%c%c' instead\n", '4', '1', previous_value[0], previous_value[1]);
		goto cleanup;
	}
	redislite_free(previous_value);

	status = redislite_page_string_getset_key_string(cs, key, 6, "4", 1, &previous_value, &previous_value_length);
	if (status !=  REDISLITE_OK) {
		printf("Unable to getset string on line %d - received %d\n", __LINE__, status);
		goto cleanup;
	}
	if (previous_value_length != 0) {
		printf("Expecting getset previous length %d, got %d instead\n", 0, previous_value_length);
		goto cleanup;
	}

	// TODO: add test for different key type

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_getbit()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char *key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "1", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 0);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 0) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 0, 0, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 1);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 0) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 0, 1, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 2);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 1) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 1, 2, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 3);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 1) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 1, 3, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 4);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 0) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 0, 4, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 5);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 0) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 0, 5, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 6);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 0) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 0, 6, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 7);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 1) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 1, 7, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 8);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 0) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value 1 on bit %d, got %d instead\n", 0, 8, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	char test[600];
	memset(test, 'a', 600);
	status = redislite_page_string_set_key_string(cs, key, 7, test, 600);
	if (status < 0 || status != REDISLITE_OK) {
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 4098);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 1) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value aaa(600)aaa on bit %d, got %d instead\n", 1, 4098, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_getbit_key_string(db, cs, key, 7, 4096);
	if (status < 0) {
		goto cleanup;
	}
	else if (status == 0) {
		status = REDISLITE_OK;
	}
	else {
		printf("getbit expected %d for value aaa(600)aaa on bit %d, got %d instead\n", 0, 4096, status);
		status = REDISLITE_ERR;
		goto cleanup;
	}

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_get_publicapi()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) {
		redislite_close_database(db);
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char key[10];
	memset(key, 'a', 10);
	int r = redislite_page_string_set_key_string(cs, key, 10, "1", 1);
	int status = REDISLITE_OK;
	if (r < 0) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	redislite_save_changeset(cs);
	redislite_free_changeset(cs);

	redislite_params *params = redislite_create_params();
	if (params == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	params->must_free_argv = 1;
	params->argc = 2;
	params->argvlen = redislite_malloc(sizeof(size_t) * 2);
	if (params->argvlen == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	params->argv = redislite_malloc(sizeof(char *) * 2);
	if (params->argv == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	params->argv[0] = redislite_malloc(sizeof(char) * 3);
	if (params->argv[0] == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	memcpy(params->argv[0], "GET", 3);
	params->argvlen[0] = 3;
	params->argv[1] = redislite_malloc(sizeof(char) * 10);
	if (params->argv[1] == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	memcpy(params->argv[1], key, 10);
	params->argvlen[1] = 10;
	redislite_reply *reply = redislite_get_command(db, params);
	if (reply == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_STRING) {
		printf("Expecting string after getting key using public API, got %d instead\n", reply->type);
		status = REDISLITE_ERR;
		redislite_free_reply(reply);
		goto cleanup;
	}
	redislite_free_reply(reply);

cleanup:
	if (params) {
		redislite_free_params(params);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_set_publicapi()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char key[10];
	memset(key, 'a', 10);
	int status = REDISLITE_OK;

	redislite_params *params = redislite_create_params();
	if (params == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	params->must_free_argv = 1;
	params->argvlen = redislite_malloc(sizeof(size_t) * 3);
	if (params->argvlen == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	params->argv = redislite_malloc(sizeof(char *) * 3);
	if (params->argv == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	params->argv[0] = redislite_malloc(sizeof(char) * 3);
	if (params->argv[0] == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	memcpy(params->argv[0], "GET", 3);
	params->argvlen[0] = 3;

	params->argv[1] = redislite_malloc(sizeof(char) * 10);
	if (params->argv[1] == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	memcpy(params->argv[1], key, 10);
	params->argvlen[1] = 10;

	params->argv[2] = redislite_malloc(sizeof(char) * 11);
	if (params->argv[2] == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}
	memcpy(params->argv[2], key, 10);
	params->argv[2][10] = 'x';
	params->argvlen[2] = 11;

	params->argc = 3;
	redislite_reply *reply = redislite_set_command(db, params);
	if (reply == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_STATUS && memcmp(reply->str, "OK", 3) != 0) {
		printf("Expecting status OK after getting key using public API, got %d instead\n", reply->type);
		if (reply->type == REDISLITE_REPLY_STATUS || reply->type == REDISLITE_REPLY_ERROR) {
			printf("Result str (status or error) was '%s'\n", reply->str);
		}
		status = REDISLITE_ERR;
		redislite_free_reply(reply);
		goto cleanup;
	}
	redislite_free_reply(reply);

	char *value;
	int length;
	size_t found = redislite_page_string_get_by_keyname(db, NULL, key, 10, &value, &length);

	if (found < 0) {
		goto cleanup;
	}

	if (length != 11) {
		printf("Wrong length (%d) should be %d\n", length, 11);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (memcmp(value, key, 10) != 0 || value[10] != 'x') {
		printf("Content mismatch on line %d\n", __LINE__);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	free(value);

cleanup:
	if (params) {
		redislite_free_params(params);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_format_get_set_publicapi()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	char key[10];
	memset(key, 'a', 10);
	int status = REDISLITE_OK;

	redislite_params *params;
	redislite_format_command(&params, "SET %s %b", "test", "tesasdasdad", 5);
	redislite_reply *reply = redislite_set_command(db, params);
	if (reply == NULL) {
		status = REDISLITE_SKIP;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_STATUS && memcmp(reply->str, "OK", 3) != 0) {
		printf("Expecting status OK after setting key using public API's formatter, got %d instead\n", reply->type);
		if (reply->type == REDISLITE_REPLY_STATUS || reply->type == REDISLITE_REPLY_ERROR) {
			printf("Result str (status or error) was '%s'\n", reply->str);
		}
		status = REDISLITE_ERR;
		redislite_free_reply(reply);
		goto cleanup;
	}
	redislite_free_reply(reply);

	char *value;
	int length;
	size_t found = redislite_page_string_get_by_keyname(db, NULL, "test", 4, &value, &length);

	if (found < 0) {
		goto cleanup;
	}

	if (length != 5) {
		printf("Wrong length (%d) should be %d\n", length, 5);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (memcmp(value, "tesas", 5) != 0) {
		printf("Content mismatch on line %d\n", __LINE__);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	free(value);

cleanup:
	if (params) {
		redislite_free_params(params);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_format()
{
	redislite_params *target;
	int status = redislite_format_command(&target, "GET %d", 123532);
	if (status != REDISLITE_OK) {
		goto cleanup;
	}

	if (target->argc != 2) {
		printf("target elements count expected to be %d, but got %d instead\n", 2, (int)target->argc);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (target->argvlen[0] != 3) {
		printf("target element 0 length expected to be %d, but got %d instead\n", 3, (int)target->argvlen[0]);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (memcmp(target->argv[0], "GET", 3) != 0) {
		printf("target element 0 content mismatch expected to be %s, but got %s instead\n", "GET", target->argv[0]);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (target->argvlen[1] != 6) {
		printf("target element 1 length expected to be %d, but got %d instead\n", 6, (int)target->argvlen[1]);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (memcmp(target->argv[1], "123532", 6) != 0) {
		printf("target element 1 content mismatch expected to be %s, but got %s instead\n", "123532", target->argv[1]);
		status = REDISLITE_ERR;
		goto cleanup;
	}


cleanup:
	if (target) {
		redislite_free_params(target);
	}
	return status;
}

int test_command()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	int status = REDISLITE_OK;
	redislite_reply *reply = redislite_command(db, "SET mykey value");
	if (reply == NULL) {
		status = REDISLITE_OOM;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_STATUS) {
		printf("Expecting status type to be %d after setting new key, got %d instead\n", REDISLITE_REPLY_STATUS, reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (reply->len != 3 || memcmp(reply->str, "OK", 3) != 0) {
		printf("Expecting status response to be %s after setting new key, got %s instead\n", "OK", reply->str);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	redislite_free_reply(reply);
	reply = redislite_command(db, "GET mykey");
	if (reply == NULL) {
		status = REDISLITE_OOM;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_STRING) {
		printf("Expecting status type to be %d after getting new key, got %d instead\n", REDISLITE_REPLY_STRING, reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (reply->len != 5 || memcmp(reply->str, "value", 5) != 0) {
		printf("Expecting status response to be %s after getting new key, got %s instead\n", "value", reply->str);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	redislite_free_reply(reply);
	reply = redislite_command(db, "GET");
	if (reply == NULL) {
		status = REDISLITE_OOM;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_ERROR) {
		printf("Expecting status type to be %d after getting with no key parameter, got %d instead\n", REDISLITE_REPLY_ERROR, reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	redislite_free_reply(reply);
	reply = redislite_command(db, "ASDET");
	if (reply == NULL) {
		status = REDISLITE_OOM;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_ERROR) {
		printf("Expecting status type to be %d after calling unexisting command, got %d instead\n", REDISLITE_REPLY_ERROR, reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}

cleanup:
	if (db) {
		redislite_close_database(db);
	}
	if (reply) {
		redislite_free_reply(reply);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_free_and_set()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	redislite_reply *reply;
	reply = redislite_command(db, "set a 1");
	redislite_free_reply(reply);
	reply = redislite_command(db, "get a");
	redislite_free_reply(reply);
	reply = redislite_command(db, "del a");
	redislite_free_reply(reply);
	reply = redislite_command(db, "set a 1");
	redislite_free_reply(reply);
	reply = redislite_command(db, "set b 1");
	redislite_free_reply(reply);
	reply = redislite_command(db, "del a b");
	redislite_free_reply(reply);
	redislite_close_database(db);
	return REDISLITE_OK;
}

int test_command_argv()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	int status = REDISLITE_OK;

	const char *set_argv[] = {"SET", "mykey", "value"};
	size_t set_argvlen[] = {3, 5, 5};

	const char *get_argv[] = {"GET", "mykey"};
	size_t get_argvlen[] = {3, 5};

	const char *fail_get_argv[] = {"GET"};
	size_t fail_get_argvlen[] = {3};

	const char *fail_command_argv[] = {"ASDET"};
	size_t fail_command_argvlen[] = {5};

	redislite_reply *reply = redislite_command_argv(db, 3, set_argv, set_argvlen);
	if (reply == NULL) {
		status = REDISLITE_OOM;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_STATUS) {
		printf("Expecting status type to be %d after setting new key, got %d instead\n", REDISLITE_REPLY_STATUS, reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (reply->len != 3 || memcmp(reply->str, "OK", 3) != 0) {
		printf("Expecting status response to be %s after setting new key, got %s instead\n", "OK", reply->str);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	redislite_free_reply(reply);
	reply = redislite_command_argv(db, 2, get_argv, get_argvlen);
	if (reply == NULL) {
		status = REDISLITE_OOM;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_STRING) {
		printf("Expecting status type to be %d after getting new key, got %d instead\n", REDISLITE_REPLY_STRING, reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	if (reply->len != 5 || memcmp(reply->str, "value", 5) != 0) {
		printf("Expecting status response to be %s after getting new key, got %s instead\n", "value", reply->str);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	redislite_free_reply(reply);
	reply = redislite_command_argv(db, 1, fail_get_argv, fail_get_argvlen);
	if (reply == NULL) {
		status = REDISLITE_OOM;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_ERROR) {
		printf("Expecting status type to be %d after getting with no key parameter, got %d instead\n", REDISLITE_REPLY_ERROR, reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}

	redislite_free_reply(reply);
	reply = redislite_command_argv(db, 1, fail_command_argv, fail_command_argvlen);
	if (reply == NULL) {
		status = REDISLITE_OOM;
		goto cleanup;
	}

	if (reply->type != REDISLITE_REPLY_ERROR) {
		printf("Expecting status type to be %d after calling unexisting command, got %d instead\n", REDISLITE_REPLY_ERROR, reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}

cleanup:
	if (db) {
		redislite_close_database(db);
	}
	if (reply) {
		redislite_free_reply(reply);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int test_issue_2()
{
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) {
		printf("OOM on test.c, on line %d\n", __LINE__);
		return REDISLITE_SKIP;
	}
	redislite_reply *reply;
	reply = redislite_command(db, "SET a 1");
	redislite_free_reply(reply);
	reply = redislite_command(db, "DEL a");
	redislite_free_reply(reply);
	reply = redislite_command(db, "SET a 1");
	redislite_free_reply(reply);
	reply = redislite_command(db, "SET a 1");
	redislite_free_reply(reply);
	reply = redislite_command(db, "GET a");
	int status = REDISLITE_OK;
	if (reply->type != REDISLITE_REPLY_STRING) {
		printf("Expecting string after getting key; got %d\n", reply->type);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	if (reply->len != 1) {
		printf("Expecting len %d after getting key; got %d\n", 1, reply->len);
		status = REDISLITE_ERR;
		goto cleanup;
	}
	if (reply->str[0] != '1') {
		printf("Expecting string '%s' getting key; got '%s'\n", "1", reply->str);
		status = REDISLITE_ERR;
		goto cleanup;
	}
cleanup:
	if (db) {
		redislite_close_database(db);
	}
	if (reply) {
		redislite_free_reply(reply);
	}
	if (status == REDISLITE_OOM) {
		status = REDISLITE_SKIP;
	}
	return status;
}

int main()
{
	srand(4);
	int test;
	const char *test_name;

	test = test_insert_and_find();
	test_name = "Insert and Find";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_insert_middle_and_find();
	test_name = "Insert Middle and Find";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_delete_and_find();
	test_name = "Delete key and find";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_setnx();
	test_name = "setnx for existing and non-existing key";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_append();
	test_name = "append string";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_incr();
	test_name = "incr string";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_decr();
	test_name = "decr and decrby string";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_exists();
	test_name = "check for key existance";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_echo();
	test_name = "testing echo";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_type();
	test_name = "testing type";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_getset();
	test_name = "testing getset";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_getrange();
	test_name = "testing getrange";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_getbit();
	test_name = "testing getbit";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_get_publicapi();
	test_name = "testing get on publicapi";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_set_publicapi();
	test_name = "testing set on publicapi";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_format_get_set_publicapi();
	test_name = "testing format get/set on publicapi";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_format();
	test_name = "testing format parsing on publicapi";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_command();
	test_name = "testing command execution on publicapi";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_command_argv();
	test_name = "testing command argv execution on publicapi";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_issue_2();
	test_name = "testing issue #2";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_free_and_set();
	test_name = "testing freelist and set";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	}
	else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	free(dummy);

	return 0;
}
