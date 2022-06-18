#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <nng/nng.h>
#include <nng/supplemental/tls/tls.h>

#include "yyjson.c"

#include "urn.h"
#include "order.h"
#include "wss.h"
#include "hmap.h"
#include "redis.h"

#ifndef PUB_NO_REDIS
#define PUB_REDIS
#endif

#ifndef MAX_DEPTH // max depth in each side.
#define MAX_DEPTH 5
#endif

#ifndef MAX_PAIRS
#define MAX_PAIRS 1024
#endif

#ifdef __linux__ // only macos has CLOCK_MONOTONIC_RAW_APPROX
#ifndef CLOCK_MONOTONIC_RAW_APPROX
#define CLOCK_MONOTONIC_RAW_APPROX CLOCK_MONOTONIC_RAW
#endif
#endif

///////////// Interface //////////////
// To use mkt_data.h, must implement below methods:

// free(pair) and return mapped pair.
char *preprocess_pair(char *pair); // BUSD-BTC -> USD-BTC
int   exchange_sym_alloc(urn_pair *pair, char **str);
void  mkt_data_set_exchange(char *s); // set global exchange 
void  mkt_data_set_wss_url(char *s);
void  mkt_data_odbk_channel(char *sym, char *chn);
void  mkt_data_odbk_snpsht_channel(char *sym, char *chn);
void  mkt_data_tick_channel(char *sym, char *chn);
int   on_wss_msg(char *msg, size_t len);
int   mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns);

///////////// Interface //////////////

int wss_connect();

static int parse_args_build_idx(int argc, char **argv);
static GList *remove_top_porder(GList *asks);
static void wss_stat();
static int broadcast();

#ifdef URN_MAIN_DEBUG
static void print_odbk(int pairid);
#else
#define print_odbk(id) ;
#endif

///////////// pair - symbol - channel hmap //////////////
hashmap* pair_to_sym;
hashmap* sym_to_pair;
hashmap* trade_chn_to_pair;
hashmap* depth_chn_to_pair;
hashmap* depth_snpsht_chn_to_pair;

///////////// data organized by pairid //////////////
int     max_pair_id = -1;
hashmap *trade_chn_to_pairid;
hashmap *depth_chn_to_pairid;
hashmap *depth_snpsht_chn_to_pairid;

char  *pair_arr[MAX_PAIRS]; // pair = pair_arr[pairid]
GList *bids_arr[MAX_PAIRS]; // bids = bids_arr[pairid]
GList *asks_arr[MAX_PAIRS]; // asks = asks_arr[pairid]
int    askct_arr[MAX_PAIRS];
int    bidct_arr[MAX_PAIRS];
long   odbk_t_arr[MAX_PAIRS];

///////////// wss_req //////////////
// Maybe two command per pair, 1 more for NULL
// After initial stage, we can reuse this by move wss_req_i
#define  MAX_WSS_REQ_BUFF (MAX_PAIRS*2+1)
char*    wss_req_s[MAX_WSS_REQ_BUFF];
int      wss_req_i = 0; // idx of next sent wss_req_s
int      wss_req_interval_e = 1; // call wss_req_next() at least 2^e msgs received.
int      wss_req_interval_ms = 0; // interval in ms
struct timespec wss_req_t; // last time to call wss_req_next()
unsigned int wss_req_wait_ct;

///////////// stat //////////////
unsigned int wss_stat_sz;
unsigned int wss_stat_ct;
struct timespec wss_stat_t;
int wss_stat_per_e = 8;
long wss_stat_mkt_ts = 0;
long mkt_latency_ms = 0;

struct timespec _tmp_clock;

///////////// broadcast ctrl //////////////
struct timespec brdcst_t; // last time to call broadcast()
unsigned long brdcst_interval_ms; // min ms between two broadcast
unsigned int  newodbk_arr[MAX_PAIRS];
char         *pub_odbk_chn_arr[MAX_PAIRS];
char         *pub_odbk_key_arr[MAX_PAIRS]; // kv snapshot
char         *pub_tick_chn_arr[MAX_PAIRS];
char         *pub_tick_key_arr[MAX_PAIRS]; // kv snapshot
redisContext *redis;

#ifdef PUB_LESS_ON_ZERO_LISTENER
struct timespec brdcst_t_arr[MAX_PAIRS]; // last time to publish pair
long long brdcst_listener_arr[MAX_PAIRS];// listeners last time.
#endif

unsigned int  brdcst_max_speed = 1000;
unsigned int  brdcst_stat_rd_ct;
unsigned int  brdcst_stat_ct;
unsigned int  brdcst_stat_t;

// BybitU or Bybit
char         exchange[32];

