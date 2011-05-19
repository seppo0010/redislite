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


static const char *wrong_type = "Operation against a key holding the wrong kind of value";
static const char *out_of_memory = "Redislite ran out of memory";
static const char *unknown_error = "Unknown error";
static const char *expected_string = "Value is not a string";
static const char *expected_integer = "value is not an integer or out of range";
static const char *expected_double = "Value is not a double";
static const char *ok = "OK";

static void set_status_message(int status, redislite_reply *reply)
{
	switch (status) {
		case REDISLITE_OK:
			redislite_free_reply_value(reply);
			reply->type = REDISLITE_REPLY_STATUS;
			reply->str = redislite_malloc(sizeof(ok)-1);
			if (reply->str == NULL) {
				// todo: what should we do here?!
				reply->type = REDISLITE_REPLY_NIL;
				return;
			}

			memcpy(reply->str, ok, sizeof(ok)-1);
			reply->len = sizeof(ok)-1;
		}
}

static void set_error_message(int status, redislite_reply *reply)
{
	// FIXME: refactor needed, too much copy paste
	switch (status) {
		case REDISLITE_NOT_FOUND:
		{
			reply->type = REDISLITE_REPLY_NIL;
			break;
		}
		case REDISLITE_WRONG_TYPE:
		{
			reply->type = REDISLITE_REPLY_ERROR;
			redislite_free_reply_value(reply);
			reply->str = redislite_malloc(sizeof(wrong_type)-1);
			if (reply->str == NULL) {
				set_error_message(REDISLITE_OOM, reply);
				return;
			}

			memcpy(reply->str, wrong_type, sizeof(wrong_type)-1);
			reply->len = sizeof(wrong_type)-1;
		}
		case REDISLITE_OOM:
		{
			reply->type = REDISLITE_REPLY_ERROR;
			redislite_free_reply_value(reply);
			reply->str = redislite_malloc(sizeof(out_of_memory)-1);
			if (reply->str == NULL) {
				// todo: what should we do here?!
				// may be we should have a static error message for OOM? or a different status?
				// don't do this: set_error_message(REDISLITE_OOM, reply);
				reply->type = REDISLITE_REPLY_NIL;
				return;
			}

			memcpy(reply->str, out_of_memory, sizeof(out_of_memory)-1);
			reply->len = sizeof(out_of_memory)-1;
		}
		case REDISLITE_EXPECT_STRING:
		{
			reply->type = REDISLITE_REPLY_ERROR;
			redislite_free_reply_value(reply);
			reply->str = redislite_malloc(sizeof(expected_string)-1);
			if (reply->str == NULL) {
				set_error_message(REDISLITE_OOM, reply);
				return;
			}

			memcpy(reply->str, expected_string, sizeof(expected_string)-1);
			reply->len = sizeof(expected_string)-1;
		}
		case REDISLITE_EXPECT_INTEGER:
		{
			reply->type = REDISLITE_REPLY_ERROR;
			redislite_free_reply_value(reply);
			reply->str = redislite_malloc(sizeof(expected_integer)-1);
			if (reply->str == NULL) {
				set_error_message(REDISLITE_OOM, reply);
				return;
			}

			memcpy(reply->str, expected_integer, sizeof(expected_integer)-1);
			reply->len = sizeof(expected_integer)-1;
		}
		case REDISLITE_EXPECT_DOUBLE:
		{
			reply->type = REDISLITE_REPLY_ERROR;
			redislite_free_reply_value(reply);
			reply->str = redislite_malloc(sizeof(expected_double)-1);
			if (reply->str == NULL) {
				set_error_message(REDISLITE_OOM, reply);
				return;
			}

			memcpy(reply->str, expected_double, sizeof(expected_double)-1);
			reply->len = sizeof(expected_double)-1;
		}
		default:
		{
			reply->type = REDISLITE_REPLY_ERROR;
			redislite_free_reply_value(reply);
			reply->str = redislite_malloc(sizeof(unknown_error)-1);
			if (reply->str == NULL) {
				set_error_message(REDISLITE_OOM, reply);
				return;
			}
			memcpy(reply->str, unknown_error, sizeof(unknown_error)-1);
			reply->len = sizeof(unknown_error)-1;
		}
	}
}

