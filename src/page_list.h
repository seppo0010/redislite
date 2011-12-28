typedef struct {
	void *db;
	int left_page;
	int right_page;
	size_t size;
	size_t element_alloced;
	char **element;
	size_t *element_len;
} redislite_page_list;

typedef struct {
	redislite_page_list *list;
	size_t total_size;
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

int redislite_lpushx_by_keyname(void *_cs, char *keyname, size_t keyname_len, char *value, size_t value_len);
int redislite_lpush_by_keyname(void *_cs, char *keyname, size_t keyname_len, char *value, size_t value_len);
int redislite_lpush_page_num(void *_cs, int *page_num, char *value, size_t value_len);
int redislite_lpop_by_keyname(void *_cs, char *keyname, size_t keyname_len, char **value, size_t *value_len);
int redislite_llen_by_keyname(void *_db, void *_cs, char *keyname, size_t keyname_len, size_t *len);
int redislite_lrange_by_keyname(void *_db, void *_cs, char *keyname, size_t keyname_len, int start, int end, size_t *list_count, char ***list, size_t **list_len);
int redislite_lindex_by_keyname(void *_db, void *_cs, char *keyname, size_t keyname_len, int pos, char **value, size_t *value_len);

int redislite_rpushx_by_keyname(void *_cs, char *keyname, size_t keyname_len, char *value, size_t value_len);
int redislite_rpush_by_keyname(void *_cs, char *keyname, size_t keyname_len, char *value, size_t value_len);
int redislite_rpush_page_num(void *_cs, int *page_num_p, char *value, size_t value_len);
int redislite_rpop_by_keyname(void *_cs, char *keyname, size_t keyname_len, char **value, size_t *value_len);
int redislite_lset_by_keyname(void *_cs, char *keyname, size_t keyname_len, int pos, char *value, size_t value_len);
int redislite_linsert_by_keyname(void *_cs, char *keyname, size_t keyname_len, int after, char *pivot, size_t pivot_len, char *value, size_t value_len);
int redislite_rpoplpush_by_keyname(void *_cs, char *source, size_t source_len, char *destination, size_t destination_len, char **value, size_t *value_len);