int main(int argc, char **argv) {
	if (argc <= 1)
		return URN_FATAL("Args: usdt-btc@p", ENOMEM);
	if (argc > 1 && (strcasecmp(argv[1], "-v") == 0)) {
		URN_LOG("Compiled at " __TIMESTAMP__);
		return 0;
	}

	mkt_data_set_exchange(exchange);
	brdcst_interval_ms = 1;

	if (argc > MAX_PAIRS)
		return URN_FATAL("Raise MAX_PAIRS please", ENOMEM);
	for (int i=0; i<MAX_PAIRS; i++) {
		pair_arr[i] = NULL;
		bids_arr[i] = NULL;
		asks_arr[i] = NULL;
		odbk_t_arr[i] = 0;
		askct_arr[i] = 0;
		bidct_arr[i] = 0;
		newodbk_arr[i] = 0;
		pub_odbk_chn_arr[i] = NULL;
		pub_odbk_key_arr[i] = NULL;
		pub_tick_chn_arr[i] = NULL;
		pub_tick_key_arr[i] = NULL;
#ifdef PUB_LESS_ON_ZERO_LISTENER
		brdcst_listener_arr[i] = 0;
		brdcst_t_arr[i].tv_sec = 0;
#endif
	}
	brdcst_stat_ct = 0;
	brdcst_stat_rd_ct = 0;
	brdcst_stat_t = 0;

	int rv = 0;
	if ((rv = parse_args_build_idx(argc, argv)) != 0)
		return URN_FATAL("Error in parse_args_build_idx()", rv);

	URN_INFOF("max_pair_id %d", max_pair_id);

	URN_INFOF("nng_tls_engine_name %s", nng_tls_engine_name());
	URN_INFOF("nng_tls_engine_description %s", nng_tls_engine_description());

#ifdef PUB_REDIS
	urn_func_opt verbose_opt = {.verbose=1,.silent=0};
	rv = urn_redis(&redis, getenv("REDIS_HOST"), getenv("REDIS_PORT"), getenv("REDIS_PSWD"), &verbose_opt);
	if (rv != 0)
		return URN_FATAL("Error in init redis", rv);
#endif

	if ((rv = wss_connect()) != 0)
		return URN_FATAL("Error in wss_connect()", rv);

	// pair are actually argv
	urn_hmap_free_with_keys(depth_chn_to_pair);
	urn_hmap_free_with_keys(depth_snpsht_chn_to_pair);
	urn_hmap_free_with_keys(trade_chn_to_pair);
	// same key/val in sym_to_pair and pair_to_sym
	urn_hmap_free(sym_to_pair);
	urn_hmap_free(pair_to_sym);
	urn_hmap_free(depth_chn_to_pairid);
	urn_hmap_free(depth_snpsht_chn_to_pairid);
	urn_hmap_free(trade_chn_to_pairid);
	return 0;
}

int wss_req_next(nng_stream *stream) {
	// reset timer no matter did req or not.
	// Don't come here every round.
	wss_req_wait_ct = 0;

	// less than wss_req_interval_ms, return
	clock_gettime(CLOCK_MONOTONIC_RAW_APPROX, &_tmp_clock);
	if (((_tmp_clock.tv_sec - wss_req_t.tv_sec)*1000l + (_tmp_clock.tv_nsec - wss_req_t.tv_nsec)/1000000l) < wss_req_interval_ms)
		return 0;
	wss_req_t.tv_sec = _tmp_clock.tv_sec;
	wss_req_t.tv_nsec = _tmp_clock.tv_nsec;

	char *req_s = wss_req_s[wss_req_i];
	if (req_s == NULL) return 0;

	int rv = 0;
	URN_INFOF("Sending wss_req_s[%d] %lu bytes: %s", wss_req_i, strlen(req_s), req_s);
	if ((rv = urn_ws_send_sync(stream, req_s)) != 0)
		return URN_FATAL_NNG(rv);
	free(wss_req_s[wss_req_i]);
	wss_req_s[wss_req_i] = NULL;

	wss_req_i++;
	if (wss_req_i >= MAX_WSS_REQ_BUFF)
		wss_req_i = 0;
	return 0;
}

