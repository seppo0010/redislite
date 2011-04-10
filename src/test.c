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

	char *data = malloc(sizeof(char) * cs->db->page_size);
	memset(data, 0, 14);
	sprintf(data, "%d", rnd);
	redislite_insert_string(cs, data, strlen(data), left);
	redislite_insert_key(cs, key, size, *left);
	free(data);
	return key;
}

#define SIZE 200

int main() {
	srand(4);
	redislite *db = redislite_open_database("test.db");
	changeset *cs = redislite_create_changeset(db);
	int i;

	char *key[SIZE];
	int value[SIZE];
	for (i=0; i < SIZE; i++)
		key[i] = test_add_key(cs, &value[i]);

	for (i=0; i < SIZE; i++)
		if (value[i] != redislite_value_page_for_key(db, cs, key[i], strlen(key[i])))
			printf("%s %d %d\n", key[i], value[i], redislite_value_page_for_key(db, key[i], strlen(key[i])));

	for (i=0; i < SIZE; i++) {
		int length = 0;
		char *value = redislite_page_string_get_by_keyname(db, cs, key[i], strlen(key[i]), &length);
		if (memcmp(key[i],value, length) != 0)
			printf("%s %s %d %d\n", value, key[i], value[i], redislite_value_page_for_key(db, key[i], strlen(key[i])));
	}

	for (i=0; i < SIZE; i++)
		free(key[i]);

	redislite_save_changeset(cs);
	redislite_free_changeset(cs);
	redislite_close_database(db);
	return 0;
}
