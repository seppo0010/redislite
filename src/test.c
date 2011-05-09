#include "redislite.h"
#include "page_string.h"
#include "page_index.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *dummy = NULL;
static char *test_add_key(changeset *cs, int *left)
{
	int rnd = rand();
	char *key = (char*)malloc(sizeof(char) * 14);
	sprintf(key, "%d", rnd);
	int size = (int)strlen(key);

	if (dummy == NULL) {
		dummy = malloc(sizeof(char) * cs->db->page_size+1);
		memset(dummy, 'a', cs->db->page_size+1);
		int i;
		for (i=0;i<cs->db->page_size+1;i++) {
			dummy[i] = (char)(('a' + i) % 128);
		}
	}
	dummy[0] = key[0];
	dummy[cs->db->page_size] = key[1];

	int insert = redislite_page_string_set_key_string(cs, key, size, dummy, cs->db->page_size+1);
	if (insert != REDISLITE_OK) {
		free(key);
		key = NULL;
	}
	return key;
}

#define SIZE 50

int test_insert_and_find() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	int i;

	char *key[SIZE];
	int value[SIZE];
	for (i=0; i < SIZE; i++)
		key[i] = test_add_key(cs, &value[i]);

	for (i=0; i < SIZE; i++) {
		if (key[i] == NULL) continue;
		int length = 0;
		char *value = NULL;
		int size = (int)strlen(key[i]);
		size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], size, &value, &length);
		if (found == REDISLITE_OOM) {
			continue;
		}
		if (found == REDISLITE_ERR) {
			printf("Key '%s' not found\n", key[i]);
			continue;
		}

		if (length != cs->db->page_size+1) {
			printf("Wrong length (%d) should be %d\n", length, cs->db->page_size+1);
		}

		if (value[0] != key[i][0] || value[length-1] != key[i][1]) {
			printf("Content mismatch\n");
			break;
		}
		free(value);
	}
	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	cs = NULL; // using stored values

	if (1) {
		for (i=0; i < SIZE; i++) {
			if (key[i] == NULL) continue;
			int length = 0;
			char *value = NULL;
			int size = (int)strlen(key[i]);
			size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], size, &value, &length);
			if (found == REDISLITE_OOM) {
				continue;
			}

			if (found == REDISLITE_ERR) {
				printf("Key '%s' not found\n", key[i]);
				continue;
			}

			if (!value) {
				printf("Unable to find key: '%s'\n", key[i]);
				continue;
			}
			if (length != db->page_size+1) {
				printf("Wrong length (%d) should be %d\n", length, db->page_size+1);
			}
			if (value[0] != key[i][0] || value[length-1] != key[i][1]) {
				printf("Content mismatch\n");
			}

			free(value);
		}
	}

	for (i=0; i < SIZE; i++)
		if (key[i] != NULL)
			free(key[i]);
	redislite_close_database(db);
	return REDISLITE_OK;
}

int test_insert_middle_and_find() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char key[412];
	memset(key, 'a', 412);
	key[0] = 'a';
	int r = redislite_page_string_set_key_string(cs, key, 200, "1", 1);
	if (r < 0) return REDISLITE_SKIP;
	key[0] = 'c';
	r = redislite_page_string_set_key_string(cs, key, 200, "1", 1);
	if (r < 0) return REDISLITE_SKIP;
	key[0] = 'b';
	r = redislite_page_string_set_key_string(cs, key, 200, "1", 1);
	if (r < 0) return REDISLITE_SKIP;
	r = redislite_page_string_set_key_string(cs, key, 2, "1", 1);
	if (r < 0) return REDISLITE_SKIP;

	char *value;
	int length;
	size_t found = redislite_page_string_get_by_keyname(db, cs, key, 200, &value, &length);
	int status;
	if (found == REDISLITE_OK) {
		free(value);
		status = REDISLITE_OK;
	} else {
		status = REDISLITE_ERR;
	}

	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	redislite_close_database(db);

	return status;
}

