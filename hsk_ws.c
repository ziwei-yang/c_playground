// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

//#define PUB_NO_REDIS

#include "mkt_wss.h"

int   on_tick(int pairid, yyjson_val *jdata);
int   on_odbk(int pairid, uint64_t ts_e3, yyjson_val *jdata);

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	*str = malloc(strlen(pair->name) + 16);
	if ((*str) == NULL) return ENOMEM;
	// USDT-BTC   -> BTCUSDT
	// USDT-BTC@P -> BTCUSDT-PERPETUAL
	if (pair->expiry == NULL)
		sprintf(*str, "%s%s", pair->asset, pair->currency);
	else {
		if (strcmp(pair->expiry, "P") == 0) {
			sprintf(*str, "%s%s-PERPETUAL", pair->asset, pair->currency);
		} else {
			urn_pair_print(*pair);
			return URN_FATAL("Not expected pair", EINVAL);
		}
	}
	urn_s_upcase(*str, strlen(*str));
	return 0;
}

int hsk_wss_mode = -1; // 0: hashkey hong kong, 1: hashkey global
void mkt_data_set_exchange(char *s) {
	char *exch = getenv("uranus_spider_exchange");
	if (exch == NULL) {
		URN_FATAL("check uranus_spider_exchange ENV VAL", EINVAL);
	} else if (strcasecmp(exch, "hashkey") == 0) {
		hsk_wss_mode = 0;
		sprintf(s, "Hashkey");
	} else if (strcasecmp(exch, "hashkeyg") == 0) {
		hsk_wss_mode = 1;
		sprintf(s, "HashkeyG");
	} else {
		URN_FATAL("check uranus_spider_exchange ENV VAL", EINVAL);
	}
}

void mkt_data_set_wss_url(char *s) {
	if (hsk_wss_mode == 0)
		sprintf(s, "wss://stream-pro.hashkey.com/quote/ws/v1");
	else if (hsk_wss_mode == 1)
		sprintf(s, "wss://stream-glb.hashkey.com/quote/ws/v1");
	else
		URN_FATAL("Unexpected hsk_wss_mode", EINVAL);
}

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int wss_req_append_cmd_ping() {
	struct timeval local_t;
	gettimeofday(&local_t, NULL);
	long ts_e3 = local_t.tv_sec * 1000 + local_t.tv_usec / 1000;
	char *ping_cmd = malloc(64);
	if (ping_cmd == NULL) return ENOMEM;
	sprintf(ping_cmd, "{\"ping\":%ld}", ts_e3);
	return wss_req_append(ping_cmd);
}

double multiplier[MAX_PAIRS] = {0}; // index starts from 1, pairid = chn_id + 1
int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	/*
	 * {
	 *   "symbol": "BTCUSDT", // or ETHUSDT-PERPETUAL
	 *   "topic": "depth", // or trade
	 *   "event": "sub",
	 *   "params": {
	 *     "binary": False
	 *   },
	 *   "id": 1
	 * }
	 */
	char *pre = "{\"symbol\":\"";
	char *mid      = "\",\"topic\":\"depth\",\"event\":\"sub\",\"params\":{\"binary\":false},\"id\":";
	char *mid_tick = "\",\"topic\":\"trade\",\"event\":\"sub\",\"params\":{\"binary\":false},\"id\":";
	char *end = "}";
	int rv = 0;

	URN_RET_ON_RV(wss_req_append_cmd_ping(), "Failed in appending ping cmd");

	for (int i=0; i<chn_ct; i++) {
		char *cmd = malloc(strlen(pre) + 12 + strlen(mid) + 3 + strlen(end));
		if (cmd == NULL) return ENOMEM;
		sprintf(cmd, "%s%s%s%d%s", pre, odbk_chns[i], mid, i*2, end);
		URN_RET_ON_RV(wss_req_append(cmd), "Failed in appending cmd");
		if (strstr(odbk_chns[i], "PERPETUAL") == NULL)
			multiplier[i+1] = 0;
		else
			multiplier[i+1] = 0.001; // perp multiplier 0.001
		URN_INFOF("multiplier %d for %s is %lf", i+1, odbk_chns[i], multiplier[i+1]);

		char *cmd2 = malloc(strlen(pre) + 12 + strlen(mid_tick) + 3 + strlen(end));
		if (cmd2 == NULL) return ENOMEM;
		sprintf(cmd2, "%s%s%s%d%s", pre, tick_chns[i], mid_tick, i*2+1, end);
		URN_RET_ON_RV(wss_req_append(cmd2), "Failed in appending cmd");
	}

	URN_INFOF("Parsing ARGV end, %d req str prepared.", chn_ct*2);
	wss_req_interval_e = 0;
	wss_req_interval_ms = 0;
	return 0;
}

