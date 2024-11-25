// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

// local options
#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

//#define PUB_NO_REDIS

#include "mkt_wss.h"

int   on_tick(int pairid, yyjson_val *jdata);
int   on_odbk_top(int pairid, yyjson_val *data);
int   on_odbk(int pairid, yyjson_val *jdata);

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+1);
	if ((*str) == NULL) return ENOMEM;
	if (pair->expiry != NULL)
		return URN_FATAL("Pair with expiry in Gate", EINVAL);
	// USDT-BTC   -> BTC_USDT
	sprintf(*str, "%s_%s", pair->asset, pair->currency);
	urn_s_upcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) {
	// not so active, wait longer.
	wss_stat_max_msg_t = 1200;
	sprintf(s, "Gate");
}

void mkt_data_set_wss_url(char *s) { sprintf(s, "wss://api.gateio.ws/ws/v4/"); }

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	struct timeval t_val;
	gettimeofday(&t_val, NULL);
	int cmd_ct = 0;

	/*
	 *   "event": "subscribe",
	 *   "channel": "spot.book_ticker",
	 *   "payload": ["BTC_USDT", "GT_USDT"],
	 *   "time": 1611541000
	 */
	char *pre = "{\"event\":\"subscribe\",\"channel\":\"spot.book_ticker\",\"payload\":[";
	char *mid = "],\"time\":";
	char *cmd = malloc(strlen(pre) + strlen(mid) + chn_ct*16);
	if (cmd == NULL) return ENOMEM;
	// insert channel
	char *pos = cmd;
	pos += sprintf(pos, "%s", pre);
	// insert payload
	for (int i=0; i<chn_ct; i++)
		pos += sprintf(pos, "\"%s\",", odbk_chns[i]);
	pos--; // remove last comma
	// append time in second
	pos += sprintf(pos, "%s%ld}", mid, t_val.tv_sec);
	URN_DEBUGF("req %d : %s", cmd_ct, cmd);
	wss_req_s[cmd_ct++] = cmd;

	/*
	 *   "event": "subscribe",
	 *   "channel": "spot.trades",
	 *   "payload": ["BTC_USDT", "GT_USDT"],
	 *   "time": 1611541000
	 */
	pre = "{\"event\":\"subscribe\",\"channel\":\"spot.trades\",\"payload\":[";
	cmd = malloc(strlen(pre) + strlen(mid) + chn_ct*16);
	if (cmd == NULL) return ENOMEM;
	// insert channel
	pos = cmd;
	pos += sprintf(pos, "%s", pre);
	// insert payload
	for (int i=0; i<chn_ct; i++)
		pos += sprintf(pos, "\"%s\",", odbk_chns[i]);
	pos--; // remove last comma
	// append time in second
	pos += sprintf(pos, "%s%ld}", mid, t_val.tv_sec);
	URN_DEBUGF("req %d : %s", cmd_ct, cmd);
	wss_req_s[cmd_ct++] = cmd;

	// For each pair, subscribe snapshot
	/*
	 * "event": "subscribe",
	 * "channel": "spot.order_book",
	 * "payload": ["BTC_USDT", "5", "100ms"]
	 * "time": 1611541000
	 */
	for (int i=0; i<chn_ct; i++) {
		cmd = malloc(256);
		if (cmd == NULL) return ENOMEM;
		sprintf(cmd, "{\"event\":\"subscribe\",\"channel\":\"spot.order_book\",\"payload\":[\"%s\",\"10\",\"100ms\"],\"time\":%ld}",
				odbk_chns[i], t_val.tv_sec);
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		wss_req_s[cmd_ct++] = cmd;
	}

	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	wss_req_interval_e = 1;
	wss_req_interval_ms = 50;
	return 0;
}

// Gate wss json message might be sliced.
int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jresult = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "channel"), "No channel", EINVAL);
	const char *channel= yyjson_get_str(jval);
	URN_RET_ON_NULL(channel, "No channel", EINVAL);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "event"), "No event", EINVAL);
	const char *event = yyjson_get_str(jval);
	URN_RET_ON_NULL(event, "No event", EINVAL);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "time_ms"), "No time_ms", EINVAL);
	const unsigned long ts_e3 = yyjson_get_uint(jval);

	URN_RET_ON_NULL(jresult = yyjson_obj_get(jroot, "result"), "No result", EINVAL);
	if (strcmp(event, "subscribe") == 0) {
		URN_RET_ON_NULL(jval = yyjson_obj_get(jresult, "status"), "No result/status", EINVAL);
		const char *status = yyjson_get_str(jval);
		URN_RET_ON_NULL(event, "No result/status str", EINVAL);
		if (strcmp(status, "success") == 0) {
			URN_INFOF("<-- %s", msg);
		} else {
			URN_WARNF("<-X %s", msg);
		}
		goto final;
	} else if (strcmp(event, "update") == 0) {
		// Extract symbol first
		if (strcmp(channel, "spot.trades") == 0) {
			URN_RET_ON_NULL(jval = yyjson_obj_get(jresult, "currency_pair"), "No result/currency_pair", EINVAL);
		} else {
			URN_RET_ON_NULL(jval = yyjson_obj_get(jresult, "s"), "No result/s", EINVAL);
		}

		const char *symbol = yyjson_get_str(jval);
		URN_RET_ON_NULL(event, "No result/s str", EINVAL);
		uintptr_t pairid = 0;
		urn_hmap_getptr(symb_to_pairid, symbol, &pairid);
		if (pairid == 0) {
			URN_WARNF("Unknown symbol %s\n%s", symbol, msg);
			goto error;
		}
		char *pair = pair_arr[pairid];
		odbk_t_arr[pairid] = ts_e3 * 1000;

		if (strcmp(channel, "spot.book_ticker") == 0) {
			URN_DEBUGF("\t -> odbk top %3lu %s", pairid, pair);
			URN_GO_FINAL_ON_RV(on_odbk_top(pairid, jresult), "Err in odbk handling")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
			goto final;
		} else if (strcmp(channel, "spot.order_book") == 0) {
			URN_DEBUGF("\t -> odbk snapshot %3lu %s", pairid, pair);
			URN_GO_FINAL_ON_RV(on_odbk(pairid, jresult), "Err in odbk handling")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
			goto final;
		} else if (strcmp(channel, "spot.trades") == 0) {
			URN_GO_FINAL_ON_RV(on_tick(pairid, jresult), "Err in tick handling")
			URN_GO_FINAL_ON_RV(tick_updated(pairid), "Err in tick_updated()")
			goto final;
		}
	}

