typedef struct {
	void *db;
	int right_page;
	char *value;
} redislite_page_string_overflow;

typedef struct {
	void *db;
	int right_page;
	size_t size;
	char *value;
} redislite_page_string;

void redislite_write_string(void *_db, unsigned char *data, void *page);
void *redislite_read_string(void *_db, unsigned char *data);
void redislite_free_string(void *_db, void *page);
void redislite_delete_string(void *_cs, void *page);
void redislite_write_string_overflow(void *_db, unsigned char *data, void *page);
void *redislite_read_string_overflow(void *_db, unsigned char *data);
void redislite_free_string_overflow(void *_db, void *page);
void redislite_delete_string_overflow(void *_cs, void *page);
int redislite_page_string_get_by_keyname(void *_db, void *_cs, char *key_name, size_t key_length, char **str, size_t *length);
int redislite_insert_string(void *_cs, char *str, size_t length, int *num);
int redislite_page_string_getset_key_string(void *_cs, char *key_name, size_t key_length, char *str, size_t length, char **previous_value, size_t *previous_value_length);
int redislite_page_string_set_key_string(void *_cs, char *key_name, size_t key_length, char *str, size_t length);
int redislite_page_string_setnx_key_string(void *_cs, char *key_name, size_t key_length, char *str, size_t length);
int redislite_page_string_strlen_by_keyname(void *_db, void *_cs, char *key_name, size_t key_length);
int redislite_page_string_append_key_string(void *_cs, char *key_name, size_t key_length, char *str, size_t length, size_t *new_length);
int redislite_page_string_incr_by_key_string(void *_cs, char *key_name, size_t key_length, long long incr, long long *new_value);
int redislite_page_string_incr_key_string(void *_cs, char *key_name, size_t key_length, long long *new_value);
int redislite_page_string_decr_by_key_string(void *_cs, char *key_name, size_t key_length, long long decr, long long *new_value);
int redislite_page_string_decr_key_string(void *_cs, char *key_name, size_t key_length, long long *new_value);
int redislite_page_string_strlen_key_string(void *_db, void *_cs, char *key_name, size_t key_length);
int redislite_page_string_getrange_key_string(void *_db, void *_cs, char *key_name, size_t key_length, int _start, int _end, char **str, size_t *str_length);
int redislite_echo(char *src_name, size_t src_length, char **dst_name, size_t *dst_length);
int redislite_page_string_getbit_key_string(void *_db, void *_cs, char *key_name, size_t key_length, long long bitoffset);
int redislite_page_string_setbit_key_string(void *_cs, char *key_name, size_t key_length, long long bitoffset, long on);
