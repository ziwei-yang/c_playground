// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

// local options
#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

//#define PUB_NO_REDIS

#include "mkt_wss.h"

int   on_tick(int pairid, long ts_e6, yyjson_val *jdata);
int   on_odbk(int pairid, long ts_e6, yyjson_val *jdata);
int   on_odbk_update(int pairid, long ts_e6, yyjson_val *jdata);

char *preprocess_pair(char *pair) {
	if (pair == NULL) URN_FATAL("pair is NULL", EINVAL);
	return pair;
}

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+1);
	if ((*str) == NULL) return ENOMEM;
	if (pair->expiry != NULL)
		return URN_FATAL("Pair with expiry in coinbase", EINVAL);
	// USDT-BTC   -> BTC-USDT
	sprintf(*str, "%s-%s", pair->asset, pair->currency);
	urn_s_upcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) {
	sprintf(s, "Coinbase");
}

void mkt_data_set_wss_url(char *s) {
	sprintf(s, "wss://advanced-trade-ws.coinbase.com/");
}

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	char *spider_key = getenv("spiderkey");
	if (spider_key == NULL) URN_FATAL("ENV[spiderkey] is NULL", EINVAL);

	yyjson_doc *jdoc = yyjson_read(spider_key, strlen(spider_key), 0);
	yyjson_val *jroot = yyjson_doc_get_root(jdoc);
	yyjson_val *lv2_req = yyjson_obj_get(jroot, "level2_req");
	yyjson_val *tk_req = yyjson_obj_get(jroot, "ticker_req");

	if (lv2_req == NULL && tk_req == NULL)
		URN_FATAL("ENV[spiderkey] has no level2_req or ticker_req", EINVAL);

	wss_req_s[0] = yyjson_val_write(lv2_req, 0, NULL); // one-line json
	URN_LOG(wss_req_s[0]);
	wss_req_s[1] = yyjson_val_write(tk_req, 0, NULL); // one-line json
	URN_LOG(wss_req_s[1]);

	wss_req_interval_e = 1;
	wss_req_interval_ms = 100;

	URN_INFOF("Parsing ARGV end, %d req str prepared.", 2);
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return 0;
}

struct tm _coinbase_tm;
// 2022-06-18T09:14:56.470799Z
#define parse_coinbase_time_to_e6(s) \
	parse_timestr_w_e6(&_coinbase_tm, (s), "%Y-%m-%dT%H:%M:%S.");