char wss_uri[256];
int wss_connect() {
	int rv = 0;
	mkt_data_set_wss_url(wss_uri);
	nng_aio *dialer_aio = NULL;
	nng_stream_dialer *dialer = NULL;
	nng_stream *stream = NULL;

	// coinbase snapshot msg larger than 256KB
	int recv_buflen = 4*1024*1024;
	char *recv_buf = malloc(recv_buflen);
	if (recv_buf == NULL)
		return URN_FATAL("Not enough memory for wss recv_buf", ENOMEM);

	nng_aio *recv_aio = NULL;
	nng_iov *recv_iov = NULL;
	size_t recv_bytes = 0;

	URN_INFOF("Init wss conn %s", wss_uri);
	if ((rv = urn_ws_init(wss_uri, &dialer, &dialer_aio)) != 0)
		return URN_FATAL_NNG(rv);

	URN_INFO("Wait conn stream");
	if ((rv = urn_ws_wait_stream(dialer_aio, &stream)) != 0)
		return URN_FATAL_NNG(rv);
	URN_INFO("Wss conn stream is ready");

	if ((rv = wss_req_next(stream)) != 0)
		return URN_FATAL_NNG(rv);

	// Reuse iov buffer and aio to receive data.
	if ((rv = nngaio_init(recv_buf, recv_buflen, &recv_aio, &recv_iov)) != 0)
		return URN_FATAL_NNG(rv);
	URN_INFO("Recv aio and iov ready");

	wss_stat_sz = 0;
	wss_stat_ct = 0;
	wss_req_wait_ct = 0;
	clock_gettime(CLOCK_MONOTONIC_RAW_APPROX, &wss_stat_t);
	clock_gettime(CLOCK_MONOTONIC_RAW_APPROX, &brdcst_t);
	clock_gettime(CLOCK_MONOTONIC_RAW_APPROX, &wss_req_t);

	while(true) {
#ifdef URN_WSS_DEBUG
		if ((rv = nngaio_recv_wait_res(stream, recv_aio, recv_iov, &recv_bytes, "<-- recv wait", "<-- recv poll")) != 0)
#else
		if ((rv = nngaio_recv_wait_res(stream, recv_aio, recv_iov, &recv_bytes, NULL, NULL)) != 0)
#endif
		{
			URN_WARN("Error in nngaio_recv_wait_res()");
			return URN_FATAL_NNG(rv);
		}
		wss_stat_ct++;
		wss_stat_sz += recv_bytes;
		URN_DEBUGF("pre on_wss_msg %lu/%d %.*s", recv_bytes, recv_buflen,
				URN_MIN(200, ((int)recv_bytes)), recv_buf);
		// only 0..recv_bytes is message received this time.
		// terminate it, some message does not have '\0\ at last
		recv_buf[recv_bytes] = '\0';
		rv = on_wss_msg(recv_buf, recv_bytes);
		if (rv != 0) {
			URN_WARNF("Error in processing wss msg len(%lu) %.*s", recv_bytes, (int)recv_bytes, recv_buf);
			return rv;
		}
		
		broadcast();

		// every 2^e rounds, send unfinished requests after.
		// Too many subscribes at once may cause wss server drops conn.
		wss_req_wait_ct ++;
		if ((wss_req_wait_ct >> wss_req_interval_e) > 0) {
			if ((rv = wss_req_next(stream)) != 0)
				return URN_FATAL_NNG(rv);
		}

		// stat every few seconds
		if ((wss_stat_ct >> wss_stat_per_e) > 0)
			wss_stat();
	}
	URN_INFOF("wss_connect finished at wss_stat_ct %u", wss_stat_ct);
	return 0;
}

