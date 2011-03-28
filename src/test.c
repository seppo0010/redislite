#include "redislite.h"
#include <string.h>

static void test_add_key(redislite *db)
{
	int rnd = arc4random();
	char key[14];
	sprintf(key, "%d", rnd);
	int size = strlen(key);

	redislite_insert_key(db, key, size, 0);
}

int main() {
	redislite *db = redislite_open_database("test.db");
	int i;
	for (i=0; i < 100; i++)
		test_add_key(db);
	redislite_close_database(db);
	return 0;
}
