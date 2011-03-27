#ifndef _REDISLITE_H
#define _REDISLITE_H

#include <stdio.h>

typedef struct {
	unsigned char *filename;
	FILE *file;
	int page_size;
	int file_change_counter;
	int number_of_pages;
	int first_freelist_page;
	int number_of_freelist_pages;
	void *root;

	int readonly;

	int modified_pages_length;
	int modified_pages_free;
	void **modified_pages;
} redislite;

redislite* redislite_create_database(const unsigned char *filename);
redislite* redislite_open_database(const unsigned char *filename);
void redislite_close_database(redislite *db);

#define REDISLITE_OK 0
#define REDISLITE_ERR 1
#define REDISLITE_OOM 2

#endif
