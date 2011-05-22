#include "public_api.h"
#include "redislite.h"
#include "sds.h"

redislite_reply *redislite_create_reply() {
	redislite_reply *reply = redislite_malloc(sizeof(redislite_reply));
	if (reply == NULL) return NULL;
	reply->type = REDISLITE_REPLY_NIL;
}

static void redislite_free_reply_value(redislite_reply *reply) {
	size_t j;

	switch(reply->type) {
		case REDISLITE_REPLY_INTEGER:
			break; /* Nothing to free */

		case REDISLITE_REPLY_ARRAY:
			for (j = 0; j < reply->elements; j++) 
				if (reply->element[j]) redislite_free_reply(reply->element[j]);
					redislite_free(reply->element);
			break;

		case REDISLITE_REPLY_ERROR:
		case REDISLITE_REPLY_STATUS:
		case REDISLITE_REPLY_STRING:
			redislite_free(reply->str);
			break;
	}    
}

void redislite_free_reply(redislite_reply *reply) {
	redislite_free_reply_value(reply);
	redislite_free(reply);
}

static void redislite_free_params_value(redislite_params *param) {
	redislite_free_reply_value(param);
}

void redislite_free_params(redislite_params *param) {
	redislite_free_params_value(param);
	redislite_free(param);
}

redislite_params *redislite_create_params() {
	return redislite_create_reply();
}



static const char *wrong_arity = "wrong number of arguments for '%s' command";
static const char *unknown_command = "unknown command '%s'";
static const char *wrong_type = "Operation against a key holding the wrong kind of value";
static const char *out_of_memory = "Redislite ran out of memory";
static const char *unknown_error = "Unknown error";
static const char *expected_string = "Value is not a string";
static const char *expected_integer = "value is not an integer or out of range";
static const char *expected_double = "Value is not a double";
static const char *ok = "OK";
static const char *not_implemented_yet = "This command hasn't been implemented on redislite yet";
static const char *implementation_not_planned = "This command hasn't been planned to be implemented on redislite";

static void set_status_message(int status, redislite_reply *reply)
{
	switch (status) {
		case REDISLITE_OK:
			redislite_free_reply_value(reply);
			reply->type = REDISLITE_REPLY_STATUS;
			reply->str = redislite_malloc(strlen(ok));
			if (reply->str == NULL) {
				// todo: what should we do here?!
				reply->type = REDISLITE_REPLY_NIL;
				return;
			}

			memcpy(reply->str, ok, strlen(ok));
			reply->len = strlen(ok);
		}
}

static void set_error_message(int status, redislite_reply *reply)
{
	const char *error = NULL;
	switch (status) {
		case REDISLITE_NOT_FOUND:
		{
			redislite_free_reply_value(reply);
			reply->type = REDISLITE_REPLY_NIL;
			return;
		}
		case REDISLITE_OOM:
		{
			redislite_free_reply_value(reply);
			reply->type = REDISLITE_REPLY_ERROR;
			reply->str = redislite_malloc(strlen(out_of_memory));
			if (reply->str == NULL) {
				// TODO: what should we do here?!
				// may be we should have a static error message for OOM? or a different status?
				// don't do this: set_error_message(REDISLITE_OOM, reply);
				reply->len = 0;
				return;
			}

			memcpy(reply->str, out_of_memory, strlen(out_of_memory));
			reply->len = strlen(out_of_memory);
			return;
		}
		case REDISLITE_WRONG_TYPE:
		{
			error = wrong_type;
			break;
		}
		case REDISLITE_EXPECT_STRING:
		{
			error = expected_string;
			break;
		}
		case REDISLITE_EXPECT_INTEGER:
		{
			error = expected_integer;
			break;
		}
		case REDISLITE_EXPECT_DOUBLE:
		{
			error = expected_double;
			break;
		}
		case REDISLITE_NOT_IMPLEMENTED_YET:
		{
			error = not_implemented_yet;
			break;
		}
		case REDISLITE_IMPLEMENTATION_NOT_PLANNED:
		{
			error = implementation_not_planned;
			break;
		}
		default:
		{
			error = unknown_error;
			break;
		}
	}

	if (error == NULL) {
		// an assert may be?
		redislite_free_reply_value(reply);
		reply->type = REDISLITE_REPLY_NIL;
		return;
	}

	redislite_free_reply_value(reply);
	reply->type = REDISLITE_REPLY_ERROR;
	reply->str = redislite_malloc(strlen(error));
	if (reply->str == NULL) {
		set_error_message(REDISLITE_OOM, reply);
		return;
	}
	memcpy(reply->str, error, strlen(error));
	reply->len = strlen(error);
}

