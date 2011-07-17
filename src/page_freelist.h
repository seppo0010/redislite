typedef struct {
	void *db;
	int right_page;
	size_t size;
} redislite_page_freelist;

void redislite_write_freelist(void *_db, unsigned char *data, void *page);
void *redislite_read_freelist(void *_db, unsigned char *data);
void redislite_free_freelist(void *_db, void *page);
