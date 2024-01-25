// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

// local options
#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

#include "mkt_wss.h"

int   on_tick(int pairid, yyjson_val *jdata);
int   on_odbk(int pairid, const char *type, yyjson_val *jdata);
int   on_odbk_update(int pairid, const char *type, yyjson_val *jdata);

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+1);
	if ((*str) == NULL) return ENOMEM;
	if (pair->expiry != NULL)
		return URN_FATAL("Pair with expiry in Bitstamp", EINVAL);
	// USD-BTC   -> btcusd
	sprintf(*str, "%s%s", pair->asset, pair->currency);
	urn_s_downcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) {
	// not so active, wait longer.
	wss_stat_max_msg_t = 900;
	sprintf(s, "Bitstamp");
}

void mkt_data_set_wss_url(char *s) { sprintf(s, "wss://ws.bitstamp.net/"); }

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "diff_order_book_%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "order_book_%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "live_trades_%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	/*
	'event':'bts:subscribe',
	'data': { "channel": "order_book_btcusd" }
	*/
	char *pre = "{\"event\":\"bts:subscribe\",\"data\":{\"channel\":\"";
	char *end = "\"}}";
	int cmd_ct = 0;

	for (int i=0; i<chn_ct; i++) {
		char *cmd = malloc(strlen(pre) + strlen(end) + 64);
		if (cmd == NULL) return ENOMEM;
		strcpy(cmd, pre);
		char *pos = cmd + strlen(pre);
		strcpy(pos, odbk_snpsht_chns[i]);
		pos = pos + strlen(odbk_snpsht_chns[i]);
		strcpy(pos, end);

		wss_req_s[cmd_ct] = cmd;
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;

		cmd = malloc(strlen(pre) + strlen(end) + 64);
		if (cmd == NULL) return ENOMEM;
		strcpy(cmd, pre);
		pos = cmd + strlen(pre);
		strcpy(pos, odbk_chns[i]);
		pos = pos + strlen(odbk_chns[i]);
		strcpy(pos, end);

		wss_req_s[cmd_ct] = cmd;
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;

		cmd = malloc(strlen(pre) + strlen(end) + 64);
		if (cmd == NULL) return ENOMEM;
		strcpy(cmd, pre);
		pos = cmd + strlen(pre);
		strcpy(pos, tick_chns[i]);
		pos = pos + strlen(tick_chns[i]);
		strcpy(pos, end);

		wss_req_s[cmd_ct] = cmd;
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;
	}


	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	wss_req_interval_e = 1;
	wss_req_interval_ms = 50;
	return 0;
}

int bitstamp_wss_cancel_channel(const char *channel) {
	/*
	'event':'bts:unsubscribe',
	'data': { "channel": "order_book_btcusd" }
	*/
	char *pre = "{\"event\":\"bts:unsubscribe\",\"data\":{\"channel\":\"";
	char *end = "\"}}";

	char *cmd = malloc(strlen(pre) + strlen(end) + 64);
	if (cmd == NULL) return ENOMEM;
	strcpy(cmd, pre);
	char *pos = cmd + strlen(pre);
	strcpy(pos, channel);
	pos = pos + strlen(channel);
	strcpy(pos, end);

	// Find a empty slot.
	int cmd_ct = wss_req_i;
	bool slot_found = false;
	for (int i=0; i< MAX_WSS_REQ_BUFF; i++) {
		cmd_ct = wss_req_i + i;
		if (cmd_ct >= MAX_WSS_REQ_BUFF)
			cmd_ct -= MAX_WSS_REQ_BUFF;
		if (wss_req_s[cmd_ct] == NULL) {
			wss_req_s[cmd_ct] = cmd;
			slot_found = true;
			break;
		}
	}
	if (slot_found) {
		URN_INFOF("cancel channel req %d : %s", cmd_ct, cmd);
	} else {
		URN_WARNF("could not find slot for cancel channel req %s", channel);
	}
	return 0;
}

