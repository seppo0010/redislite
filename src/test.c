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

	*left = redislite_add_modified_page(db, -1, redislite_page_type_data, NULL);
	redislite_insert_key(db, key, size, *left);
	return key;
}

int main() {
	redislite *db = redislite_open_database("test.db");
	int i;

	char *key[100];
	int value[100];
	for (i=0; i < 100; i++)
		key[i] = test_add_key(db, &value[i]);

/*
	for (i=0; i < 100; i++)
		printf("%d\n", value[i] == redislite_value_page_for_key(db, key[i], strlen(key[i])));
*/

	redislite_close_database(db);
	return 0;
}