error:
	// Unknown type
	URN_GO_FINAL_ON_RV(EINVAL, msg);

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}

int on_odbk_top(int pairid, yyjson_val *data) {
	char* pair = pair_arr[pairid];
	URN_DEBUGF("\ton_odbk top %d %s", pairid, pair);

	// must free manually if insertion failed.
	urn_inum *ap=NULL, *as=NULL; // ask px sz
	urn_inum *bp=NULL, *bs=NULL; // bid px sz
	urn_inum_alloc(&ap, yyjson_get_str(yyjson_obj_get(data, "a")));
	urn_inum_alloc(&as, yyjson_get_str(yyjson_obj_get(data, "A")));
	urn_inum_alloc(&bp, yyjson_get_str(yyjson_obj_get(data, "b")));
	urn_inum_alloc(&bs, yyjson_get_str(yyjson_obj_get(data, "B")));

	mkt_wss_odbk_update_top(pairid, ap, as, false);
	mkt_wss_odbk_update_top(pairid, bp, bs, true);

	URN_DEBUGF("\ton_odbk top done");
	return 0;
}

// *type: "A" "B"
// op_type: 0 order_book - snapshot
// op_type: 1 insert     - delta
// op_type: 2 update     - delta
// op_type: 3 delete     - delta
// op_type: 4 guess      - update or delete, delete if size is zero
int parse_n_mod_odbk_porder(int pairid, const char *type, yyjson_val *v, int op_type) {
	if (op_type != 4)
		return URN_FATAL("Unexpected op_type, Gate only has guess type", EINVAL);

	int rv = 0;
	urn_inum *p=NULL, *s=NULL; // must free by hand if insertion failed.
	bool need_free_ps = true; // free when delete, or update failed.
	char prices[32], sizes[32];
	bool buy;

	if (type[0] == 'B')
		buy = true;
	else if (type[0] == 'A')
		buy = false;
	else
		return URN_FATAL("Unexpected buy/sell side in type", EINVAL);

	yyjson_val *v_el;
	int el_ct = 0;
	size_t idx, max;
	yyjson_arr_foreach(v, idx, max, v_el) {
		if (el_ct == 0) { // price
			urn_inum_alloc(&p, yyjson_get_str(v_el));
		} else if (el_ct == 1) { // size
			urn_inum_alloc(&s, yyjson_get_str(v_el));
		} else
			return URN_FATAL("Unexpected attr ct in order, should be 3", EINVAL);
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

int on_odbk(int pairid, yyjson_val *jdata) {
	// clear bids and asks for new snapshot.
	mkt_wss_odbk_purge(pairid);

	URN_DEBUGF("\ton_odbk %d", pairid);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *orders = NULL;

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "bids"), "No bids", EINVAL);
	char ask_or_bid = 0;
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'B';
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4), "Error in parse_n_mod_odbk_porder() for bids");
	}

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "asks"), "No asks", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'A';
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4), "Error in parse_n_mod_odbk_porder() for asks");
	}
	return 0;
}

int on_tick(int pairid, yyjson_val *jdata) {
	/*
	 * "create_time_ms": "1606292218213.4578",
	 * "side": "sell",
	 * "amount": "16.4700000000",
	 * "price": "0.4705000000",
	 */
	URN_DEBUGF("\ton_tick %d", pairid);
	char* pair = pair_arr[pairid];
	int rv = 0;
	const char *pstr=NULL, *sstr=NULL, *side=NULL;
	yyjson_val *jval = NULL;
	long ts_e6 = -1;

	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "create_time_ms"), "No create_time_ms", EINVAL);
	const char *create_time_ms;
	create_time_ms = yyjson_get_str(jval);
	URN_RET_ON_NULL(create_time_ms, "No create_time_ms str", EINVAL);
	char *stopstring; 
	ts_e6 = (long)(strtod(create_time_ms, &stopstring) * 1000);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "price"), "No price", EINVAL);
	URN_RET_ON_NULL(pstr = yyjson_get_str(jval), "No price str", EINVAL);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "amount"), "No amount", EINVAL);
	URN_RET_ON_NULL(sstr = yyjson_get_str(jval), "No amount str", EINVAL);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "side"), "No side", EINVAL);
	URN_RET_ON_NULL(side = yyjson_get_str(jval), "No side str", EINVAL);

	urn_inum p, s;
	urn_inum_parse(&p, pstr);
	urn_inum_parse(&s, sstr);
	bool buy;
	if (strcmp(side, "buy") == 0)
		buy = true;
	else if (strcmp(side, "sell") == 0)
		buy = false;
	else
		URN_RET_ON_NULL(NULL, "Unexpected side str", EINVAL);

	// wss_mkt_ts = ts_e6; // Dont set as market latest msg ts
	urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);

	return 0;
}