// broadcast every few milliseconds.
struct timeval brdcst_json_t;
struct timeval brdcst_json_end_t;
// build broadcast json
// [bids, asks, t, mkt_t]
// for each order:
// 	strlen({"p":X.X,"s":X.X}) = 13+4X
// total orderbook len:
// 	2 * DEPTH * (13 + 4X)
// timestamp len = 1655250053846 = 13
// [bids, asks, ts, ts] len:
// 	2 * DEPTH * (13 + 4X) + 26
#define MAX_BRDCST_LEN (2 * MAX_DEPTH * (13 + 4*URN_INUM_PRECISE) + 26)
char   brdcst_str[MAX_BRDCST_LEN];
int    brdcst_pairs[MAX_PAIRS];
static int broadcast() {
	// less than Xms = X*1_000_000 ns, return
	clock_gettime(CLOCK_MONOTONIC_RAW_APPROX, &_tmp_clock);
	if ((_tmp_clock.tv_sec == brdcst_t.tv_sec) && 
			(_tmp_clock.tv_nsec - brdcst_t.tv_nsec < 1000000l * brdcst_interval_ms))
		return 0;

	int rv = 0;
	brdcst_t.tv_sec = _tmp_clock.tv_sec;
	brdcst_t.tv_nsec = _tmp_clock.tv_nsec;
	gettimeofday(&brdcst_json_t, NULL);

	bool do_stat = brdcst_t.tv_sec - brdcst_stat_t > 4;
	// Every stat round, 1/8 chance to write full snapshot.
	bool write_snapshot = false;
#ifdef PUB_REDIS
	write_snapshot = do_stat && ((_tmp_clock.tv_nsec & 8) < 1);
	if (write_snapshot)
		URN_DEBUGF_C(YELLOW, "write_snapshot %ld %ld, %ld, %ld",
			_tmp_clock.tv_sec, (_tmp_clock.tv_sec & 7),
			_tmp_clock.tv_nsec, (_tmp_clock.tv_nsec & 65535));
#endif

	int data_ct = 0;
	char *s = brdcst_str;
	for (int pairid = 1; pairid <= max_pair_id; pairid ++) { // The 0 pairid is NULL
		if ((!write_snapshot) && (newodbk_arr[pairid] <= 0))
			continue;

#ifdef PUB_LESS_ON_ZERO_LISTENER
		if ((!write_snapshot) && (brdcst_listener_arr[pairid] == 0)) {
			// Publish every 2s if no listener last time.
			if (brdcst_t_arr[pairid].tv_sec + 2 > brdcst_t.tv_sec)
				continue;
			brdcst_t_arr[pairid].tv_sec = brdcst_t.tv_sec;
		}
#endif

		URN_DEBUGF("to broadcast %s %d/%d, updates %d bids %d asks %d",
			pair_arr[pairid], pairid, max_pair_id, newodbk_arr[pairid],
			bidct_arr[pairid], askct_arr[pairid]);
		if ((bidct_arr[pairid] == 0) || (askct_arr[pairid] == 0))
			continue;
		brdcst_stat_ct ++;

#ifdef PUB_REDIS
		int ct = 0;
		*(s+ct) = '['; ct++;
		ct += sprintf_odbk_json(s+ct, bids_arr[pairid]);
		*(s+ct) = ','; ct++;
		ct += sprintf_odbk_json(s+ct, asks_arr[pairid]);
		ct += sprintf(s+ct, ",%ld%03ld,%ld]",
				brdcst_json_t.tv_sec,
				(long)(brdcst_json_t.tv_usec/1000),
				odbk_t_arr[pairid]/1000);
		URN_DEBUGF("broadcast %s, updates %d, json len %d/%d -> %s\n%s",
				pair_arr[pairid], newodbk_arr[pairid],
				ct, MAX_BRDCST_LEN, pub_odbk_chn_arr[pairid], s);

		redisAppendCommand(redis, "PUBLISH %s %s", pub_odbk_chn_arr[pairid], s);
		if (write_snapshot) {
			URN_DEBUGF("also snapshot to %s", pub_odbk_key_arr[pairid]);
			redisAppendCommand(redis, "SET %s %s", pub_odbk_key_arr[pairid], s);
		}
#endif
		newodbk_arr[pairid] = 0; // reset new odbk ct;
		brdcst_pairs[data_ct] = pairid;
		data_ct ++;
	}
	URN_DEBUGF("broadcast check full=%d stat=%d data_ct=%d", write_snapshot, do_stat, data_ct);
	if (data_ct == 0)
		goto final;

	brdcst_stat_rd_ct ++;

	long long listener_ttl_ct = 0;
#ifdef PUB_REDIS
	URN_GO_FINAL_ON_RV(redis->err, "redis context err");
	long long listener_ct = 0;
	redisReply *reply = NULL;
	for (int i=0; i<data_ct; i++) {
		rv = redisGetReply(redis, (void**)&reply);
		URN_GO_FINAL_ON_RV(rv, "Redis get reply code error");
		int pairid = brdcst_pairs[i];
		rv = urn_redis_chkfree_reply_long(reply, &listener_ct, NULL);
		URN_GO_FINAL_ON_RV(rv, "error in checking listeners");
		if (write_snapshot) {
			rv = redisGetReply(redis, (void**)&reply);
			URN_GO_FINAL_ON_RV(rv, "Redis get reply code error");
			rv = urn_redis_chkfree_reply_str(reply, "OK", NULL);
			URN_GO_FINAL_ON_RV(rv, "error in checking listeners");
		}
#ifdef PUB_LESS_ON_ZERO_LISTENER
		brdcst_listener_arr[pairid] = listener_ct;
#endif
		listener_ttl_ct += listener_ct;
	}
#endif

	if (do_stat) {
		// stat in few seconds, also print cost time this round.
		gettimeofday(&brdcst_json_end_t, NULL);
		long cost_us = urn_usdiff(brdcst_json_t, brdcst_json_end_t);
		bool redis_slow = (cost_us > 1000) ? true : false;
		if (cost_us*5 > brdcst_interval_ms*1000) {
			// should not spend >= 20% time of interval
			unsigned long new_brdcst_interval_ms = URN_MAX(brdcst_interval_ms+1, 4*cost_us/1000);
			URN_LOGF_C(YELLOW, "brdcst cost_us %ld = %ld ms, interval_ms %lu, incr to %lu",
				cost_us, cost_us/1000, brdcst_interval_ms, new_brdcst_interval_ms);
			brdcst_interval_ms = new_brdcst_interval_ms;
		}

		int diff = brdcst_t.tv_sec - brdcst_stat_t;
		if (write_snapshot && (diff == 0))
			goto final;

		int speed = brdcst_stat_ct / diff;
		// control interval to publish redis at MAX_SPEED ~ MAX_SPEED/5
		if (speed > brdcst_max_speed) {
			URN_LOGF_C(YELLOW, "brdcst max speed %d < real %d, cost_us %ld, incr interval",
				brdcst_max_speed, speed, cost_us)
			if (brdcst_interval_ms < 10)
				brdcst_interval_ms ++;
			else
				brdcst_interval_ms += brdcst_interval_ms/5;
		} else if ((!redis_slow) && speed < brdcst_max_speed/4 && brdcst_interval_ms > 1) {
			URN_LOGF_C(YELLOW, "brdcst max speed %d > real %d, cost_us %ld, decr interval",
				brdcst_max_speed, speed, cost_us)
			brdcst_interval_ms --;
		}

#ifdef PUB_REDIS
		URN_LOGF_C(GREEN, "--> %d/s %dr/s in %d, every %ldms, %4.2fms > %d chn > %lld ears %s",
			speed, brdcst_stat_rd_ct/diff, diff, brdcst_interval_ms,
			(float)cost_us/1000.f, data_ct, listener_ttl_ct,
			(write_snapshot ? "FULL" : ""));
#else
		URN_LOGF_C(GREEN, "--> %d/s %dr/s in %d, every %ldms",
			speed, brdcst_stat_rd_ct/diff, diff, brdcst_interval_ms);
#endif
		brdcst_stat_rd_ct = 0;
		brdcst_stat_ct = 0;
		brdcst_stat_t = brdcst_t.tv_sec;
	}

final:
	return rv;
}

