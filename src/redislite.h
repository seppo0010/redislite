#ifndef _REDISLITE_H
#define _REDISLITE_H

#include <stdio.h>
#include "page.h"
#include "memory.h"

typedef struct {
	char *filename;
	FILE *file;
	size_t page_size;
	int file_change_counter;
	int number_of_pages;
	int first_freelist_page;
	int number_of_freelist_pages;
	int number_of_keys;
	void *root;

	int readonly;

	void **types; // types and handlers
} redislite;

redislite *redislite_create_database(const char *filename);
redislite *redislite_open_database(const char *filename);
void redislite_close_database(redislite *db);

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
#define REDISLITE_SOURCE_DESTINATION_SAME -14

#endif
