#include "page.h"
#include "redislite.h"
#include "page_index.h"

void *redislite_page_get(void* _db, int num, redislite_page_type* type) {
	redislite *db = (redislite*)_db;
	unsigned char *data = redislite_read_page(db, num);
	if (data == NULL) return NULL;
	void *result = NULL;
	if (data[0] == 'I') {
		result = redislite_read_index(db, data);
	}
	free(data);
	return result;
}
