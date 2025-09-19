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
int   on_odbk(int pairid, yyjson_val *jdata, bool is_top);

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+1);
	if ((*str) == NULL) return ENOMEM;
	if (pair->expiry != NULL)
		return URN_FATAL("Pair with expiry in Bitget", EINVAL);
	// USDT-BTC   -> BTCUSDT
	sprintf(*str, "%s%s", pair->asset, pair->currency);
	urn_s_upcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) { sprintf(s, "Bitget"); }

void mkt_data_set_wss_url(char *s) { sprintf(s, "wss://ws.bitget.com/v2/ws/public"); }

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	struct timeval t_val;
	gettimeofday(&t_val, NULL);
	int cmd_ct = 0;

	/*
	 *   "op": "subscribe",
	 *   "args": [
	 *   {
		 "instType": "SPOT",
		 "channel": "trade",
		 "instId": $symbol
		}, ...
	 */
	char *pre = "{\"op\":\"subscribe\",\"args\":[";
	char *end = "]}";
	char *cmd = malloc(strlen(pre) + chn_ct*70*3 + strlen(end));
	if (cmd == NULL) return ENOMEM;
	// insert channel
	char *pos = cmd;
	pos += sprintf(pos, "%s", pre);
	// insert payload
	for (int i=0; i<chn_ct; i++) {
		pos += sprintf(pos, "{\"instType\":\"SPOT\",\"channel\":\"books15\",\"instId\":\"%s\"},", odbk_chns[i]);
		pos += sprintf(pos, "{\"instType\":\"SPOT\",\"channel\":\"books1\",\"instId\":\"%s\"},", odbk_chns[i]);
		pos += sprintf(pos, "{\"instType\":\"SPOT\",\"channel\":\"trade\",\"instId\":\"%s\"},", odbk_chns[i]);
	}
	pos--; // remove last comma
	pos += sprintf(pos, "%s", end);
	URN_DEBUGF("req %d : %s", cmd_ct, cmd);
	wss_req_s[cmd_ct++] = cmd;

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
	yyjson_val *jarg = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jdata = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	jval = yyjson_obj_get(jroot, "event");
	if (jval != NULL) {
		const char *event = yyjson_get_str(jval);
		if (strcmp(event, "subscribe") == 0) {
			URN_INFOF("<-- %s", msg);
			goto final;
		} else {
			URN_WARNF("??? %s", msg);
			goto error;
		}
	}

	URN_RET_ON_NULL(jarg = yyjson_obj_get(jroot, "arg"), "No arg", EINVAL);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jarg, "channel"), "No arg/channel", EINVAL);
	const char *channel= yyjson_get_str(jval);
	URN_RET_ON_NULL(channel, "No channel", EINVAL);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jarg, "instId"), "No arg/symbol", EINVAL);
	const char *symbol = yyjson_get_str(jval);
	URN_RET_ON_NULL(symbol, "No symbol", EINVAL);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "action"), "No action", EINVAL);
	const char *action = yyjson_get_str(jval);
	URN_RET_ON_NULL(action, "No action", EINVAL);

	uintptr_t pairid = 0;
	urn_hmap_getptr(symb_to_pairid, symbol, &pairid);
	if (pairid == 0) {
		URN_WARNF("Unknown symbol %s\n%s", symbol, msg);
		goto error;
	}
	char *pair = pair_arr[pairid];

	URN_RET_ON_NULL(jdata = yyjson_obj_get(jroot, "data"), "No data", EINVAL);
	yyjson_val *v_el;
	size_t idx, max;
	if (strcmp(channel, "books1") == 0) {
		yyjson_arr_foreach(jdata, idx, max, v_el) {
			URN_RET_ON_NULL(jval = yyjson_obj_get(v_el, "ts"), "No data[]/ts", EINVAL);
			const unsigned long ts_e3 = yyjson_get_uint(jval);
			on_odbk(pairid, v_el, true);
			odbk_t_arr[pairid] = ts_e3 * 1000;
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
		}
		goto final;
	} else if (strcmp(channel, "books15") == 0) {
		if (strcmp(action, "snapshot") == 0)
			mkt_wss_odbk_purge(pairid); // clear bids and asks for new snapshot.
		yyjson_arr_foreach(jdata, idx, max, v_el) {
			URN_RET_ON_NULL(jval = yyjson_obj_get(v_el, "ts"), "No data[]/ts", EINVAL);
			const unsigned long ts_e3 = yyjson_get_uint(jval);
			on_odbk(pairid, v_el, false);
			odbk_t_arr[pairid] = ts_e3 * 1000;
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
		}
		goto final;
	} else if (strcmp(channel, "trade") == 0) {
		yyjson_arr_foreach(jdata, idx, max, v_el) {
			URN_GO_FINAL_ON_RV(on_tick(pairid, v_el), "Err in tick handling")
			URN_GO_FINAL_ON_RV(tick_updated(pairid), "Err in tick_updated()")
		}
		goto final;
	} else {
		URN_WARNF("Unknown channel%s\n%s", channel, msg);
	}

