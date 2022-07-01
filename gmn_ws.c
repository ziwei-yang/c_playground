// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

// local options
#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

//#define PUB_NO_REDIS

#include "mkt_wss.h"

int   on_odbk_update(int pairid, const char *type, yyjson_val *jdata);

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+1);
	if ((*str) == NULL) return ENOMEM;
	if (pair->expiry != NULL)
		return URN_FATAL("Pair with expiry in Gemini", EINVAL);
	// USDT-BTC   -> BTCUSDT
	sprintf(*str, "%s%s", pair->asset, pair->currency);
	urn_s_upcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) {
	// not so active, wait longer.
	wss_stat_max_msg_t = 900;
	sprintf(s, "Gemini");
}

void mkt_data_set_wss_url(char *s) { sprintf(s, "wss://api.gemini.com/v2/marketdata/"); }

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	/*
	"type": "subscribe",
	"subscriptions":[
		{"name":"l2","symbols":[...]} // Gemini l2 includes trades and heartbeat
	]
	*/
	char *pre = "{\"type\":\"subscribe\",\"subscriptions\":[{\"name\":\"l2\",\"symbols\":[";
	char *end = "]}]}";
	char *cmd = malloc(strlen(pre) + strlen(end) + chn_ct*16);
	if (cmd == NULL) return ENOMEM;
	strcpy(cmd, pre);
	char *pos = cmd + strlen(pre);
	for (int i=0; i<chn_ct; i++) {
		*(pos++) = '"';
		strcpy(pos, odbk_chns[i]);
		pos = pos + strlen(odbk_chns[i]);
		*(pos++) = '"';
		*(pos++) = ',';
	}
	pos--; // remove last comma
	strcpy(pos, end);

	int cmd_ct = 0;
	wss_req_s[cmd_ct] = cmd;
	URN_DEBUGF("req %d : %s", cmd_ct, cmd);
	cmd_ct++;

	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	wss_req_interval_e = 1;
	wss_req_interval_ms = 100;
	return 0;
}

int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "type"), "No type", EINVAL);
	const char *type = yyjson_get_str(jval);
	URN_RET_ON_NULL(type, "No type", EINVAL);

	if (strcmp(type, "trade") == 0) {
		goto final;
	}

	// No server time, use local time only.
	struct timeval odbk_mkt_t;
	gettimeofday(&odbk_mkt_t, NULL);

	if (strcmp(type, "heartbeat") == 0) {
		// {"type":"heartbeat","timestamp":1656683731256}
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "timestamp"), "No timestamp", EINVAL);
		long server_ts_e3 = yyjson_get_sint(jval);
		// Calculate latency only, only heartbeat has server ts
		long ts_e3 = (long)(odbk_mkt_t.tv_sec) * 1000l + (long)(odbk_mkt_t.tv_usec/1000);
		long latency_e3 = ts_e3 - server_ts_e3;
		URN_LOGF("heartbeat latency %ldms", latency_e3);
		goto final;
	}

	if (strcmp(type, "l2_updates") != 0) {
		URN_WARNF("Unknown type %s\n%s", type, msg);
		goto error;
	}

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "symbol"), "No symbol", EINVAL);
	const char *symbol = yyjson_get_str(jval);
	URN_RET_ON_NULL(symbol, "No symbol", EINVAL);
	uintptr_t pairid = 0;
	urn_hmap_getptr(symb_to_pairid, symbol, &pairid);
	if (pairid == 0) {
		URN_WARNF("Unknown symbol %s\n%s", symbol, msg);
		goto error;
	}

	yyjson_val *data = NULL;
	URN_RET_ON_NULL(data = yyjson_obj_get(jroot, "changes"), "No changes", EINVAL);

	// l2_updates processing here.
	newodbk_arr[pairid] ++;
	odbk_t_arr[pairid] = (long)(odbk_mkt_t.tv_sec) * 1000000l + (long)(odbk_mkt_t.tv_usec);
	char *depth_pair = pair_arr[pairid];
	URN_DEBUGF("\t -> odbk update %3lu %s", pairid, depth_pair);
	URN_GO_FINAL_ON_RV(on_odbk_update(pairid, NULL, data), "Err in odbk handling")
	URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
	goto final;

error:
	// Unknown type
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

	// always ["buy"/"sell", "price", "size"]
	bool side_price_size = true;
	bool buy;
	int rv = 0;

	if (op_type != 4)
		return URN_FATAL("Unexpected op_type, Gemini only has guess type", EINVAL);

	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];

	yyjson_val *v_el;
	int el_ct = 0;
	size_t idx, max;
	const char *side;
	yyjson_arr_foreach(v, idx, max, v_el) {
		if (el_ct == 0) { // buy or sell
			side = yyjson_get_str(v_el);
			if (strcmp(side, "buy") == 0) {
				buy = true;
			} else if (strcmp(side, "sell") == 0) {
				buy = false;
			} else
				return URN_FATAL("Unexpected buy/sell side in order", EINVAL);
		} else if (el_ct == 1) { // price
			urn_inum_alloc(&p, yyjson_get_str(v_el));
		} else if (el_ct == 2) { // size
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

int on_odbk_update(int pairid, const char *type, yyjson_val *jdata) {
	URN_DEBUGF("\ton_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *orders = NULL;

	yyjson_arr_foreach(jdata, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, NULL, v, 4),
			"Error in parse_n_mod_odbk_porder() for bids");
	}
	return 0;
}