int test_setnx() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char key[10];
	memset(key, 'a', 10);
	int r = redislite_page_string_set_key_string(cs, key, 10, "1", 1);
	int status = REDISLITE_OK;
	if (r < 0) status = REDISLITE_SKIP;
	r = redislite_page_string_setnx_key_string(cs, key, 10, "1", 1);
	if (r < 0) status = REDISLITE_SKIP;
	else if (r > 0) status = REDISLITE_ERR;
	key[0] = 'b';
	r = redislite_page_string_setnx_key_string(cs, key, 10, "1", 1);
	if (r < 0) status = REDISLITE_SKIP;
	else if (r == 0) status = REDISLITE_ERR;

	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	redislite_close_database(db);

	return status;
}

int test_delete_and_find() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	int status = REDISLITE_OK;
	int i;

	char *key[SIZE];
	int value[SIZE];
	for (i=0; i < SIZE; i++)
		key[i] = test_add_key(cs, &value[i]);

	char **keys = malloc(sizeof(char*) * (SIZE/2));
	int *lengths = malloc(sizeof(int) * (SIZE/2));
	for (i=0; i < SIZE/2; i++) {
		int size = (int)strlen(key[i*2]);
		keys[i] = key[i*2];
		lengths[i] = size;
	}
	redislite_delete_keys(cs, SIZE/2, keys, lengths);

	for (i=0; i < SIZE; i++) {
		if (key[i] == NULL) continue;
		int length = 0;
		char *value = NULL;
		int size = (int)strlen(key[i]);
		size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], size, &value, &length);
		if (found == REDISLITE_OOM) {
			continue;
		}

		if (i % 2 == 0 && found != REDISLITE_ERR) {
			printf("Key '%s' found after deleted\n", key[i]);
			status = REDISLITE_ERR;
		} else if (i % 2 == 1 && found == REDISLITE_ERR) {
			printf("Key '%s' not found\n", key[i]);
			status = REDISLITE_ERR;
		}
		free(value);
	}

	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	cs = NULL; // using stored values

	for (i=0; i < SIZE; i++) {
		if (key[i] == NULL) continue;
		int length = 0;
		char *value = NULL;
		int size = (int)strlen(key[i]);
		size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], size, &value, &length);
		if (found == REDISLITE_OOM) {
			continue;
		}

		if (i % 2 == 0 && found != REDISLITE_ERR) {
			printf("Key '%s' found after deleted\n", key[i]);
			status = REDISLITE_ERR;
		} else if (i % 2 == 1 && found == REDISLITE_ERR) {
			printf("Key '%s' not found\n", key[i]);
			status = REDISLITE_ERR;
		}
		free(value);
	}

	for (i=0; i < SIZE; i++)
		if (key[i] != NULL)
			free(key[i]);

	free(keys);
	free(lengths);
	redislite_close_database(db);

	return status;
}