static void wss_stat() {
	if (wss_stat_mkt_ts != 0) {
		clock_gettime(CLOCK_REALTIME, &_tmp_clock);
		mkt_latency_ms = _tmp_clock.tv_sec * 1000l + 
			_tmp_clock.tv_nsec/1000000l - wss_stat_mkt_ts/1000l;
	}

	clock_gettime(CLOCK_MONOTONIC_RAW_APPROX, &_tmp_clock);
	time_t passed_s = _tmp_clock.tv_sec - wss_stat_t.tv_sec;
	float ct_per_s = (float)(wss_stat_ct) / (float)passed_s;
	float kb_per_s = (float)(wss_stat_sz) / (float)passed_s / 1000.f;
	URN_INFOF("<-- %s in %6lu s %8.f msg/s %8.f KB/s + %ld ms", exchange, passed_s, ct_per_s, kb_per_s, mkt_latency_ms);
	if (passed_s < 2)
		wss_stat_per_e ++; // double stat interval
	// Reset stat
	wss_stat_ct = 0;
	wss_stat_sz = 0;
	wss_stat_t = _tmp_clock;
}

// Remove top order (if any) and return new list head.
static GList *remove_top_porder(GList *asks) {
	if (asks->next == NULL)
		URN_FATAL("remove_top_porder() but GList->next is NULL", EINVAL);
	if (asks->data == NULL)
		URN_FATAL("remove_top_porder() but GList->data is NULL", EINVAL);
	GList *new_head = asks->next;
	urn_porder_free(asks->data);
	URN_DEBUGF("freeing top GList %p", asks);
	g_list_free_1(asks);
	new_head->prev = NULL;
	return new_head;
}

