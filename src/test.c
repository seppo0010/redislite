#include "redislite.h"
#include "page_string.h"
#include "page_index.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *test_add_key(changeset *cs, int *left)
{
	int rnd = rand();
	char *key = (char*)malloc(sizeof(char) * 14);
	sprintf(key, "%d", rnd);
	int size = (int)strlen(key);

	char *data = malloc(sizeof(char) * cs->db->page_size+1);
	memset(data, key[0], cs->db->page_size+1);
	int i;
	for (i=0;i<cs->db->page_size+1;i++) {
		data[i] = (char)(((int)key[0] + i) % 256);
	}

	int insert = redislite_page_string_set_key_string(cs, key, size, data, cs->db->page_size+1);
	if (insert != REDISLITE_OK) {
		free(key);
		key = NULL;
	}
	free(data);
	return key;
}

#define SIZE 200

int test_insert_and_find() {
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	if (db == NULL) { printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	changeset *cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return REDISLITE_SKIP; }
	int i, j;

	char *key[SIZE];
	int value[SIZE];
	for (i=0; i < SIZE; i++)
		key[i] = test_add_key(cs, &value[i]);

	for (i=0; i < SIZE; i++) {
		if (key[i] == NULL) continue;
		int length = 0;
		char *value = NULL;
		size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], (int)strlen(key[i]), &value, &length);
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
		for (j=0; j < SIZE; j++) {
			if (value[j] != (char)(((int)key[i][0] + j) % 256)) {
				printf("Content mismatch\n");
				break;
			}
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
			size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], (int)strlen(key[i]), &value, &length);
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
			for (j=0; j < SIZE; j++) {
				if (value[j] != (char)(((int)key[i][0] + j) % 256))
					printf("Content mismatch\n");
			}

			free(value);
		}
	}

	for (i=0; i < SIZE; i++)
		if (key[i] != NULL)
			free(key[i]);
	redislite_close_database(db);
	// TODO: check for leaks
	return REDISLITE_OK;
}

int test_delete_and_find() {
/*
	cs = redislite_create_changeset(db);
	if (cs == NULL) { redislite_close_database(db); printf("OOM on test.c, on line %d\n", __LINE__); return 0; }
	for (i=0; i < SIZE/2; i++) {
		redislite_delete_key(cs, key[i*2], (int)strlen(key[i*2]));
	}
	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	cs = NULL; // using stored values

	for (i=0; i < SIZE; i++) {
		if (key[i] == NULL) continue;
		int length = 0;
		char *value = NULL;
		size_t found = redislite_page_string_get_by_keyname(db, cs, key[i], (int)strlen(key[i]), &value, &length);
		if (found == REDISLITE_OOM) {
			continue;
		}

		if (i % 2 == 0 && found != REDISLITE_ERR) {
			printf("Key '%s' found after deleted\n", key[i]);
		} else if (i % 2 == 1 && found == REDISLITE_ERR) {
			printf("Key '%s' not found\n", key[i]);
		}
		free(value);
	}
*/
	return REDISLITE_ERR;
}

int main() {
	srand(4);
	int test;
	const char * test_name;

	test = test_insert_and_find();
	test_name = "Insert and Find";
	if (test == REDISLITE_SKIP) {
		printf("Skipped test %s on line %d", test_name, __LINE__);
	} else if (test != REDISLITE_OK) {
		printf("Failed test %s on line %d", test_name, __LINE__);
	}

	return 0;
}
