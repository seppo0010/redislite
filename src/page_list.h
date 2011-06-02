typedef struct {
	void *db;
	int left_page;
	int right_page;
	int size;
	int element_alloced;
	char** element;
	int* element_len;
	int free_bytes;
} redislite_page_list;

typedef struct {
	redislite_page_list *list;
	int total_size;
} redislite_page_list_first;

void redislite_write_list(void *_db, unsigned char *data, void *page);
void *redislite_read_list(void *_db, unsigned char *data);
void redislite_free_list(void *_db, void *page);
void redislite_delete_list(void *_cs, void *page);

void redislite_write_list_first(void *_db, unsigned char *data, void *page);
void *redislite_read_list_first(void *_db, unsigned char *data);
void redislite_free_list_first(void *_db, void *page);
void redislite_delete_list_first(void *_cs, void *page);

int redislite_lpush_by_keyname(void *_cs, char *keyname, int keyname_len, char *value, int value_len);