#ifdef URN_MAIN_DEBUG
static void print_odbk(int pairid) {
	char *pair = pair_arr[pairid];
	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];
	int idx = 0;
	URN_DEBUGF("orderbook[%3d] %20s bids %p->%p asks %p->%p", pairid, pair, bids, bids->data, asks, asks->data);
	urn_porder *a = NULL;
	urn_porder *b = NULL;
	do {
		if (asks != NULL) {
			a = (urn_porder *)(asks->data);
			asks = asks->next;
		} else
			a = NULL;
		if (bids != NULL) {
			b = (urn_porder *)(bids->data);
			bids = bids->next;
		} else
			b = NULL;

		if (b != NULL && a != NULL) {
			printf("%2d %20s %20s  -  %20s %20s\n",
				idx,
				urn_inum_str(b->p), urn_inum_str(b->s),
				urn_inum_str(a->p), urn_inum_str(a->s)
			);
		} else if (b != NULL) {
			URN_DEBUGF("orderbook[%3d] %20s %3d bid %p ask %p", pairid, pair, idx, b, a);
			printf("%2d %20s %20s  -  %20s %20s\n",
				idx,
				urn_inum_str(b->p), urn_inum_str(b->s),
				"", ""
			);
		} else if (a != NULL) {
			URN_DEBUGF("orderbook[%3d] %20s %3d bid %p ask %p", pairid, pair, idx, b, a);
			printf("%2d %20s %20s  -  %20s %20s\n",
				idx, "", "",
				urn_inum_str(a->p), urn_inum_str(a->s)
			);
		} else
			printf("%2d NULL - NULL \n", idx);
		if (a != NULL && (a->s == NULL || a->p == NULL))
			URN_FATAL("ask not NULL but s/p is NULL", EINVAL);
		if (b != NULL && (b->s == NULL || b->p == NULL))
			URN_FATAL("bid not NULL but s/p is NULL", EINVAL);
		idx++;
	} while(bids != NULL || asks != NULL);
}
#endif

static int parse_args_build_idx(int argc, char **argv) {
	const int max_pairn = argc;
	int rv = 0;
	if (argc < 2)
		return URN_FATAL("No target pair supplied!", 1);
	URN_INFO("Parsing ARGV start");

	pair_to_sym = urn_hmap_init(max_pairn*5); // to avoid resizing.
	sym_to_pair = urn_hmap_init(max_pairn*5);
	depth_chn_to_pair = urn_hmap_init(max_pairn*5);
	depth_snpsht_chn_to_pair= urn_hmap_init(max_pairn*5);
	trade_chn_to_pair = urn_hmap_init(max_pairn*5);

	// dont use 0=NULL as pairid
	for (int i=0; i<argc+1; i++) {
		bids_arr[i] = g_list_alloc();
		bids_arr[i]->prev = NULL;
		bids_arr[i]->next = NULL;
		asks_arr[i] = g_list_alloc();
		asks_arr[i]->prev = NULL;
		asks_arr[i]->next = NULL;
	}

	depth_chn_to_pairid = urn_hmap_init(max_pairn*5);
	depth_snpsht_chn_to_pairid = urn_hmap_init(max_pairn*5);
	trade_chn_to_pairid = urn_hmap_init(max_pairn*5);

	int chn_ct = 0;
	const char *odbk_chns[max_pairn];
	const char *odbk_snpsht_chns[max_pairn];
	const char *tick_chns[max_pairn];

	urn_pair pairs[argc];
	for (int i=1; i<argc; i++) {
		char *upcase_s = argv[i];
		urn_s_upcase(upcase_s, strlen(upcase_s));

		// preprocess real pair to its mapping
		// For binance: BUSD-XRP -> USD-XRP
		upcase_s = preprocess_pair(upcase_s);

		// auto completion: xrp -> btc-xrp
		gchar **currency_and_left = g_strsplit(upcase_s, "-", 3);
		if (currency_and_left[1] == NULL) {
			gchar *gs = g_strjoin("-", "BTC", upcase_s, NULL);
			upcase_s = malloc(strlen(gs)+1);
			strcpy(upcase_s, gs);
			g_free(gs);
		}
		g_strfreev(currency_and_left);

		// Check dup args
		char *dupval;
		int dup = urn_hmap_getstr(pair_to_sym, upcase_s, &dupval);
		if (dup) {
			URN_WARNF("Duplicated arg[%d] %s <-> %s", i, upcase_s, dupval);
			continue;
		}
		URN_LOGF("%d -->    %s", i-1, upcase_s);

		// parsing urn_pair
		urn_pair pair = pairs[i-1];
		rv = urn_pair_alloc(&pair, upcase_s, strlen(upcase_s), NULL);
		if (rv != 0)
			return URN_FATAL("Error in parsing pairs", EINVAL);
		// urn_pair_print(pair);

		char *exch_sym = NULL;
		if ((rv = exchange_sym_alloc(&pair, &exch_sym)) != 0) {
			URN_WARNF("Invalid pair arg[%d] %s", i, pair.name);
			continue;
		}
		URN_LOGF("exchange sym %s", exch_sym);

		// Prepare symbol <-> pair map.
		urn_hmap_setstr(pair_to_sym, upcase_s, exch_sym);
		urn_hmap_setstr(sym_to_pair, exch_sym, upcase_s);

		// Prepare channels, some exchanges need snapshot channel.
		char *depth_chn = malloc(64+strlen(exch_sym));
		char *depth_snpsht_chn = malloc(64+strlen(exch_sym));
		char *trade_chn = malloc(64+strlen(exch_sym));
		if (depth_chn == NULL || trade_chn == NULL)
			return URN_FATAL("Not enough memory for creating channel str", ENOMEM);
		mkt_data_odbk_channel(exch_sym, depth_chn);
		mkt_data_odbk_snpsht_channel(exch_sym, depth_snpsht_chn);
		mkt_data_tick_channel(exch_sym, trade_chn);
		odbk_chns[chn_ct] = depth_chn;
		odbk_snpsht_chns[chn_ct] = depth_snpsht_chn;
		tick_chns[chn_ct] = trade_chn;
		URN_LOGF("\targv %d chn %d %s, %s, %s", i, chn_ct, depth_chn, depth_snpsht_chn, trade_chn);
		urn_hmap_setstr(depth_chn_to_pair, depth_chn, upcase_s);
		urn_hmap_setstr(depth_snpsht_chn_to_pair, depth_snpsht_chn, upcase_s);
		urn_hmap_setstr(trade_chn_to_pair, trade_chn, upcase_s);

		int pairid = chn_ct + 1; // 0==NULL
		pair_arr[pairid] = upcase_s;
		pub_odbk_chn_arr[pairid] = malloc(128);
		sprintf(pub_odbk_chn_arr[pairid], "URANUS:%s:%s:full_odbk_channel", exchange, upcase_s);
		pub_odbk_key_arr[pairid] = malloc(128);
		sprintf(pub_odbk_key_arr[pairid], "URANUS:%s:%s:orderbook", exchange, upcase_s);
		pub_tick_chn_arr[pairid] = malloc(128);
		sprintf(pub_tick_chn_arr[pairid], "URANUS:%s:%s:full_tick_channel", exchange, upcase_s);
		pub_tick_key_arr[pairid] = malloc(128);
		sprintf(pub_tick_key_arr[pairid], "URANUS:%s:%s:trades", exchange, upcase_s);
		URN_LOGF("\tpair_arr %p pair_arr[%d] %p %s", pair_arr, pairid, &(pair_arr[pairid]), upcase_s);

		urn_hmap_set(depth_chn_to_pairid, depth_chn, (uintptr_t)pairid);
		urn_hmap_set(depth_snpsht_chn_to_pairid, depth_snpsht_chn, (uintptr_t)pairid);
		urn_hmap_set(trade_chn_to_pairid, trade_chn, (uintptr_t)pairid);
		max_pair_id = pairid;
		chn_ct ++;
	}

	brdcst_max_speed = URN_MIN(max_pair_id*300, 2000);

	URN_INFO("parsing end, building index now");

	urn_hmap_print(pair_to_sym, "pair_to_sym");
	urn_hmap_print(sym_to_pair, "sym_to_pair");
	urn_hmap_print(depth_chn_to_pair, "depth_to_pair");
	urn_hmap_print(depth_snpsht_chn_to_pair, "depth_snpsht_to_pair");
	urn_hmap_print(trade_chn_to_pair, "trade_to_pair");
	urn_hmap_printi(depth_chn_to_pairid, "depth_to_pairid");
	urn_hmap_printi(depth_snpsht_chn_to_pairid, "depth_snpsht_to_pairid");
	urn_hmap_printi(trade_chn_to_pairid, "trade_to_pairid");

	URN_LOGF("Generate req json with %d odbk and tick channels", chn_ct);

	URN_LOG("pair by pairid:");
	for (int i=0; i<=argc; i++)
		URN_LOGF("\tpair_arr %d %s", i, pair_arr[i]);

	mkt_wss_prepare_reqs(chn_ct, odbk_chns, odbk_snpsht_chns, tick_chns);
	return 0;
}