redislite_reply *redislite_get_command(redislite *db, redislite_params *params) 
{
	char *key;
	int len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) return NULL;
	if (params->element[1]->type == REDISLITE_PARAM_STRING) {
		key = params->element[1]->str;
		len = params->element[1]->len;
		int status = redislite_page_string_get_by_keyname(db, NULL, key, len, &reply->str, &reply->len);
		if (status == REDISLITE_OK) {
			reply->type = REDISLITE_REPLY_STRING;
		} else {
			set_error_message(status, reply);
		}
	} else {
		set_error_message(REDISLITE_EXPECT_STRING, reply);
	}
	return reply;
}

redislite_reply *redislite_set_command(redislite *db, redislite_params *params) 
{
	char *key, *value;
	int len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) return NULL;
	if (params->element[1]->type == REDISLITE_PARAM_STRING && params->element[2]->type == REDISLITE_PARAM_STRING) {
		key = params->element[1]->str;
		len = params->element[1]->len;
		value = params->element[2]->str;
		value_len = params->element[2]->len;
		changeset *cs = redislite_create_changeset(db);
		int status = redislite_page_string_set_key_string(cs, key, len, value, value_len);
		redislite_save_changeset(cs);
		redislite_free_changeset(cs);
		set_status_message(status, reply);
	} else {
		set_error_message(REDISLITE_EXPECT_STRING, reply);
	}
	return reply;
}

redislite_reply *redislite_append_command(redislite *db, redislite_params *params) 
{
	char *key, *value;
	int len, value_len;
	redislite_reply *reply = redislite_create_reply();
	if (reply == NULL) return NULL;
	if (params->element[1]->type == REDISLITE_PARAM_STRING && params->element[2]->type == REDISLITE_PARAM_STRING) {
		key = params->element[1]->str;
		len = params->element[1]->len;
		value = params->element[2]->str;
		value_len = params->element[2]->len;
		changeset *cs = redislite_create_changeset(db);
		int status = redislite_page_string_append_key_string(cs, key, len, value, value_len);
		redislite_save_changeset(cs);
		redislite_free_changeset(cs);
		set_status_message(status, reply);
	} else {
		set_error_message(REDISLITE_EXPECT_STRING, reply);
	}
	return reply;
}

redislite_reply *redislite_command_not_implemented_yet(redislite *db, redislite_params *params) 
{
	redislite_reply *reply = redislite_create_reply();
	set_error_message(REDISLITE_NOT_IMPLEMENTED_YET, reply);
	return reply;
}

redislite_reply *redislite_command_implementation_not_planned(redislite *db, redislite_params *params) 
{
	redislite_reply *reply = redislite_create_reply();
	set_error_message(REDISLITE_IMPLEMENTATION_NOT_PLANNED, reply);
	return reply;
}