redislite_reply *redislite_get_command(redislite *db, redislite_params *params) 
{
	char *key;
	int len;
	redislite_reply *reply = redislite_create_reply();
	if (params->element[0]->type == REDISLITE_PARAM_STRING) {
		key = params->element[0]->str;
		len = params->element[0]->len;
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
	if (params->element[0]->type == REDISLITE_PARAM_STRING && params->element[1]->type == REDISLITE_PARAM_STRING) {
		key = params->element[0]->str;
		len = params->element[0]->len;
		value = params->element[1]->str;
		value_len = params->element[1]->len;
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

struct redislite_command *commandTable;
struct redislite_command redisliteCommandTable[] = {
	{"get",redislite_get_command,2,0},
	{"set",redislite_set_command,3,0} // remember to add a comma here
	// NIY {"setnx",setnxCommand,3,0},
	// NIY {"setex",setexCommand,4,0},
	// NIY {"append",appendCommand,3,0},
	// NIY {"strlen",strlenCommand,2,0},
	// NIY {"del",delCommand,-2,0},
	// NIY {"exists",existsCommand,2,0},
	// NIY {"setbit",setbitCommand,4,0},
	// NIY {"getbit",getbitCommand,3,0},
	// NIY {"setrange",setrangeCommand,4,0},
	// NIY {"getrange",getrangeCommand,4,0},
	// NIY {"substr",getrangeCommand,4,0},
	// NIY {"incr",incrCommand,2,0},
	// NIY {"decr",decrCommand,2,0},
	// NIY {"mget",mgetCommand,-2,0},
	// NIY {"rpush",rpushCommand,3,0},
	// NIY {"lpush",lpushCommand,3,0},
	// NIY {"rpushx",rpushxCommand,3,0},
	// NIY {"lpushx",lpushxCommand,3,0},
	// NIY {"linsert",linsertCommand,5,0},
	// NIY {"rpop",rpopCommand,2,0},
	// NIY {"lpop",lpopCommand,2,0},
	// NIY {"brpop",brpopCommand,-3,0},
	// NIY {"brpoplpush",brpoplpushCommand,4,0},
	// NIY {"blpop",blpopCommand,-3,0},
	// NIY {"llen",llenCommand,2,0},
	// NIY {"lindex",lindexCommand,3,0},
	// NIY {"lset",lsetCommand,4,0},
	// NIY {"lrange",lrangeCommand,4,0},
	// NIY {"ltrim",ltrimCommand,4,0},
	// NIY {"lrem",lremCommand,4,0},
	// NIY {"rpoplpush",rpoplpushCommand,3,0},
	// NIY {"sadd",saddCommand,3,0},
	// NIY {"srem",sremCommand,3,0},
	// NIY {"smove",smoveCommand,4,0},
	// NIY {"sismember",sismemberCommand,3,0},
	// NIY {"scard",scardCommand,2,0},
	// NIY {"spop",spopCommand,2,0},
	// NIY {"srandmember",srandmemberCommand,2,0},
	// NIY {"sinter",sinterCommand,-2,0},
	// NIY {"sinterstore",sinterstoreCommand,-3,0},
	// NIY {"sunion",sunionCommand,-2,0},
	// NIY {"sunionstore",sunionstoreCommand,-3,0},
	// NIY {"sdiff",sdiffCommand,-2,0},
	// NIY {"sdiffstore",sdiffstoreCommand,-3,0},
	// NIY {"smembers",sinterCommand,2,0},
	// NIY {"zadd",zaddCommand,4,0},
	// NIY {"zincrby",zincrbyCommand,4,0},
	// NIY {"zrem",zremCommand,3,0},
	// NIY {"zremrangebyscore",zremrangebyscoreCommand,4,0},
	// NIY {"zremrangebyrank",zremrangebyrankCommand,4,0},
	// NIY {"zunionstore",zunionstoreCommand,-4,0},
	// NIY {"zinterstore",zinterstoreCommand,-4,0},
	// NIY {"zrange",zrangeCommand,-4,0},
	// NIY {"zrangebyscore",zrangebyscoreCommand,-4,0},
	// NIY {"zrevrangebyscore",zrevrangebyscoreCommand,-4,0},
	// NIY {"zcount",zcountCommand,4,0},
	// NIY {"zrevrange",zrevrangeCommand,-4,0},
	// NIY {"zcard",zcardCommand,2,0},
	// NIY {"zscore",zscoreCommand,3,0},
	// NIY {"zrank",zrankCommand,3,0},
	// NIY {"zrevrank",zrevrankCommand,3,0},
	// NIY {"hset",hsetCommand,4,0},
	// NIY {"hsetnx",hsetnxCommand,4,0},
	// NIY {"hget",hgetCommand,3,0},
	// NIY {"hmset",hmsetCommand,-4,0},
	// NIY {"hmget",hmgetCommand,-3,0},
	// NIY {"hincrby",hincrbyCommand,4,0},
	// NIY {"hdel",hdelCommand,3,0},
	// NIY {"hvals",hvalsCommand,2,0},
	// NIY {"hgetall",hgetallCommand,2,0},
	// NIY {"hexists",hexistsCommand,3,0},
	// NIY {"incrby",incrbyCommand,3,0},
	// NIY {"decrby",decrbyCommand,3,0},
	// NIY {"getset",getsetCommand,3,0},
	// NIY {"mset",msetCommand,-3,0},
	// NIY {"msetnx",msetnxCommand,-3,0},
	// NIY {"randomkey",randomkeyCommand,1,0},
	// NIY {"select",selectCommand,2,0},
	// NIY {"move",moveCommand,3,0},
	// NIY {"rename",renameCommand,3,0},
	// NIY {"renamenx",renamenxCommand,3,0},
	// NIY {"expire",expireCommand,3,0},
	// NIY {"expireat",expireatCommand,3,0},
	// NIY {"keys",keysCommand,2,0},
	// NIY {"dbsize",dbsizeCommand,1,0},
	// NIY {"auth",authCommand,2,0},
	// NIY {"ping",pingCommand,1,0},
	// NIY {"echo",echoCommand,2,0},
	// NIY {"save",saveCommand,1,0},
	// NIY {"bgsave",bgsaveCommand,1,0},
	// NIY {"bgrewriteaof",bgrewriteaofCommand,1,0},
	// NIY {"shutdown",shutdownCommand,1,0},
	// NIY {"lastsave",lastsaveCommand,1,0},
	// NIY {"type",typeCommand,2,0},
	// NIY {"multi",multiCommand,1,0},
	// NIY {"exec",execCommand,1,0},
	// NIY {"discard",discardCommand,1,0},
	// NIY {"sync",syncCommand,1,0},
	// NIY {"flushdb",flushdbCommand,1,0},
	// NIY {"flushall",flushallCommand,1,0},
	// NIY {"sort",sortCommand,-2,0},
	// NIY {"info",infoCommand,-1,0},
	// NIY {"monitor",monitorCommand,1,0},
	// NIY {"ttl",ttlCommand,2,0},
	// NIY {"persist",persistCommand,2,0},
	// NIY {"slaveof",slaveofCommand,3,0},
	// NIY {"debug",debugCommand,-2,0},
	// NIY {"config",configCommand,-2,0},
	// NIY {"subscribe",subscribeCommand,-2,0},
	// NIY {"unsubscribe",unsubscribeCommand,-1,0},
	// NIY {"psubscribe",psubscribeCommand,-2,0},
	// NIY {"punsubscribe",punsubscribeCommand,-1,0},
	// NIY {"publish",publishCommand,3,0},
	// NIY {"watch",watchCommand,-2,0},
	// NIY {"unwatch",unwatchCommand,1,0}
};

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