uint64_t last_ping_ts_e3 = 0;
int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jparams = NULL;
	yyjson_val *jdata = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	jval = yyjson_obj_get(jroot, "pong");
	if (jval != NULL)
		goto final;

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "topic"), "No topic", EINVAL);
	const char *topic = yyjson_get_str(jval);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "symbol"), "No symbol", EINVAL);
	const char *symbol = yyjson_get_str(jval);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "sendTime"), "No sendTime", EINVAL);
	uint64_t ts_e3 = yyjson_get_uint(jval);

	if (ts_e3 - last_ping_ts_e3 >= 9000) {
		// client is required to initiate a periodic heartbeat message every 10s
		wss_req_append_cmd_ping();
		last_ping_ts_e3 = ts_e3;
	}

	URN_RET_ON_NULL(jdata = yyjson_obj_get(jroot, "data"), "No data", EINVAL);
	if (strcmp(topic, "depth") == 0) {
		yyjson_val *v;
		size_t idx, max;
		yyjson_arr_foreach(jdata, idx, max, v) {
			// Element has its own symbol and timestamp.
			URN_RET_ON_NULL(jval = yyjson_obj_get(v, "t"), "No data.t", EINVAL);
			ts_e3 = yyjson_get_uint(jval);

			URN_RET_ON_NULL(jval = yyjson_obj_get(v, "s"), "No data.s", EINVAL);
			symbol = yyjson_get_str(jval);
			uintptr_t pairid = 0;
			urn_hmap_getptr(depth_chn_to_pairid, symbol, &pairid);
			if (pairid <= 0) {
				URN_WARNF("Unknown symbol %s", symbol);
				goto final;
			}

			char *depth_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk %3lu %s %llu %s", pairid, depth_pair, ts_e3, topic);
			wss_mkt_ts = ts_e3 * 1000;
			odbk_t_arr[pairid] = ts_e3 * 1000;
			URN_GO_FINAL_ON_RV(on_odbk(pairid, ts_e3, v), "Err in on_odbk()")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
		}
		goto final;
	} else if (strcmp(topic, "trade") == 0) {
		uintptr_t pairid = 0;
		urn_hmap_getptr(depth_chn_to_pairid, symbol, &pairid);
		char *trade_pair = pair_arr[pairid];
		URN_DEBUGF("\t -> tick %3lu %s %llu %sf", pairid, trade_pair, ts_e3, topic);
		URN_GO_FINAL_ON_RV(on_tick(pairid, jdata), "Err in on_tick()")
		URN_GO_FINAL_ON_RV(tick_updated(pairid, 0), "Err in tick_updated()")
		goto final;
	} else {
		URN_WARNF("Unknown topic %s", topic);
		goto final;
	}

error:
	URN_WARNF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);
	URN_GO_FINAL_ON_RV(EINVAL, msg);

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}