///////////// ODBK INSERT/UPDATE/DELETE //////////////
void mkt_wss_odbk_purge(int pairid) {
	g_list_free_full(bids_arr[pairid], urn_porder_free);
	g_list_free_full(asks_arr[pairid], urn_porder_free);
	bids_arr[pairid] = g_list_alloc();
	bids_arr[pairid]->prev = NULL;
	bids_arr[pairid]->next = NULL;
	asks_arr[pairid] = g_list_alloc();
	asks_arr[pairid]->prev = NULL;
	asks_arr[pairid]->next = NULL;
	askct_arr[pairid] = 0;
	bidct_arr[pairid] = 0;
}

void mkt_wss_odbk_insert(int pairid, urn_inum *p, urn_inum *s, bool buy) {
	urn_porder *o = urn_porder_alloc(NULL, p, s, buy, NULL);
	if (buy) { // bids from low to high, reversed
		GList *bids = bids_arr[pairid];
		if (bids->data == NULL) {
			bids->data = o;
			bidct_arr[pairid]++;
			return;
		}
		bids = g_list_insert_sorted(bids, o, urn_porder_px_cmp);
		URN_DEBUGF_C(GREEN, "\t %s %s insert bids %p->%p o %p", urn_inum_str(p), urn_inum_str(s), bids, bids->data, o);
		if (bidct_arr[pairid]+1 > MAX_DEPTH)
			bids = remove_top_porder(bids);
		else
			bidct_arr[pairid]++;
		bids_arr[pairid] = bids;
		bids->prev = NULL;
	} else { // asks from high to low, reversed
		GList *asks = asks_arr[pairid];
		if (asks->data == NULL) {
			asks->data = o;
			askct_arr[pairid]++;
			return;
		}
		asks = g_list_insert_sorted(asks, o, urn_porder_px_cmprv);
		URN_DEBUGF_C(GREEN, "\t %s %s insert asks %p->%p o %p", urn_inum_str(p), urn_inum_str(s), asks, asks->data, o);
		if (askct_arr[pairid]+1 > MAX_DEPTH)
			asks = remove_top_porder(asks);
		else
			askct_arr[pairid]++;
		asks_arr[pairid] = asks;
		asks->prev = NULL;
	}
}