struct redislite_command redislite_command_table[] = {
	{"get",redislite_get_command,2,0},
	{"set",redislite_set_command,3,0},
	{"setnx",redislite_command_not_implemented_yet,3,0},
	{"setex",redislite_command_implementation_not_planned,4,0},
	{"append",redislite_append_command,3,0},
	{"strlen",redislite_command_not_implemented_yet,2,0},
	{"del",redislite_command_not_implemented_yet,2,0},
	{"exists",redislite_command_not_implemented_yet,2,0},
	{"setbit",redislite_command_not_implemented_yet,4,0},
	{"getbit",redislite_command_not_implemented_yet,3,0},
	{"setrange",redislite_command_not_implemented_yet,4,0},
	{"getrange",redislite_command_not_implemented_yet,4,0},
	{"substr",redislite_command_not_implemented_yet,4,0},
	{"incr",redislite_command_not_implemented_yet,2,0},
	{"decr",redislite_command_not_implemented_yet,2,0},
	{"mget",redislite_command_not_implemented_yet,2,0},
	{"rpush",redislite_command_not_implemented_yet,3,0},
	{"lpush",redislite_command_not_implemented_yet,3,0},
	{"rpushx",redislite_command_not_implemented_yet,3,0},
	{"lpushx",redislite_command_not_implemented_yet,3,0},
	{"linsert",redislite_command_not_implemented_yet,5,0},
	{"rpop",redislite_command_not_implemented_yet,2,0},
	{"lpop",redislite_command_not_implemented_yet,2,0},
	{"brpop",redislite_command_implementation_not_planned,3,0},
	{"brpoplpush",redislite_command_implementation_not_planned,4,0},
	{"blpop",redislite_command_implementation_not_planned,3,0},
	{"llen",redislite_command_not_implemented_yet,2,0},
	{"lindex",redislite_command_not_implemented_yet,3,0},
	{"lset",redislite_command_not_implemented_yet,4,0},
	{"lrange",redislite_command_not_implemented_yet,4,0},
	{"ltrim",redislite_command_not_implemented_yet,4,0},
	{"lrem",redislite_command_not_implemented_yet,4,0},
	{"rpoplpush",redislite_command_not_implemented_yet,3,0},
	{"sadd",redislite_command_not_implemented_yet,3,0},
	{"srem",redislite_command_not_implemented_yet,3,0},
	{"smove",redislite_command_implementation_not_planned,4,0},
	{"sismember",redislite_command_not_implemented_yet,3,0},
	{"scard",redislite_command_not_implemented_yet,2,0},
	{"spop",redislite_command_not_implemented_yet,2,0},
	{"srandmember",redislite_command_not_implemented_yet,2,0},
	{"sinter",redislite_command_not_implemented_yet,2,0},
	{"sinterstore",redislite_command_not_implemented_yet,3,0},
	{"sunion",redislite_command_not_implemented_yet,2,0},
	{"sunionstore",redislite_command_not_implemented_yet,3,0},
	{"sdiff",redislite_command_not_implemented_yet,2,0},
	{"sdiffstore",redislite_command_not_implemented_yet,3,0},
	{"smembers",redislite_command_not_implemented_yet,2,0},
	{"zadd",redislite_command_not_implemented_yet,4,0},
	{"zincrby",redislite_command_not_implemented_yet,4,0},
	{"zrem",redislite_command_not_implemented_yet,3,0},
	{"zremrangebyscore",redislite_command_not_implemented_yet,4,0},
	{"zremrangebyrank",redislite_command_not_implemented_yet,4,0},
	{"zunionstore",redislite_command_not_implemented_yet,4,0},
	{"zinterstore",redislite_command_not_implemented_yet,4,0},
	{"zrange",redislite_command_not_implemented_yet,4,0},
	{"zrangebyscore",redislite_command_not_implemented_yet,4,0},
	{"zrevrangebyscore",redislite_command_not_implemented_yet,4,0},
	{"zcount",redislite_command_not_implemented_yet,4,0},
	{"zrevrange",redislite_command_not_implemented_yet,4,0},
	{"zcard",redislite_command_not_implemented_yet,2,0},
	{"zscore",redislite_command_not_implemented_yet,3,0},
	{"zrank",redislite_command_not_implemented_yet,3,0},
	{"zrevrank",redislite_command_not_implemented_yet,3,0},
	{"hset",redislite_command_not_implemented_yet,4,0},
	{"hsetnx",redislite_command_not_implemented_yet,4,0},
	{"hget",redislite_command_not_implemented_yet,3,0},
	{"hmset",redislite_command_not_implemented_yet,4,0},
	{"hmget",redislite_command_not_implemented_yet,3,0},
	{"hincrby",redislite_command_not_implemented_yet,4,0},
	{"hdel",redislite_command_not_implemented_yet,3,0},
	{"hvals",redislite_command_not_implemented_yet,2,0},
	{"hgetall",redislite_command_not_implemented_yet,2,0},
	{"hexists",redislite_command_not_implemented_yet,3,0},
	{"incrby",redislite_command_not_implemented_yet,3,0},
	{"decrby",redislite_command_not_implemented_yet,3,0},
	{"getset",redislite_command_not_implemented_yet,3,0},
	{"mset",redislite_command_not_implemented_yet,3,0},
	{"msetnx",redislite_command_not_implemented_yet,3,0},
	{"randomkey",redislite_command_not_implemented_yet,1,0},
	{"select",redislite_command_implementation_not_planned,2,0},
	{"move",redislite_command_implementation_not_planned,3,0},
	{"rename",redislite_command_not_implemented_yet,3,0},
	{"renamenx",redislite_command_not_implemented_yet,3,0},
	{"expire",redislite_command_implementation_not_planned,3,0},
	{"expireat",redislite_command_implementation_not_planned,3,0},
	{"keys",redislite_command_not_implemented_yet,2,0},
	{"dbsize",redislite_command_not_implemented_yet,1,0},
	{"auth",redislite_command_implementation_not_planned,2,0},
	{"ping",redislite_command_implementation_not_planned,1,0},
	{"echo",redislite_command_not_implemented_yet,2,0},
	{"save",redislite_command_implementation_not_planned,1,0},
	{"bgsave",redislite_command_implementation_not_planned,1,0},
	{"bgrewriteaof",redislite_command_implementation_not_planned,1,0},
	{"shutdown",redislite_command_implementation_not_planned,1,0},
	{"lastsave",redislite_command_implementation_not_planned,1,0},
	{"type",redislite_command_not_implemented_yet,2,0},
	{"multi",redislite_command_not_implemented_yet,1,0},
	{"exec",redislite_command_not_implemented_yet,1,0},
	{"discard",redislite_command_not_implemented_yet,1,0},
	{"sync",redislite_command_implementation_not_planned,1,0},
	{"flushdb",redislite_command_implementation_not_planned,1,0},
	{"flushall",redislite_command_not_implemented_yet,1,0},
	{"sort",redislite_command_not_implemented_yet,2,0},
	{"info",redislite_command_not_implemented_yet,1,0},
	{"monitor",redislite_command_implementation_not_planned,1,0},
	{"ttl",redislite_command_implementation_not_planned,2,0},
	{"persist",redislite_command_implementation_not_planned,2,0},
	{"slaveof",redislite_command_implementation_not_planned,3,0},
	{"debug",redislite_command_implementation_not_planned,2,0},
	{"config",redislite_command_implementation_not_planned,2,0},
	{"subscribe",redislite_command_implementation_not_planned,2,0},
	{"unsubscribe",redislite_command_implementation_not_planned,1,0},
	{"psubscribe",redislite_command_implementation_not_planned,2,0},
	{"punsubscribe",redislite_command_implementation_not_planned,1,0},
	{"publish",redislite_command_implementation_not_planned,3,0},
	{"watch",redislite_command_not_implemented_yet,2,0},
	{"unwatch",redislite_command_not_implemented_yet,1,0}
};

