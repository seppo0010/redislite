#include "redislite.h"
#include "page_string.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *test_add_key(changeset *cs, int *left)
{
	int rnd = rand();
	char *key = (char*)malloc(sizeof(char) * 14);
	sprintf(key, "%d", rnd);
	int size = strlen(key);

	char *data = malloc(sizeof(char) * cs->db->page_size+1);
	memset(data, key[0], cs->db->page_size+1);
	int i;
	for (i=0;i<cs->db->page_size+1;i++) {
		data[i] = (char)(((int)key[0] + i) % 256);
	}
	redislite_insert_string(cs, data, cs->db->page_size+1, left);
	redislite_insert_key(cs, key, size, *left);
	free(data);
	return key;
}

#define SIZE 200

int main() {
	srand(4);
	remove("test.db");
	redislite *db = redislite_open_database("test.db");
	changeset *cs = redislite_create_changeset(db);
	int i, j;

	char *key[SIZE];
	int value[SIZE];
	for (i=0; i < SIZE; i++)
		key[i] = test_add_key(cs, &value[i]);

	for (i=0; i < SIZE; i++)
		if (value[i] != redislite_value_page_for_key(db, cs, key[i], strlen(key[i])))
			printf("%d %s %d %d\n", i, key[i], value[i], redislite_value_page_for_key(db, cs, key[i], strlen(key[i])));

	for (i=0; i < SIZE; i++) {
		int length = 0;
		char *value = redislite_page_string_get_by_keyname(db, cs, key[i], strlen(key[i]), &length);
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
		redislite_value_page_for_key(db, cs, key[57], strlen(key[57]));
		for (i=0; i < SIZE; i++) {
			int v = redislite_value_page_for_key(db, cs, key[i], strlen(key[i]));
			if (value[i] != v)
				printf("%d %s %d %d\n", i, key[i], value[i], v);
		}

		for (i=0; i < SIZE; i++) {
			int length = 0;
			char *value = redislite_page_string_get_by_keyname(db, cs, key[i], strlen(key[i]), &length);
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
		free(key[i]);

	redislite_close_database(db);
	return 0;
}