// return if any odbk operation did
// manage to free p and s by yourself if false is returned.
bool mkt_wss_odbk_update_or_delete(int pairid, urn_inum *p, urn_inum *s, bool buy, bool update) {
	bool found = false;
	int check_idx = -1; // -1 means price not compared
	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];
	GList *node = buy ? bids : asks;
	// Find node with target price
	while(true) {
		if (node->next == NULL && node->data == NULL)
			break;
		if (node->next != NULL && node->data == NULL)
			URN_FATAL("GList->next is not NULL but ->data is NULL", EINVAL);

		check_idx++;
		int cmpv = urn_inum_cmp(p, ((urn_porder *)(node->data))->p);
		found = (cmpv == 0);
		URN_DEBUGF("cmp %s %s = %d",
				urn_inum_str(p),
				urn_inum_str(((urn_porder *)(node->data))->p),
				cmpv
			  );
		if ((buy) && cmpv < 0) break; // bids: p smaller than tail.
		if ((!buy) && cmpv > 0) break; // asks: p higher than tail.
		if (found) break;

		if (node->next == NULL) break;
		node = node->next;
	}
	if (!found) {
		URN_DEBUGF("node not found at check_idx %d", check_idx);
		// abort update when out_of_range && range==max
		if (check_idx == 0) {
			if ((buy && bidct_arr[pairid] >= MAX_DEPTH) ||
					((!buy)&& askct_arr[pairid] >= MAX_DEPTH)) {
				URN_DEBUG("price checked but out of valid range, return not found");
				return false;
			}
		}
		// update should become insert after this
		if (update) {
			mkt_wss_odbk_insert(pairid, p, s, buy);
		}
		return true;
	}

	// found node with same price.
	if (update) { // update p and s only
		urn_porder_free(node->data);
		urn_porder *o = urn_porder_alloc(NULL, p, s, buy, NULL);
		node->data = o;
		URN_DEBUGF_C(BLUE, "update the node %p s %s p %s", node, urn_inum_str(o->s), urn_inum_str(o->p));
	} else { // delete this node.
		urn_porder *o = (urn_porder *)(node->data);
		// free node->data first.
		if (node->data != NULL) {
			URN_DEBUGF_C(RED, "delete the node %p s %s p %s", node, urn_inum_str(o->s), urn_inum_str(o->p));
			urn_porder_free(o);
		} else
			URN_DEBUGF_C(RED, "Delete the node %p NULL data", node);
		node->data = NULL;

		if (node->prev == NULL && node->next == NULL) {
			// node is the only one element, delete data only.
			URN_DEBUGF_C(RED, "clear data only for node %p", node);
		} else if (node->prev == NULL || node == bids_arr[pairid] || node == asks_arr[pairid]) {
			// node is at head, remains after
			URN_DEBUGF_C(RED, "free node %p as head", node);
			if (buy)  bids = node->next;
			if (!buy) asks = node->next;
			g_list_free_1(node);
		} else if (node->next == NULL) { // node is at end
			URN_DEBUGF_C(RED, "free node %p as tail", node);
			node->prev->next = NULL;
			g_list_free_1(node);
		} else { // node is in middle, connect prev<->next
			URN_DEBUGF_C(RED, "free node %p as middle", node);
			node->prev->next = node->next;
			node->next->prev = node->prev;
			g_list_free_1(node);
		}
		if (buy)  bidct_arr[pairid]--;
		if (!buy) askct_arr[pairid]--;
	}
	bids_arr[pairid] = bids;
	bids->prev = NULL;
	asks_arr[pairid] = asks;
	asks->prev = NULL;
	return true;
}
