#include "redislite.h"

int main() {
	redislite *db = redislite_open_database("test.db");
	redislite_close_database(db);
	return 0;
}
