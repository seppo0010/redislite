#ifndef _REDISLITE_H
#define _REDISLITE_H

#include <stdio.h>
#include "page.h"
#include "memory.h"

#define HEADER_STRING "Redislite format 1"
#define DEFAULT_PAGE_SIZE 512
#define DEFAULT_MODIFIED_PAGE_SIZE 4
#define DEFAULT_OPENED_PAGE_SIZE 32
#define WRITE_FORMAT_VERSION 1
#define READ_FORMAT_VERSION 1

typedef struct {
	char *filename;
	FILE *file;
	int page_size;
	int file_change_counter;
	int number_of_pages;
	int first_freelist_page;
	int number_of_freelist_pages;
	void *root;

	int readonly;

	void **types; // types and handlers
} redislite;

typedef struct {
	redislite *db;

	int opened_pages_length;
	int opened_pages_free;
	void **opened_pages;

	int modified_pages_length;
	int modified_pages_free;
	void **modified_pages;
} changeset;

changeset *redislite_create_changeset(redislite *db);
void redislite_free_changeset(changeset *cs);
int redislite_save_changeset(changeset *cs);
redislite *redislite_create_database(const char *filename);
redislite *redislite_open_database(const char *filename);
void redislite_close_database(redislite *db);
unsigned char *redislite_read_page(redislite *db, changeset *cs, int num);
int redislite_add_modified_page(changeset *cs, int page_number, char type, void *page_data);
int redislite_add_opened_page(changeset *cs, int page_number, char type, void *page_data);

#define REDISLITE_OK 0
#define REDISLITE_ERR -1
#define REDISLITE_OOM -2
#define REDISLITE_READONLY -3
#define REDISLITE_SKIP -4
#define REDISLITE_NOT_FOUND -5
#define REDISLITE_WRONG_TYPE -6
#define REDISLITE_EXPECT_STRING -7
#define REDISLITE_EXPECT_INTEGER -8
#define REDISLITE_EXPECT_DOUBLE -9
#define REDISLITE_NOT_IMPLEMENTED_YET -10
#define REDISLITE_IMPLEMENTATION_NOT_PLANNED -11
#define REDISLITE_BIT_OFFSET_INVALID -12
#define REDISLITE_BIT_INVALID -13

#endif