int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jevents = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	URN_GO_FINAL_ON_NULL(jval = yyjson_obj_get(jroot, "channel"), "No channel");
	const char *channel = yyjson_get_str(jval);
	URN_GO_FINAL_ON_NULL(channel, "No channel");

	long ts_e6 = 0;
	URN_GO_FINAL_ON_NULL(jval = yyjson_obj_get(jroot, "timestamp"), "No timestamp");
	const char *timestr = yyjson_get_str(jval);
	URN_GO_FINAL_ON_NULL(timestr, "No timestamp");
	ts_e6 = parse_coinbase_time_to_e6(timestr);
	URN_DEBUGF("on_wss_msg %s %ld %s", timestr, ts_e6, channel);
	if (ts_e6 <= 0) {
		URN_WARNF("parse_coinbase_time_to_e6 failed %s", timestr);
		rv = EINVAL;
		goto final;
	}
	wss_mkt_ts = ts_e6;

	URN_GO_FINAL_ON_NULL(jevents = yyjson_obj_get(jroot, "events"), "No events");

	if (strcmp(channel, "subscriptions") == 0) {
		URN_LOGF("on_wss_msg %s", msg);
		goto final;
	} else if (strcmp(channel, "l2_data") == 0) {
		yyjson_val *v_el;
		size_t idx, max;
		yyjson_arr_foreach(jevents, idx, max, v_el) {
			URN_GO_FINAL_ON_NULL(jval = yyjson_obj_get(v_el, "product_id"), "No events[]/product_id");
			const char *product_id = yyjson_get_str(jval);
			uintptr_t pairid = 0;
			urn_hmap_getptr(depth_snpsht_chn_to_pairid, product_id, &pairid);
			if (pairid == 0) {
				URN_WARNF("on_wss_msg UNKNOWN events[]/product_id %s", msg);
				goto final;
			}
			char *depth_pair = pair_arr[pairid];

			URN_GO_FINAL_ON_NULL(jval = yyjson_obj_get(v_el, "type"), "No events[]/type");
			const char *type = yyjson_get_str(jval);

			URN_GO_FINAL_ON_NULL(jval = yyjson_obj_get(v_el, "updates"), "No events[]/updates[]");
			if (strcmp(type, "snapshot") == 0) {
				URN_DEBUGF("\t -> odbk pair snapshot %lu %s", pairid, depth_pair);
				URN_GO_FINAL_ON_RV(on_odbk(pairid, ts_e6, jval), "Err in odbk handling");
				URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()");
			} else if (strcmp(type, "update") == 0) {
				URN_DEBUGF("\t -> odbk pair update   %lu %s", pairid, depth_pair);
				URN_GO_FINAL_ON_RV(on_odbk_update(pairid, ts_e6, jval), "Err in odbk handling");
				URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()");
			} else {
				URN_WARNF("on_wss_msg UNKNOWN events[]/type %s", msg);
			}
		}
		goto final;
	} else if (strcmp(channel, "ticker") == 0) {
		yyjson_val *v_el;
		size_t idx, max;
		yyjson_arr_foreach(jevents, idx, max, v_el) {
			// {"type":"snapshot","tickers":[]
			yyjson_val *v_tk;
			size_t tk_idx, tk_max;
			URN_GO_FINAL_ON_NULL(v_el = yyjson_obj_get(v_el, "tickers"), "No events[]/tickers[]");
			yyjson_arr_foreach(v_el, tk_idx, tk_max, v_tk) {
				// {"type":"ticker","product_id":"DOGE-USDT"
				URN_GO_FINAL_ON_NULL(jval = yyjson_obj_get(v_tk, "product_id"), "No events[]/tickers[]/product_id");
				const char *product_id = yyjson_get_str(jval);
				uintptr_t pairid = 0;
				urn_hmap_getptr(depth_snpsht_chn_to_pairid, product_id, &pairid);
				if (pairid == 0) {
					URN_WARNF("on_wss_msg UNKNOWN events[]/tickers[]/product_id %s", msg);
					goto final;
				}
				char *trade_pair = pair_arr[pairid];
				URN_DEBUGF("\t -> odbk pair ticker   %lu %s %ld", pairid, trade_pair, ts_e6);
				URN_GO_FINAL_ON_RV(on_tick(pairid, ts_e6, v_tk), "Err in tick_updated()");
				URN_GO_FINAL_ON_RV(tick_updated(pairid), "Err in tick_updated()");
			}
		}
		goto final;
	} else {
		URN_WARNF("on_wss_msg UNKNOWN channel %s", msg);
		goto final;
	}

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
int parse_n_mod_odbk_porder(int pairid, yyjson_val *v, int op_type) {
	urn_inum *p=NULL, *s=NULL; // must free by hand if insertion failed.
	bool need_free_ps = true; // free when delete, or update failed.
	char prices[32], sizes[32];
	yyjson_val *jval;
	const char *side = NULL;
	bool buy;
	int rv = 0;

	URN_RET_ON_NULL(jval = yyjson_obj_get(v, "side"), "No events[]/updates[]/side", EINVAL);
	URN_RET_ON_NULL(side = yyjson_get_str(jval), "No events[]/updates[]/side", EINVAL);
	if (strcmp(side, "bid") == 0)
		buy = true;
	else if (strcmp(side, "offer") == 0)
		buy = false;
	else
		return URN_FATAL("Unexpected side type", EINVAL);

	if (op_type != 4)
		return URN_FATAL("Unexpected op_type, Coinbase only has guess type", EINVAL);

	URN_RET_ON_NULL(jval = yyjson_obj_get(v, "price_level"), "No events[]/updates[]/price_level", EINVAL);
	urn_inum_alloc(&p, yyjson_get_str(jval));
	URN_RET_ON_NULL(jval = yyjson_obj_get(v, "new_quantity"), "No events[]/updates[]/new_quantity", EINVAL);
	urn_inum_alloc(&s, yyjson_get_str(jval));

	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];

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

int on_odbk(int pairid, long ts_e6, yyjson_val *updates) {
	// clear bids and asks for new snapshot.
	mkt_wss_odbk_purge(pairid);
	return on_odbk_update(pairid, ts_e6, updates);
}

int on_odbk_update(int pairid, long ts_e6, yyjson_val *updates) {
	URN_DEBUGF("\ton_odbk %d %ld", pairid, ts_e6);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_arr_foreach(updates, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, v, 4), "Error in parse_n_mod_odbk_porder()");
	}
	return 0;
}

int on_tick(int pairid, long ts_e6, yyjson_val *jdata) {
	URN_DEBUGF("\ton_tick %d %s", pairid, yyjson_val_write(jdata, 0, NULL));
	char* pair = pair_arr[pairid];
	int rv = 0;
	const char *pstr=NULL, *sstr=NULL, *side=NULL;
	yyjson_val *jval = NULL;

	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "price"), "No price", EINVAL);
	URN_RET_ON_NULL(pstr = yyjson_get_str(jval), "No price str", EINVAL);

	urn_inum p, s;
	urn_inum_parse(&p, pstr);
	urn_inum_parse(&s, "0.001"); // No size given.

	// Guess direction by comparing ticker to top prices.
	GList *bids = bids_arr[pairid];
	while(bids != NULL && bids->next != NULL)
		bids = bids->next;
	GList *asks = asks_arr[pairid];
	while(asks != NULL && asks->next != NULL)
		asks = asks->next;
	double bid_p = -1, ask_p = -1;
	if (bids != NULL)
		bid_p = urn_inum_to_db(((urn_porder *)(bids->data))->p);
	if (asks != NULL)
		ask_p = urn_inum_to_db(((urn_porder *)(asks->data))->p);
	double tk_p = urn_inum_to_db(&p);
	bool buy = (bid_p > 0 && ask_p > 0 && (ask_p - tk_p < tk_p - bid_p)) ? true : false;

	urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);
	return 0;
}