error:
	// Unknown type
	URN_GO_FINAL_ON_RV(EINVAL, msg);

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}

// *type: "A" "B"
// op_type: 0 order_book - snapshot
// op_type: 1 insert     - delta
// op_type: 2 update     - delta
// op_type: 3 delete     - delta
// op_type: 4 guess      - update or delete, delete if size is zero
int parse_n_mod_odbk_porder(int pairid, const char *type, yyjson_val *v, int op_type, bool is_top) {
	if (op_type < 4)
		return URN_FATAL("Unexpected op_type, Bitget orderbook only has guess (& top) type", EINVAL);

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

	URN_DEBUGF("\tOP %d buy %d p %s s %s", op_type, buy, urn_inum_str(p), urn_inum_str(s));

	if (is_top) { // Insert, and trim data out of price
		mkt_wss_odbk_update_top(pairid, p, s, buy);
		if (op_type == 2)
			need_free_ps = false;
	} else {
		// update or delete, should not become insert or snapshot
		bool op_done = mkt_wss_odbk_update_or_delete(pairid, p, s, buy, (op_type==2));
		if ((op_type == 2) && op_done)
			need_free_ps = false;
	}

final:
	print_odbk(pairid);
	if (need_free_ps) {
		if (p != NULL) free(p);
		if (s != NULL) free(s);
	}
	return 0;
}

int on_odbk(int pairid, yyjson_val *jdata, bool is_top) {
	URN_DEBUGF("\ton_odbk %d %s", pairid, pair_arr[pairid]);
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *orders = NULL;

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "bids"), "No bids", EINVAL);
	char ask_or_bid = 0;
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'B';
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4, is_top), "Error in parse_n_mod_odbk_porder() for bids");
		if (is_top && idx >= 1)
			URN_WARN("Multiple data in top/bid");
	}

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "asks"), "No asks", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'A';
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4, is_top), "Error in parse_n_mod_odbk_porder() for asks");
		if (is_top && idx >= 1)
			URN_WARN("Multiple data in top/ask");
	}

	URN_DEBUGF("\ton_odbk done");
	return 0;
}

int on_tick(int pairid, yyjson_val *jdata) {
	/*
	 * "ts": "ts_e3",
	 * "side": "sell",
	 * "size": "16.4700000000",
	 * "price": "0.4705000000",
	 */
	URN_DEBUGF("\ton_tick %d", pairid);
	char* pair = pair_arr[pairid];
	int rv = 0;
	const char *pstr=NULL, *sstr=NULL, *side=NULL;
	yyjson_val *jval = NULL;
	long ts_e6 = -1;

	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "ts"), "No ts", EINVAL);
	const char *ts = yyjson_get_str(jval);
	URN_RET_ON_NULL(ts, "No ts str", EINVAL);
	sscanf(ts, "%ld", &ts_e6);
	ts_e6 *= 1000;

	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "price"), "No price", EINVAL);
	URN_RET_ON_NULL(pstr = yyjson_get_str(jval), "No price str", EINVAL);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "size"), "No size", EINVAL);
	URN_RET_ON_NULL(sstr = yyjson_get_str(jval), "No size str", EINVAL);
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
