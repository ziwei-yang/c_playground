// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

//#define PUB_NO_REDIS

#include "mkt_wss.h"

int   on_tick(int pairid, yyjson_val *jdata);
int   on_odbk(int pairid, const char *type, yyjson_val *jdata);
int   on_odbk_update(int pairid, const char *type, yyjson_val *jdata);

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	*str = malloc(strlen(pair->name));
	if ((*str) == NULL) return ENOMEM;
	// USDT-BTC@P -> BTCUSDT // Only for Perp and Spot, no expiry support TODO
	// USDT-BTC   -> BTCUSDT // WSS URL is different for linear/spot/inverse
	sprintf(*str, "%s%s", pair->asset, pair->currency);
	urn_s_upcase(*str, strlen(*str));
	return 0;
}

int byb_wss_mode = -1; // 0: Bybit Inverse 1: Linear 2: Spot
void mkt_data_set_exchange(char *s) {
	char *exch = getenv("uranus_spider_exchange");
	if (strcasecmp(exch, "bybit") == 0) {
		byb_wss_mode = 2;
		sprintf(s, "Bybit"); // Spot mode for default name Bybit
	} else if (strcasecmp(exch, "bybitl") == 0) {
		byb_wss_mode = 1;
		sprintf(s, "BybitL"); // Linear
	} else if (strcasecmp(exch, "bybiti") == 0) {
		byb_wss_mode = 0;
		sprintf(s, "BybitI"); // Inverse
	} else
		URN_FATAL("check uranus_spider_exchange ENV VAL", EINVAL);
}

void mkt_data_set_wss_url(char *s) {
	if (byb_wss_mode == 0) // Inverse
		sprintf(s, "wss://stream.bybit.com/v5/public/inverse");
	else if (byb_wss_mode == 1) // Linear
		sprintf(s, "wss://stream.bybit.com/v5/public/linear");
	else if (byb_wss_mode == 2) // Spot
		sprintf(s, "wss://stream.bybit.com/v5/public/spot");
	else
		URN_FATAL("Unexpected byb_wss_mode", EINVAL);
}

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "orderbook.50.%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "publicTrade.%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	/*
	 * "op": "subscribe",
	 * "args": [
	 *     "orderbook.1.BTCUSDT"
	 *     "ticker.BTCUSDT"
	 * ]
	 */
	int cmd_ct = 0;
	char *pre = "{\"op\":\"subscribe\",\"args\":[";
	char *end = "]}";

	// For spot, max 10 args per command, we request 2 here (odbk + trade).
	// Could do 5 symbols per command but why bother making this complex
	for (int i=0; i<chn_ct; i++) {
		char *cmd = malloc(strlen(pre) + strlen(end) + 64);
		if (cmd == NULL) return ENOMEM;
		char *pos = cmd;
		pos += sprintf(pos, "%s", pre);
		pos += sprintf(pos, "%s", "\"");
		pos += sprintf(pos, "%s", odbk_chns[i]);
		pos += sprintf(pos, "%s", "\",\"");
		pos += sprintf(pos, "%s", tick_chns[i]);
		pos += sprintf(pos, "%s", "\"");
		pos += sprintf(pos, "%s", end);

		wss_req_s[cmd_ct] = cmd;
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;
	}

	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	wss_req_interval_e = 1;
	wss_req_interval_ms = 50;
	return 0;
}

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

	jval = yyjson_obj_get(jroot, "op");
	if (jval != NULL) {
		const char *op = yyjson_get_str(jval);
		URN_RET_ON_NULL(op, "Has op but no str val", EINVAL);
		if (strcmp(op, "pong") == 0)
			goto final;

		jdata = yyjson_obj_get(jroot, "success");
		URN_RET_ON_NULL(jdata, "Has op but no success", EINVAL);
		if (yyjson_get_bool(jdata)) {
			URN_DEBUGF("on_wss_msg omit success op %s", msg);
			goto final;
		} else {
			URN_WARNF("on_wss_msg unknown event %s", msg);
			goto final;
		}
	}

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "topic"), "No topic", EINVAL);
	const char *topic = yyjson_get_str(jval);
	URN_RET_ON_NULL(topic, "Has topic but no str val", EINVAL);

	uintptr_t depth_pairid = 0;
	uintptr_t trade_pairid = 0;
	urn_hmap_getptr(depth_chn_to_pairid, topic, &depth_pairid);
	if (depth_pairid <= 0)
		urn_hmap_getptr(trade_chn_to_pairid, topic, &trade_pairid);
	if (depth_pairid > 0 || trade_pairid > 0) {
		URN_RET_ON_NULL(jdata = yyjson_obj_get(jroot, "data"), "No data", EINVAL);

		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "type"), "No type", EINVAL);
		const char *type = yyjson_get_str(jval);

		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "ts"), "No ts", EINVAL);
		long ts_e3 = yyjson_get_uint(jval);
		URN_RET_IF(ts_e3 < 1656853597000, "No valid data/t long val", EINVAL);

		if (depth_pairid > 0) {
			uintptr_t pairid = depth_pairid;
			char *depth_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk %3lu %s %ld %s", pairid, depth_pair, ts_e3, type);
			wss_mkt_ts = ts_e3 * 1000;
			odbk_t_arr[pairid] = ts_e3 * 1000;
			URN_GO_FINAL_ON_RV(on_odbk(pairid, type, jdata), "Err in on_odbk()")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
			goto final;
		} else if (trade_pairid > 0) {
			uintptr_t pairid = trade_pairid;
			char *trade_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk %3lu %s %ld %sf", pairid, trade_pair, ts_e3, type);
			URN_GO_FINAL_ON_RV(on_tick(pairid, jdata), "Err in on_tick()")
			URN_GO_FINAL_ON_RV(tick_updated(pairid, 0), "Err in tick_updated()")
			goto final;
		}
	}

	URN_WARNF("on_wss_msg unknown topic %s", msg);

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
		return URN_FATAL("Unexpected op_type, Bybit only has guess type", EINVAL);

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

