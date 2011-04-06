#include "redislite.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *test_add_key(redislite *db, int *left)
{
	int rnd = rand();
	char *key = (char*)malloc(sizeof(char) * 14);
	sprintf(key, "%d", rnd);
	int size = strlen(key);

	char *data = malloc(sizeof(char) * db->page_size);
	memset(data, 0, 14);
	sprintf(data, "%d", rnd);
	redislite_insert_string(db, data, strlen(data), left);
	redislite_insert_key(db, key, size, *left);
	return key;
}

#define SIZE 200

int main() {
	srand(4);
	redislite *db = redislite_open_database("test.db");
	int i;

	char *key[SIZE];
	int value[SIZE];
	for (i=0; i < SIZE; i++)
		key[i] = test_add_key(db, &value[i]);

	for (i=0; i < SIZE; i++)
		if (value[i] != redislite_value_page_for_key(db, key[i], strlen(key[i])))
			printf("%s %d %d\n", key[i], value[i], redislite_value_page_for_key(db, key[i], strlen(key[i])));

	for (i=0; i < SIZE; i++)
		free(key[i]);

	redislite_close_database(db);
	return 0;
}
