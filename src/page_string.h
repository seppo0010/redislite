typedef struct {
	void *db;
	int right_page;
	char* value;
} redislite_page_string_overflow;

typedef struct {
	void *db;
	int right_page;
	int size;
	char* value;
} redislite_page_string;

void redislite_write_string(void *_db, unsigned char *data, void *page);
void *redislite_read_string(void *_db, unsigned char *data);
void redislite_free_string(void *_db, void *page);
void redislite_write_string_overflow(void *_db, unsigned char *data, void *page);
void *redislite_read_string_overflow(void *_db, unsigned char *data);
void redislite_free_string_overflow(void *_db, void *page);
char *redislite_page_string_get_by_keyname(void *_db, char *key_name, int length);