static int memcaseequal(const char *str1, const char *str2, size_t length)
{   
	int i;

	for (i = 0; i < length; i++)
	{   
		if (str1[i] == str2[i]) continue;
		if (str1[i] < str2[i]) {
			if (str1[i] < 'A' || str1[i] > 'Z') return 0;
			if (str2[i] < 'a' || str2[i] > 'z') return 0;
			if (str1[i] + 'a' - 'A' != str2[i]) return 0;
		}   
		if (str1[i] > str2[i]) {
			if (str2[i] < 'A' || str2[i] > 'Z') return 0;
			if (str1[i] < 'a' || str1[i] > 'z') return 0;
			if (str1[i] + 'A' - 'a' != str2[i]) return 0;
		}   
	}   
	return 1;
}

struct redislite_command* redislite_command_lookup(char *command, int length)
{
	if (length < 2) return NULL;
	//redislite_command_table
	if (command[0] > 122 || command[0] < 65) return NULL;
	if (command[1] > 122 || command[1] < 65) return NULL;
	if (command[2] > 122 || command[2] < 65) return NULL;
	int sum = (int)command[0] + (int)command[1] + (int)command[2];
	if (command[0] > 90) sum += 'A' - 'a';
	if (command[1] > 90) sum += 'A' - 'a';
	if (command[2] > 90) sum += 'A' - 'a';
	switch (sum) {
		case 224:
			if (length == 3 && memcaseequal(command, "get", 3)) {
				return &redislite_command_table[0];
			}
			break;

		case 236:
			if (length == 3 && memcaseequal(command, "set", 3)) {
				return &redislite_command_table[1];
			}
			break;
	}
	return NULL;
}

/* Helper function for redislitev_format_command(). */
static int add_argument(redislite_params *target, char *str, int len) {
	if (target->elements == 0)
	{
		target->type = REDISLITE_PARAM_ARRAY;
		target->element = redislite_malloc(sizeof(redislite_params*) * 1);
		if (target->element == NULL) {
			redislite_free_params_value(target);
			return REDISLITE_OOM;
		}
	} else {
		redislite_params** element = redislite_realloc(target->element, sizeof(redislite_params*) * (target->elements+1));
		if (element == NULL) {
			redislite_free_params_value(target);
			return REDISLITE_OOM;
		}
		target->element = element;
	}
	redislite_params *param = redislite_create_params();
	param->type = REDISLITE_PARAM_STRING;
	param->str = redislite_malloc(sizeof(char) * len);
	if (param->str == NULL) return REDISLITE_OOM;
	memcpy(param->str, str, len);
	param->len = len;
	target->element[target->elements++] = param;
	return REDISLITE_OK;
}