int test_append() {
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
		if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
		cs = redislite_create_changeset(db);
		if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }

		int r = redislite_page_string_set_key_string(cs, key, 10, value, 9);
		if (r < 0) status = REDISLITE_SKIP;

		redislite_page_string_append_key_string(cs, key, 10, value, 10, NULL);
		if (i == 0) {
			redislite_save_changeset(cs);
			redislite_free_changeset(cs);
			cs = NULL;
		}

		char *lookup_value;
		int lookup_length;
		size_t found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
		if (found == REDISLITE_OOM) status = REDISLITE_SKIP;
		else if (found == REDISLITE_OK) {
			if (value[0] != lookup_value[0] || lookup_value[lookup_length-1] != value[9]) {
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
		if (found == REDISLITE_OOM) status = REDISLITE_SKIP;
		else {
			if (value[0] != lookup_value[0] || lookup_value[lookup_length-1] != value[499]) {
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
		if (found == REDISLITE_OOM) status = REDISLITE_SKIP;
		else {
			if (value[0] != lookup_value[0] || lookup_value[lookup_length-1] != value[0]) {
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

int test_incr() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char* key = "testkey";
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
	} else if (status == REDISLITE_OK) {
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
	if (status == REDISLITE_OOM) status = REDISLITE_SKIP;
	return status;
}

int test_decr() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char* key = "testkey";
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
	} else if (status == REDISLITE_OK) {
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
	if (status == REDISLITE_OOM) status = REDISLITE_SKIP;
	return status;
}

int test_exists() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char* key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	status = redislite_exists_key(db, cs, key, 7);
	if (status == 0)
	{
		printf("Failed to find existing key\n");
		goto cleanup;
	} else if (status != 1) {
		goto cleanup;
	}

	status = redislite_exists_key(db, cs, key, 6);
	if (status == 1)
	{
		printf("Failed to not-find non existing key\n");
		goto cleanup;
	} else if (status != 1) {
		goto cleanup;
	}

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) status = REDISLITE_SKIP;
	return status;
}

int test_type() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char* key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	char type;
	status = redislite_page_index_type(db, cs, key, 7, &type);
	if (status == REDISLITE_OOM)
	{
		status = REDISLITE_SKIP;
		goto cleanup;
	} else if (status != REDISLITE_OK)
	{
		printf("Failed to get type of existing key\n");
		goto cleanup;
	}

	if (type != REDISLITE_PAGE_TYPE_STRING)
	{
		status = REDISLITE_ERR;
		printf("Wrong type: expecting '%c', got '%c' instead", REDISLITE_PAGE_TYPE_STRING, type);
		goto cleanup;
	}

	status = redislite_page_index_type(db, cs, key, 6, &type);
	if (status == REDISLITE_OOM)
	{
		status = REDISLITE_SKIP;
		goto cleanup;
	} else if (status != REDISLITE_ERR)
	{
		status = REDISLITE_ERR;
		printf("Getting type of non-existing key\n");
		goto cleanup;
	} else {
		status = REDISLITE_OK;
	}

cleanup:
	if (cs) {
		redislite_free_changeset(cs);
	}
	if (db) {
		redislite_close_database(db);
	}
	if (status == REDISLITE_OOM) status = REDISLITE_SKIP;
	return status;
}

int test_echo() {
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
	if (dst) redislite_free(dst);
	free(test_str);
	return status;
}

int test_strlen() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char* key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	status = redislite_page_string_strlen_key_string(db, cs, key, 7);
	if (status == 1) {
		status = REDISLITE_OK;
	} else {
		printf("Unable to get string length (expected %d, got %d)", 1, status);
		if (status > 1) status = REDISLITE_ERR;
		goto cleanup;
	}

	status = redislite_page_string_strlen_key_string(db, cs, key, 7);
	if (status == 0) {
		status = REDISLITE_OK;
	} else {
		printf("Unable to get unexisting string length (expected %d, got %d)", 0, status);
		if (status > 1) status = REDISLITE_ERR;
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
	if (status == REDISLITE_OOM) status = REDISLITE_SKIP;
	return status;
}

int test_getset() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char* key = "testkey";
	int status = redislite_page_string_set_key_string(cs, key, 7, "3", 1);
	if (status != REDISLITE_OK) {
		printf("Failed to create a random key\n");
		goto cleanup;
	}

	char* previous_value;
	int previous_value_length;
	status = redislite_page_string_getset_key_string(cs, key, 7, "41", 2, &previous_value, &previous_value_length);
	if (status !=  REDISLITE_OK) {
		printf("Unable to getset string\n");
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
		printf("Unable to getset string\n");
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
		printf("Unable to getset string\n");
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
	if (status == REDISLITE_OOM) status = REDISLITE_SKIP;
	return status;
}

int main() {
	srand(4);
	int test;
	const char * test_name;

	test = test_insert_and_find();
	test_name = "Insert and Find";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_insert_middle_and_find();
	test_name = "Insert Middle and Find";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_delete_and_find();
	test_name = "Delete key and find";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_setnx();
	test_name = "setnx for existing and non-existing key";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_append();
	test_name = "append string";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_incr();
	test_name = "incr string";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_decr();
	test_name = "decr and decrby string";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_exists();
	test_name = "check for key existance";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_echo();
	test_name = "testing echo";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_type();
	test_name = "testing type";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	test = test_getset();
	test_name = "testing getset";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d\n", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d\n", test_name, __LINE__);
	}

	free(dummy);

	return 0;
}