int on_odbk(int pairid, const char *type, yyjson_val *jdata) {
	if (strcmp(type, "delta") == 0) {
		return on_odbk_update(pairid, type, jdata);
	} else if (strcmp(type, "snapshot") == 0) {
		// clear bids and asks for new snapshot.
		URN_DEBUGF("\tpurge odbk %d %s", pairid, type);
		mkt_wss_odbk_purge(pairid);
		return on_odbk_update(pairid, type, jdata);
	} else {
		URN_ERRF("\tUnexpected type %s", type);
		return EINVAL;
	}
}

int on_odbk_update(int pairid, const char *type, yyjson_val *jdata) {
	URN_DEBUGF("\ton_odbk %d %s", pairid, type);
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
	// [{"i":"2290000000087513415","T":1702104700664,"p":"44210.78","v":"0.005388","S":"Buy","s":"BTCUSDT","BT":false}]
	yyjson_arr_foreach(jdata, idx, max, v) {
		// "m":false
		yyjson_val *jval = NULL;
		const char *p_str=NULL, *s_str=NULL, *buy_str=NULL;
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "T"), "No data/T", EINVAL);
		unsigned long ts_e6 = yyjson_get_uint(jval) * 1000l;

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "p"), "No data/p", EINVAL);
		URN_RET_ON_NULL((p_str = yyjson_get_str(jval)), "No data/p str val", EINVAL);

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "v"), "No data/v", EINVAL);
		URN_RET_ON_NULL((s_str = yyjson_get_str(jval)), "No data/s str val", EINVAL);

		urn_inum p, s;
		urn_inum_parse(&p, p_str);
		urn_inum_parse(&s, s_str);

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "S"), "No data/S", EINVAL);
		URN_RET_ON_NULL((buy_str = yyjson_get_str(jval)), "No data/S str val", EINVAL);
		bool buy = (strcmp(buy_str, "Buy") == 0) ? true : false;

		urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);
	}
	return 0;
}
