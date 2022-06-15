#ifndef URANUS_REDIS
#define URANUS_REDIS

#include <hiredis.h>
#include <adapters/glib.h>

#include "urn.h"

int urn_redis_chkfree_reply_str(redisReply *reply, char *expect_ans, urn_func_opt *opt);
int urn_redis_chkfree_reply_long(redisReply *reply, long long *lldp, urn_func_opt *opt);

int urn_redis(redisContext **ctx, char *host, char *portc, char *pswd, urn_func_opt *opt) {
	int rv = 0;
	int port = 6379;
	char *err_str = NULL;

	if (host == NULL) host = "127.0.0.1";
	if (portc != NULL) port = atoi(portc);

	URN_INFOF("connecting redis %s %d pswd %s", host, port, pswd==NULL ? "NO":"YES");
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

	if (pswd == NULL) {
		if (opt && opt->verbose)
			URN_LOG("PING PONG test");
		redisReply *reply = redisCommand(*ctx, "PING");
		if ((*ctx)->err) {
			rv = ((*ctx)->err);
			err_str = "Error in Redis AUTH reply";
		}
		rv = urn_redis_chkfree_reply_str(reply, "PONG", opt);
	} else {
		if (opt && opt->verbose)
			URN_INFO("AUTH with password");
		redisReply *reply = redisCommand(*ctx, "AUTH %s", pswd);
		if ((*ctx)->err) {
			rv = ((*ctx)->err);
			err_str = "Error in Redis AUTH reply";
		}
		rv = urn_redis_chkfree_reply_str(reply, "OK", opt);
	}

	if (rv != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_WARN(err_str);
		redisFree(*ctx);
		return URN_ERRRET(err_str, rv);
	}
	return 0;
}

// Could be used for pipelined reply, it must be 'OK'.
int urn_redis_chkfree_reply_str(redisReply *reply, char *expect_ans, urn_func_opt *opt) {
	// str len 2: OK
	// if (reply->len != 2 || reply->str[0] != 'O' || reply->str[1] != 'K') {
	if (strcmp(reply->str, expect_ans) != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "Unexpected Redis reply, expected %s got:%s", expect_ans, reply->str);
		return EINVAL;
	} else if (opt && opt->verbose)
		URN_LOGF("<-- %s", reply->str);
	freeReplyObject(reply);
	return 0;
}

// Could be used for pipelined reply, it must be 'OK'.
int urn_redis_chkfree_reply_long(redisReply *reply, long long *lldp, urn_func_opt *opt) {
	// str len 2: OK
	if (reply->type != REDIS_REPLY_INTEGER) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "Unexpected Redis long long reply type expect %d got %d", REDIS_REPLY_INTEGER, reply->type);
		return EINVAL;
	} else if (opt && opt->verbose)
		URN_LOGF("<-- %lld", reply->integer);
	if (lldp != NULL) *lldp = reply->integer;
	freeReplyObject(reply);
	return 0;
}

int urn_redis_set(redisContext *ctx, char *key, char*value, urn_func_opt *opt) {
	int rv = 0;
	char *err_str = NULL;

	if (opt && opt->verbose)
		URN_LOGF("--> SET %s %s", key, value);
	rv = urn_redis_chkfree_reply_str(
			redisCommand(ctx, "SET %s %s", key, value), "OK", opt);

	if (rv != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "Redis SET %s %s failed: %s", key, value, err_str);
		return URN_ERRRET(err_str, rv);
	}
	return rv;
}

int urn_redis_broadcast(redisContext *ctx, char *key, char*value, urn_func_opt *opt) {
	int rv = 0;
	char *err_str = NULL;

	if (opt && opt->verbose)
		URN_LOGF("--> PUBLISH %s %s", key, value);
	rv = urn_redis_chkfree_reply_str(
			redisCommand(ctx, "SET %s %s", key, value), "OK", opt);

	if (rv != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "Redis SET %s %s failed: %s", key, value, err_str);
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

int urn_redis_pub(redisContext *ctx, char *key, char*value, long long *listener_ct, urn_func_opt *opt) {
	int rv = 0;
	char *err_str = NULL;

	if (opt && opt->verbose)
		URN_LOGF("--> PUBLISH %s %s", key, value);
	rv = urn_redis_chkfree_reply_long(
			redisCommand(ctx, "PUBLISH %s %s", key, value), listener_ct, opt);

	if (rv != 0) {
		if (opt == NULL || (opt && !(opt->silent)))
			URN_LOGF_C(RED, "Redis PUBLISH %s %s failed: %s", key, value, err_str);
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
