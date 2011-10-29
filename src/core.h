#include "redislite.h"
#include "page_index.h"
#include "page_first.h"

#define HEADER_STRING "Redislite format 1"
#define DEFAULT_PAGE_SIZE 512
#define DEFAULT_MODIFIED_PAGE_SIZE 4
#define DEFAULT_OPENED_PAGE_SIZE 32
#define WRITE_FORMAT_VERSION 1
#define READ_FORMAT_VERSION 1

typedef struct {
	redislite *db;

	size_t opened_pages_length;
	size_t opened_pages_free;
	void **opened_pages;

	size_t modified_pages_length;
	size_t modified_pages_free;
	void **modified_pages;
} changeset;

changeset *redislite_create_changeset(redislite *db);
void redislite_free_changeset(changeset *cs);
int redislite_save_changeset(changeset *cs);
unsigned char *redislite_read_page(redislite *db, changeset *cs, int num);
redislite_page *redislite_modified_page(changeset *cs, int page_number);
int redislite_add_modified_page(changeset *cs, int page_number, char type, void *page_data);
int redislite_add_opened_page(changeset *cs, int page_number, char type, void *page_data);
int redislite_set_root(redislite *db, redislite_page_index_first *page);
int redislite_close_opened_page(changeset *cs, int page_number);
