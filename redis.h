#ifndef URANUS_REDIS
#define URANUS_REDIS

#include <hiredis.h>
#include <adapters/glib.h>

#include "urn.h"

int urn_redis(redisContext **ctx, char *host, int port, char *pswd, urn_func_opt *opt) {
	int rv = 0;
	char *err_str = NULL;

	*ctx = redisConnect(host, port);
	if (*ctx == NULL)
		return URN_ERRRET("Could not allocate redis context", 1);
	if ((*ctx)->err) {
		rv = ((*ctx)->err);
		redisFree(*ctx);
		return URN_ERRRET("Redis context init error", (*ctx)->err);
	}
	if (opt == NULL || (opt && !(opt->silent)))
		URN_LOGF("connected %s %d", host, port);

	if (pswd == NULL)
		return 0;

	if (opt && opt->verbose)
		URN_LOG("AUTH with password");
	redisReply *reply = redisCommand(*ctx, "AUTH %s", pswd);
	if ((*ctx)->err) {
		rv = ((*ctx)->err);
		err_str = "Error in Redis AUTH reply";
	}
	// str len 2: OK
	if (reply->type != REDIS_REPLY_STATUS) {
		rv = 1;
		err_str = "Unexpected Redis AUTH reply type";
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "%s: %d", err_str, reply->type);
	} else if (reply->len != 2 || reply->str[0] != 'O' || reply->str[1] != 'K') {
		rv = 1;
		err_str = "Unexpected Redis AUTH reply str";
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "%s: %s", err_str, reply->str);
	}

	freeReplyObject(reply);
	if (rv != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOG_C(RED, err_str);
		redisFree(*ctx);
		return URN_ERRRET(err_str, rv);
	}
	return 0;
}

int urn_redis_set(redisContext *ctx, char *key, char*value, urn_func_opt *opt) {
	int rv = 0;
	char *err_str = NULL;

	if (opt && opt->verbose)
		URN_LOGF("--> SET %s %s", key, value);
	redisReply *reply = redisCommand(ctx, "SET %s %s", key, value);

	// str len 2: OK
	if (reply->type != REDIS_REPLY_STATUS) {
		rv = 1;
		err_str = "Unexpected Redis SET reply type";
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "SET %s %s: err:%s reply.type:%d", key, value, err_str, reply->type);
	} else if (reply->len != 2 || reply->str[0] != 'O' || reply->str[1] != 'K') {
		rv = 1;
		err_str = "Unexpected Redis SET reply str";
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "SET %s %s: err:%s", key, value, err_str);
	} else if (opt && opt->verbose)
		URN_LOGF("<-- SET %s", key);

	freeReplyObject(reply);
	if (rv != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "Redis SET failed: %s", err_str);
		return URN_ERRRET(err_str, rv);
	}
	return rv;
}

int urn_redis_get(redisContext *ctx, char *key, char **str, int *len, urn_func_opt *opt) {
	int rv = 0;
	char *err_str = NULL;

	if (opt && opt->verbose)
		URN_LOGF("--> GET %s", key);
	redisReply *reply = redisCommand(ctx, "GET %s", key);

	if (0 && opt && opt->verbose) // debug
		URN_LOGF("reply type %d\ninteger %lld\ndval %f\nstr len %zu\nstr %s\nvtype %s\nelements %zu",
				reply->type,
				reply->integer,
				reply->dval,
				reply->len,
				reply->str,
				reply->vtype,
				reply->elements
			);

	if (reply->type == REDIS_REPLY_NIL) {
		if (opt && (opt->verbose))
			URN_LOGF("<-- GET %s : NULL", key)
		*len = 0;
		*str = NULL;
	} else if (reply->type != REDIS_REPLY_STRING) {
		rv = 1;
		err_str = "Unexpected Redis GET reply type";
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "GET %s : err:%s reply.type:%d", key, err_str, reply->type);
	} else { // REDIS_REPLY_STRING
		*len = reply->len;
		if (opt && (opt->verbose))
			URN_LOGF("<-- GET %s : len %d", key, *len);
		*str = malloc((*len) + 1);
		memcpy(*str, reply->str, *len);
		(*str)[(*len)] = '\0';
		URN_LOGF("<-- GET %s : [%s]", key, *str);
	}

	freeReplyObject(reply);
	if (rv != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "SET failed: %s", err_str);
		return URN_ERRRET(err_str, rv);
	}
	return rv;
}

int urn_redis_del(redisContext *ctx, char *key, urn_func_opt *opt) {
	int rv = 0;
	char *err_str = NULL;

	if (opt && opt->verbose)
		URN_LOGF("--> DEL %s", key);
	redisReply *reply = redisCommand(ctx, "DEL %s", key);

	if (reply->type == REDIS_REPLY_INTEGER) {
		if (opt && (opt->verbose))
			URN_LOGF("<-- DEL %s : %lld", key, reply->integer)
	} else {
		rv = 1;
		err_str = "Unexpected Redis DEL reply type";
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "DEL %s : err:%s reply.type:%d", key, err_str, reply->type);
	}

	freeReplyObject(reply);
	if (rv != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "DEL failed: %s", err_str);
		return URN_ERRRET(err_str, rv);
	}
	return rv;
}

//////////////////////////////////////////
// Async mode redis client.
//////////////////////////////////////////

int urn_redis_async(redisAsyncContext **ctx, char *host, int port) {
	int rv = 0;
	char *err_str = NULL;

	*ctx = redisAsyncConnect(host, port);
	if (*ctx == NULL)
		return URN_ERRRET("Could not allocate redis async context", 1);
	if ((*ctx)->err) {
		rv = ((*ctx)->err);
		// TODO how to redisFree(*ctx);
		return URN_ERRRET("Redis context init error", (*ctx)->err);
	}

	// Attach to GLib mainloop
	GSource *gsource = redis_source_new(*ctx);
	if (gsource == NULL)
		return URN_ERRRET("Could not allocate GSource", 1);
	g_source_attach(gsource, NULL);
	return 0;
}

#endif
