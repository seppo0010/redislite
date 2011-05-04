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

	for (i=0; i < SIZE/2; i++) {
		int size = (int)strlen(key[i*2]);
		redislite_delete_key(cs, key[i*2], size);
	}

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

	redislite_close_database(db);

	return status;
}

int test_append() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	char key[10];
	char value[1026];
	memset(key, 'a', 10);
	value[0] = 'b';
	memset(&value[1], 'c', 999);
	value[9] = 'e';
	value[499] = 'f';
	value[1026] = 'g';

	int r = redislite_page_string_set_key_string(cs, key, 10, value, 9);
	int status = REDISLITE_OK;
	if (r < 0) status = REDISLITE_SKIP;

	redislite_page_string_append_key_string(cs, key, 10, value, 10);

	char *lookup_value;
	int lookup_length;
	size_t found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
	if (found == REDISLITE_OOM) status = REDISLITE_SKIP;
	else {
		if (value[0] != lookup_value[0] || lookup_value[lookup_length-1] != value[9]) {
			printf("Content mismatch on line %d\n", __LINE__);
			status = REDISLITE_ERR;
			goto cleanup;
		}
	}

	redislite_page_string_append_key_string(cs, key, 10, value, 500);
	found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
	if (found == REDISLITE_OOM) status = REDISLITE_SKIP;
	else {
		if (value[0] != lookup_value[0] || lookup_value[lookup_length-1] != value[499]) {
			printf("Content mismatch on line %d\n", __LINE__);
			status = REDISLITE_ERR;
			goto cleanup;
		}
	}

	/* TODO
	redislite_page_string_append_key_string(cs, key, 10, value, 1025);
	found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
	if (found == REDISLITE_OOM) status = REDISLITE_SKIP;
	else {
		if (value[0] != lookup_value[0] || lookup_value[lookup_length-1] != value[1025]) {
			printf("Content mismatch on line %d\n", __LINE__);
			status = REDISLITE_ERR;
			goto cleanup;
		}
	}
	 */

	redislite_page_string_append_key_string(cs, key, 10, value, 1);
	found = redislite_page_string_get_by_keyname(db, cs, key, 10, &lookup_value, &lookup_length);
	if (found == REDISLITE_OOM) status = REDISLITE_SKIP;
	else {
		if (value[0] != lookup_value[0] || lookup_value[lookup_length-1] != value[0]) {
			printf("Content mismatch on line %d\n", __LINE__);
			status = REDISLITE_ERR;
			goto cleanup;
		}
	}

cleanup:
	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	redislite_close_database(db);

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

	free(dummy);

	return 0;
}
