typedef struct {
	void *db;
	int left_page;
	int right_page;
	int size;
	int element_alloced;
	char **element;
	int *element_len;
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

size_t redislite_free_bytes(void *_db, redislite_page_list *list, char type);

int redislite_lpush_by_keyname(void *_cs, char *keyname, int keyname_len, char *value, int value_len);
int redislite_lpop_by_keyname(void *_cs, char *keyname, int keyname_len, char **value, int *value_len);
int redislite_llen_by_keyname(void *_db, void *_cs, char *keyname, int keyname_len, int *len);
int redislite_lrange_by_keyname(void *_db, void *_cs, char *keyname, int keyname_len, int start, int end, int *list_count, char ***list, int **list_len);
int redislite_lindex_by_keyname(void *_db, void *_cs, char *keyname, int keyname_len, int pos, char **value, int *value_len);