// *type: 'A' ask, 'B' bid
// op_type: 0 order_book - snapshot
// op_type: 1 insert     - delta
// op_type: 2 update     - delta
// op_type: 3 delete     - delta
// op_type: 4 guess      - update or delete, delete if size is zero
int parse_n_mod_odbk_porder(int pairid, const char *type, yyjson_val *v, int op_type) {
	urn_inum *p=NULL, *s=NULL; // must free by hand if insertion failed.
	bool need_free_ps = true; // free when delete, or update failed.

	char prices[32], sizes[32];

	// ["price", "size"]
	bool buy;
	int rv = 0;

	if ((*type) == 'B')
		buy = true;
	else if ((*type) == 'A')
		buy = false;
	else
		return URN_FATAL("Unexpected order type", EINVAL);

	if (op_type != 4)
		return URN_FATAL("Unexpected op_type, Hashkey only has guess type", EINVAL);

	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];

	yyjson_val *v_el;
	int el_ct = 0;
	size_t idx, max;
	const char *side;
	yyjson_arr_foreach(v, idx, max, v_el) {
		if (el_ct == 0) { // price
			urn_inum_alloc(&p, yyjson_get_str(v_el));
		} else if (el_ct == 1) { // size
			urn_inum_alloc(&s, yyjson_get_str(v_el));
			if (multiplier[pairid] > 0)
				urn_inum_mul(s, multiplier[pairid]);
		} else
			return URN_FATAL("Unexpected attr ct in order, should be 2", EINVAL);
		el_ct ++;
	}

	if ((s->intg == 0) && (s->frac_ext == 0))
		op_type = 3;
	else
		op_type = 2;

	URN_DEBUGF("\tOP %d %d %s %s", op_type, buy, urn_inum_str(p), urn_inum_str(s));

	// update or delete, should not become insert or snapshot
	bool op_done = mkt_wss_odbk_update_or_delete(pairid, p, s, buy, (op_type==2));
	if ((op_type == 2) && op_done)
		need_free_ps = false;

final:
	print_odbk(pairid);
	if (need_free_ps) {
		if (p != NULL) free(p);
		if (s != NULL) free(s);
	}
	return 0;
}

int on_odbk(int pairid, uint64_t ts_e3, yyjson_val *jdata) {
	URN_DEBUGF("\ton_odbk snapshot %d", pairid);
	// clear bids and asks for new snapshot.
	mkt_wss_odbk_purge(pairid);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *bids, *asks;
	URN_RET_ON_NULL(bids = yyjson_obj_get(jdata, "b"), "No data/b", EINVAL);
	URN_RET_ON_NULL(asks = yyjson_obj_get(jdata, "a"), "No data/a", EINVAL);

	yyjson_val *v;
	size_t idx, max;
	yyjson_arr_foreach(bids, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, "B", v, 4),
			"Error in parse_n_mod_odbk_porder() for bids");
	}

	yyjson_arr_foreach(asks, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, "A", v, 4),
			"Error in parse_n_mod_odbk_porder() for bids");
	}
	return 0;
}

int on_tick(int pairid, yyjson_val *jdata) {
	int rv = 0;
	yyjson_val *v;
	size_t idx, max;
	// "v": "1447335405363150849",   // Transaction ID
	// "t": 1687271825415,           // Time
	// "p": "10001",                 // Price
	// "q": "0.001",                 // Quantity
	// "m": false                    // true = buy, false = sell
	yyjson_arr_foreach(jdata, idx, max, v) {
		// "m":false
		yyjson_val *jval = NULL;
		const char *p_str=NULL, *s_str=NULL;
		bool buy;
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "t"), "No data/t", EINVAL);
		unsigned long ts_e6 = yyjson_get_uint(jval) * 1000l;

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "p"), "No data/p", EINVAL);
		URN_RET_ON_NULL((p_str = yyjson_get_str(jval)), "No data/p str val", EINVAL);

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "q"), "No data/q", EINVAL);
		URN_RET_ON_NULL((s_str = yyjson_get_str(jval)), "No data/q str val", EINVAL);

		urn_inum p, s;
		urn_inum_parse(&p, p_str);
		urn_inum_parse(&s, s_str);
		if (multiplier[pairid] > 0)
			urn_inum_mul(&s, multiplier[pairid]);

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "m"), "No data/m", EINVAL);
		buy = !(yyjson_get_bool(jval));

		urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);
	}
	return 0;
}

