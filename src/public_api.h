#ifndef _REDISLITE_PUBLIC_API_H
#define _REDISLITE_PUBLIC_API_H
#include "redislite.h"
#include <string.h>
#include <stdarg.h>

#define REDISLITE_REPLY_STRING 1
#define REDISLITE_REPLY_ARRAY 2
#define REDISLITE_REPLY_INTEGER 3
#define REDISLITE_REPLY_NIL 4
#define REDISLITE_REPLY_STATUS 5
#define REDISLITE_REPLY_ERROR 6

typedef struct redislite_reply {
	int type; /* REDISLITE_REPLY_* */
	long long integer; /* The integer when type is REDISLITE_REPLY_INTEGER */
	int len; /* Length of string */
	char *str; /* Used for both REDISLITE_REPLY_ERROR and REDISLITE_REPLY_STRING */
	size_t elements; /* number of elements, for REDISLITE_REPLY_ARRAY */
	struct redislite_reply **element; /* elements vector for REDISLITE_REPLY_ARRAY */
} redislite_reply;

typedef struct {
	int must_free_argv;
	int argc;
	char **argv;
	size_t *argvlen;
} redislite_params;

redislite_reply *redislite_create_reply();
void redislite_free_reply(redislite_reply *reply);
redislite_params *redislite_create_params();
void redislite_free_params(redislite_params *params);
redislite_reply *redislite_get_command(redislite *db, redislite_params *params);
redislite_reply *redislite_set_command(redislite *db, redislite_params *params);
int redislitev_format_command(redislite_params **target, const char *format, va_list ap);
int redislite_format_command(redislite_params **target, const char *format, ...);
redislite_reply *redislite_command(redislite *db, char *command);
redislite_reply *redislite_command_argv(redislite *db, int argc, const char **argv, const size_t *argvlen);
redislite_reply *redislite_execute_command(redislite *db, redislite_params *params);

typedef redislite_reply *redislite_command_proc(redislite *c, redislite_params *params);
struct redislite_command {
	char *name;
	redislite_command_proc *proc;
	int arity;
	int flags;
};
#endif
