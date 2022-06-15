#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <unistd.h> // sleep

#include "urn.h"
#include "redis.h"

#include <hiredis.h>
#include <adapters/glib.h>

int redis_sync_example() {
	int rv = 0;
	redisContext *c = NULL;
	redisReply *reply;
	urn_func_opt opt = {.verbose=1,.silent=0};

	rv = urn_redis(&c, "127.0.0.1", 6379, getenv("REDIS_PSWD"), &opt);
	if (rv != 0)
		return URN_FATAL("Error in init redis", rv);
	URN_LOG("redis context initialized");

	URN_INFO("SET foo bar");
	rv = urn_redis_set(c, "foo", "bar", &opt);
	if (rv != 0)
		return URN_FATAL("Error in set foo bar", rv);

	int value_len;

	char *value;
	URN_INFO("--> GET foo");
	rv = urn_redis_get(c, "foo", &value, &value_len, &opt);
	if (rv != 0) return URN_FATAL("Error in get foo", rv);
	URN_INFOF("<-- %d %s", value_len, value);
	free(value);

	URN_INFO("--> GET bar");
	rv = urn_redis_get(c, "bar", &value, &value_len, &opt);
	if (rv != 0) return URN_FATAL("Error in get bar", rv);
	URN_INFOF("<-- %d %s", value_len, value);
	free(value);

	URN_INFO("--> GET barrr");
	rv = urn_redis_get(c, "barrr", &value, &value_len, &opt);
	if (rv != 0) return URN_FATAL("Error in get barrr", rv);
	URN_INFOF("<-- %d %s", value_len, value);
	free(value);

	URN_INFO("--> DEL foo");
	rv = urn_redis_del(c, "foo", &opt);
	if (rv != 0) return URN_FATAL("Error in del foo", rv);
	URN_INFOF("<-- %d %s", value_len, value);

	URN_INFO("PUBLISH chn value");
	long long listener_ct = 0;
	rv = urn_redis_pub(c, "chn", "value", &listener_ct, &opt);
	if (rv != 0)
		return URN_FATAL("Error in pub chn value", rv);
	URN_INFOF("PUBLISH is heard by %lld listeners", listener_ct);
	return rv;
}

int redis_pipeline_example() {
	int rv = 0;
	redisContext *c = NULL;
	redisReply *reply;
	urn_func_opt opt = {.verbose=1,.silent=0};

	rv = urn_redis(&c, "127.0.0.1", 6379, getenv("REDIS_PSWD"), &opt);
	if (rv != 0)
		return URN_FATAL("Error in init redis", rv);
	URN_LOG("redis context initialized");

	URN_INFO("PIPELINE SET foo bar 1/2/3");
	redisAppendCommand(c, "SET foo bar");
	redisAppendCommand(c, "SET foo1 bar1");
	redisAppendCommand(c, "SET foo2 bar2");

	URN_INFO("PIPELINE PUBLISH chn value");
	redisAppendCommand(c, "PUBLISH chn value");

	URN_INFO("PIPELINE get reply: 3 for SET");
	redisGetReply(c, (void**)&reply);
	URN_RET_ON_RV(urn_redis_chkfree_reply_OK(reply, &opt), "error in checking OK");
	redisGetReply(c, (void**)&reply);
	URN_RET_ON_RV(urn_redis_chkfree_reply_OK(reply, &opt), "error in checking OK");
	redisGetReply(c, (void**)&reply);
	URN_RET_ON_RV(urn_redis_chkfree_reply_OK(reply, &opt), "error in checking OK");

	URN_INFO("PIPELINE get reply: PUBLISH");
	long long listener_ct = 0;
	redisGetReply(c, (void**)&reply);
	URN_RET_ON_RV(urn_redis_chkfree_reply_INT(reply, &listener_ct, &opt), "error in checking listeners");
	URN_INFOF("PUBLISH is heard by %lld listeners", listener_ct);
	return rv;
}

void connect_cb (const redisAsyncContext *ac G_GNUC_UNUSED, int status)
{
    if (status != REDIS_OK) {
        g_printerr("Failed to connect: %s\n", ac->errstr);
	g_main_loop_quit(g_main_loop_new(NULL, FALSE));
    } else {
        URN_INFO("Redis async connected.");
    }
}

void disconnect_cb (const redisAsyncContext *ac G_GNUC_UNUSED, int status)
{
    if (status != REDIS_OK) {
        URN_WARN("Redis async disconnected failed, maybe still alive.");
    } else {
        URN_WARN("Redis async disconnected, exit mainloop now");
	g_main_loop_quit(g_main_loop_new(NULL, FALSE));
    }
}

void command_cb(redisAsyncContext *ac, gpointer r, gpointer user_data G_GNUC_UNUSED) {
    redisReply *reply = r;

    if (reply) {
	    URN_LOGF("reply type %d\ninteger %lld\ndval %f\nstr len %zu\nstr %s\nvtype %s\nelements %zu",
			    reply->type,
			    reply->integer,
			    reply->dval,
			    reply->len,
			    reply->str,
			    reply->vtype,
			    reply->elements
		    );
	    URN_INFOF("<-- %s , from %s", reply->str, (char *)user_data);
    }

    // redisAsyncDisconnect(ac);
}

int redis_async_example() {
	int rv = 0;
	redisAsyncContext *ac;
	rv = urn_redis_async(&ac, "127.0.0.1", 6379);
	if (rv != 0) return URN_FATAL("Error in init redis async", rv);

	redisAsyncSetConnectCallback(ac, connect_cb);
	redisAsyncSetDisconnectCallback(ac, disconnect_cb);

	redisAsyncCommand(ac, command_cb, "A", "auth %s", getenv("REDIS_PSWD"));
	redisAsyncCommand(ac, command_cb, "B", "SET key 1234");
	redisAsyncCommand(ac, command_cb, "C", "GET key");

	URN_INFO("Start GLib mainloop after 5s");
	sleep(5);
	g_main_loop_run(g_main_loop_new(NULL, FALSE));
	URN_INFO("return after 5s");
	sleep(5);
	return 0;
}

int main(int argc, char **argv) {
	redis_sync_example();
	redis_pipeline_example();
	redis_async_example();
}