int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jdata = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	jval = yyjson_obj_get(jroot, "event");
	if (jval == NULL) {
		URN_WARNF("on_wss_msg unexpect no event\n%s", msg);
		goto final;
	}
	const char *event = yyjson_get_str(jval);
	URN_RET_ON_NULL(event, "Has event but no str val", EINVAL);

	if (strcmp(event, "bts:subscription_succeeded") == 0) {
		// URN_INFOF("on_wss_msg omit %s", msg);
		goto final;
	} else if (strcmp(event, "bts:unsubscription_succeeded") == 0) {
		// URN_INFOF("on_wss_msg omit %s", msg);
		goto final;
	}

	// Parse data and trade common attributes.
	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "channel"), "No channel", EINVAL);
	const char *channel = yyjson_get_str(jval);
	URN_RET_ON_NULL(channel, "No channel", EINVAL);

	URN_RET_ON_NULL(jdata = yyjson_obj_get(jroot, "data"), "No data", EINVAL);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "microtimestamp"), "No microtimestamp", EINVAL);
	const char *ts_e6 = yyjson_get_str(jval);
	URN_RET_ON_NULL(channel, "No microtimestamp str val", EINVAL);
	long ts_e6_l = strtol(ts_e6, NULL, 10);

	URN_DEBUGF("on_wss_msg event %s channel %s", event, channel);

	if (strcmp(event, "trade") == 0) {
		uintptr_t pairid = 0;
		urn_hmap_getptr(trade_chn_to_pairid, channel, &pairid);
		if (pairid == 0) {
			URN_WARNF("on_wss_msg unexpect channel %s\n%s", channel, msg);
			goto error;
		}
		URN_GO_FINAL_ON_RV(on_tick(pairid, jdata), "Err in tick handling")
		URN_GO_FINAL_ON_RV(tick_updated(pairid), "Err in tick_updated()")
		goto final;
	} else if (strcmp(event, "data") != 0) {
		URN_WARNF("on_wss_msg unexpect event %s\n%s", event, msg);
		goto error;
	}

	// event data only
	uintptr_t pairid = 0;
	bool is_snapshot = true;
	urn_hmap_getptr(depth_snpsht_chn_to_pairid, channel, &pairid);
	if (pairid == 0) {
		urn_hmap_getptr(depth_chn_to_pairid, channel, &pairid);
		is_snapshot = false;
	}
	if (pairid == 0) {
		URN_WARNF("on_wss_msg unexpect channel %s\n%s", channel, msg);
		goto error;
	}
	// snapshot/delta processing here, almost same logic.
	char *depth_pair = pair_arr[pairid];
	URN_DEBUGF("\t -> odbk %s %3lu %s", (is_snapshot ? "full":"delta"), pairid, depth_pair);
	odbk_t_arr[pairid] = ts_e6_l;
	wss_mkt_ts = ts_e6_l;
	if (is_snapshot) {
		URN_GO_FINAL_ON_RV(on_odbk(pairid, NULL, jdata), "Err in odbk handling")
		bitstamp_wss_cancel_channel(channel);
	} else {
		URN_GO_FINAL_ON_RV(on_odbk_update(pairid, NULL, jdata), "Err in odbk handling")
	}
	URN_DEBUGF("\t -> newodbk_arr %d %s", newodbk_arr[pairid], depth_pair);
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
		return URN_FATAL("Unexpected op_type, Bitstamp only has guess type", EINVAL);

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
	// clear bids and asks for new snapshot.
	URN_DEBUGF("\tpurge odbk %d %s", pairid, type);
	mkt_wss_odbk_purge(pairid);
	return on_odbk_update(pairid, type, jdata);
}

int on_odbk_update(int pairid, const char *type, yyjson_val *jdata) {
	URN_DEBUGF("\ton_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *bids, *asks;
	URN_RET_ON_NULL(bids = yyjson_obj_get(jdata, "bids"), "No data/bids", EINVAL);
	URN_RET_ON_NULL(asks = yyjson_obj_get(jdata, "asks"), "No data/asks", EINVAL);

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *orders = NULL;
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
	/*
	 * "timestamp":"1657253001",
	 * "amount":0.00057871,
	 * "amount_str":"0.00057871",
	 * "price":22100.01,
	 * "price_str":"22100.01",
	 * "type":1,
	 * "microtimestamp":"1657253001408000"
	 */
	URN_DEBUGF("\ton_tick %d", pairid);
	char* pair = pair_arr[pairid];
	int rv = 0;
	const char *pstr=NULL, *sstr=NULL, *timestr;
	int side = -1;
	yyjson_val *jval = NULL;

	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "price_str"), "No price_str", EINVAL);
	URN_RET_ON_NULL(pstr = yyjson_get_str(jval), "No price_str str", EINVAL);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "amount_str"), "No amount_str", EINVAL);
	URN_RET_ON_NULL(sstr = yyjson_get_str(jval), "No amount_str str", EINVAL);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "type"), "No type", EINVAL);
	side = yyjson_get_int(jval);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "microtimestamp"), "No microtimestamp", EINVAL);
	URN_RET_ON_NULL(timestr = yyjson_get_str(jval), "No microtimestamp str", EINVAL);
	long ts_e6 = strtol(timestr, NULL, 10);
	URN_RET_IF(ts_e6 <= 0, "No microtimestamp long", EINVAL);

	urn_inum p, s;
	urn_inum_parse(&p, pstr);
	urn_inum_parse(&s, sstr);
	bool buy;
	if (side == 0)
		buy = true;
	else if (side == 1)
		buy = false;
	else
		URN_RET_ON_NULL(NULL, "Unexpected side str", EINVAL);

	urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);

	return 0;
}