int redislitev_format_command(redislite_params **target, const char *format, va_list ap) {
    size_t size;
    const char *arg, *c = format;
    redislite_params *cmd = NULL; /* final command */
    int pos; /* position in final command */
    sds current; /* current argument */
    int interpolated = 0; /* did we do interpolation on an argument? */
    int totlen = 0;

    /* Abort if there is not target to set */
    if (target == NULL)
        return -1;

    /* Build the command string accordingly to protocol */
    cmd = redislite_create_params();
	cmd->type = REDISLITE_PARAM_ARRAY;
	cmd->elements = 0;
	current = sdsempty();
	if (current == NULL) {
		return REDISLITE_OOM;
	}
    while(*c != '\0') {
        if (*c != '%' || c[1] == '\0') {
            if (*c == ' ') {
                if (sdslen(current) != 0) {
                    add_argument(cmd, current, sdslen(current));
					sdsfree(current);
                    current = sdsempty();
                    interpolated = 0;
                }
            } else {
                current = sdscatlen(current,c,1);
            }
        } else {
            switch(c[1]) {
            case 's':
                arg = va_arg(ap,char*);
                size = strlen(arg);
                if (size > 0)
                    current = sdscatlen(current,arg,size);
                interpolated = 1;
                break;
            case 'b':
                arg = va_arg(ap,char*);
                size = va_arg(ap,size_t);
                if (size > 0)
                    current = sdscatlen(current,arg,size);
                interpolated = 1;
                break;
            case '%':
                current = sdscat(current,"%");
                break;
            default:
                /* Try to detect printf format */
                {
                    char _format[16];
                    const char *_p = c+1;
                    size_t _l = 0;
                    va_list _cpy;

                    /* Flags */
                    if (*_p != '\0' && *_p == '#') _p++;
                    if (*_p != '\0' && *_p == '0') _p++;
                    if (*_p != '\0' && *_p == '-') _p++;
                    if (*_p != '\0' && *_p == ' ') _p++;
                    if (*_p != '\0' && *_p == '+') _p++;

                    /* Field width */
                    while (*_p != '\0' && isdigit(*_p)) _p++;

                    /* Precision */
                    if (*_p == '.') {
                        _p++;
                        while (*_p != '\0' && isdigit(*_p)) _p++;
                    }

                    /* Modifiers */
                    if (*_p != '\0') {
                        if (*_p == 'h' || *_p == 'l') {
                            /* Allow a single repetition for these modifiers */
                            if (_p[0] == _p[1]) _p++;
                            _p++;
                        }
                    }

                    /* Conversion specifier */
                    if (*_p != '\0' && strchr("diouxXeEfFgGaA",*_p) != NULL) {
                        _l = (_p+1)-c;
                        if (_l < sizeof(_format)-2) {
                            memcpy(_format,c,_l);
                            _format[_l] = '\0';
                            va_copy(_cpy,ap);
                            current = sdscatvprintf(current,_format,_cpy);
                            interpolated = 1;
                            va_end(_cpy);

                            /* Update current position (note: outer blocks
                             * increment c twice so compensate here) */
                            c = _p-1;
                        }
                    }

                    /* Consume and discard vararg */
                    va_arg(ap,void);
                }
            }
            c++;
        }
        c++;
    }

    /* Add the last argument if needed */
    if (interpolated || sdslen(current) != 0) {
		add_argument(cmd, current, sdslen(current));
    }
	sdsfree(current);

    *target = cmd;
    return totlen;
}

/* Format a command according to the Redis protocol. This function
 * takes a format similar to printf:
 *
 * %s represents a C null terminated string you want to interpolate
 * %b represents a binary safe string
 *
 * When using %b you need to provide both the pointer to the string
 * and the length in bytes. Examples:
 *
 * len = redislite_format_command(target, "GET %s", mykey);
 * len = redislite_format_command(target, "SET %s %b", mykey, myval, myvallen);
 */
int redislite_format_command(redislite_params **target, const char *format, ...) {
    va_list ap;
    int len;
    va_start(ap,format);
    len = redislitev_format_command(target,format,ap);
    va_end(ap);
    return len;
}

/* Format a command according to the Redis protocol. This function takes the
 * number of arguments, an array with arguments and an array with their
 * lengths. If the latter is set to NULL, strlen will be used to compute the
 * argument lengths.
 */
int redislite_format_command_argv(char **target, int argc, const char **argv, const size_t *argvlen) {
    char *cmd = NULL; /* final command */
    int pos; /* position in final command */
    size_t len;
    int totlen, j;

    /* Calculate number of bytes needed for the command */
    totlen = 1+intlen(argc)+2;
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        totlen += 1+intlen(len)+2+len+2;
    }

    /* Build the command at protocol level */
    cmd = malloc(totlen+1);
    if (!cmd) {
		return REDISLITE_OOM;
	}
    pos = sprintf(cmd,"*%d\r\n",argc);
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        pos += sprintf(cmd+pos,"$%zu\r\n",len);
        memcpy(cmd+pos,argv[j],len);
        pos += len;
        cmd[pos++] = '\r';
        cmd[pos++] = '\n';
    }
    cmd[totlen] = '\0';
    *target = cmd;
    return totlen;
}

static redislite_reply *execute_command(redislite *db, redislite_params *params)
{
	if (params->type != REDISLITE_PARAM_ARRAY || params->elements < 1) {
		redislite_reply* reply = redislite_create_reply();
		set_error_message(REDISLITE_ERR, reply); // this is more like an assert than an expected error
		return reply;
	}

	struct redislite_command* cmd = redislite_command_lookup(params->element[0]->str, params->element[0]->len);
	if (cmd == NULL) {
		redislite_reply* reply = redislite_create_reply();
		reply->str = redislite_malloc(sizeof(char) * (strlen(unknown_command) + params->element[0]->len - 1));
		char *str = redislite_malloc(sizeof(char) * (params->element[0]->len+1));
		memcpy(str, params->element[0]->str, params->element[0]->len);
		str[params->element[0]->len] = '\0';
		sprintf(reply->str, unknown_command, str);
		redislite_free(str);
		reply->len = strlen(unknown_command) + params->element[0]->len - 2;
		reply->type = REDISLITE_REPLY_ERROR;
		return reply;
	}

	if ((cmd->arity > 0 && cmd->arity != params->elements) || ((int)params->elements < -cmd->arity)) {
		redislite_reply* reply = redislite_create_reply();
		reply->str = redislite_malloc(sizeof(char) * (strlen(wrong_arity) + params->element[0]->len - 1));
		char *str = redislite_malloc(sizeof(char) * (params->element[0]->len+1));
		memcpy(str, params->element[0]->str, params->element[0]->len);
		str[params->element[0]->len] = '\0';
		sprintf(reply->str, unknown_command, str);
		redislite_free(str);
		reply->len = strlen(wrong_arity) + params->element[0]->len - 2;
		reply->type = REDISLITE_REPLY_ERROR;
		return reply;
	}
	redislite_reply *reply = cmd->proc(db, params);
	return reply;
}

redislite_reply *redislite_command(redislite *db, char *command)
{
	redislite_params* params;
	int status = redislite_format_command(&params, command);
	if (status != REDISLITE_OK) {
		redislite_reply* reply = redislite_create_reply();
		set_error_message(status, reply);
		return reply;
	}

	redislite_reply *reply = execute_command(db, params);
	redislite_free_params(params);
	return reply;
}

redislite_reply *redislite_command_argv(redislite *db, int argc, const char **argv, const size_t *argvlen)
{
	int i, j;
	redislite_params *params = redislite_malloc(sizeof(redislite_params));
	params->type = REDISLITE_PARAM_ARRAY;
	params->elements = argc;
	params->element = redislite_malloc(sizeof(redislite_params*) * argc);
	for (i=0; i < argc; i++) {
		params->element[i] = redislite_malloc(sizeof(redislite_params));
		if (params->element[i] == NULL) {
			for (j=0;j<i;j++) {
				redislite_free(params->element[j]);
			}
			redislite_free(params);
			return NULL;
		}
		params->element[i]->type = REDISLITE_PARAM_STRING;
		// FIXME: we know this isn't gonna get modified, but it is not declared as const... should it?
		params->element[i]->str = (char*)argv[i];
		params->element[i]->len = argvlen[i];
	}

	redislite_reply *reply = execute_command(db, params);

	for (i=0; i < argc; i++) {
		redislite_free(params->element[i]);
	}
	redislite_free(params->element);
	redislite_free(params);
	return reply;
}
