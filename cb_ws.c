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
int   on_odbk(int pairid, const char *type, yyjson_val *jdata);
int   on_odbk_update(int pairid, const char *type, yyjson_val *jdata);

char *preprocess_pair(char *pair) {
	// Dirty tricks to broadcast market data.
	if (pair == NULL) URN_FATAL("pair is NULL", EINVAL);
	if ((strcmp(pair, "USDC-CVC") == 0) || (strcmp(pair, "USDC-DNT") == 0)) {
		// USDC-CVC data broadcast to -> USDT-CVC
		pair[3] = 'T';
	} else if (strncmp(pair, "USDC-", 4) == 0) {
		int slen = strlen(pair);
		// USDC-ETH data broadcast to -> USD-ETH
		// move 1 forward at pos 3.
		for (int i=3; i<slen; i++)
			pair[i] = pair[i+1];
		return pair;
	}
	return pair;
}

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+1);
	if ((*str) == NULL) return ENOMEM;
	if (pair->expiry != NULL)
		return URN_FATAL("Pair with expiry in coinbase", EINVAL);
	if (strcmp(pair->asset, "DNT") == 0 && strcmp(pair->currency, "USDT") == 0) {
		// see preprocess_pair()
		strcpy(*str, "DNT-USDC");
	} else if (strcmp(pair->asset, "CVC") == 0 && strcmp(pair->currency, "USDT") == 0) {
		// see preprocess_pair()
		strcpy(*str, "CVC-USDC");
	} else if (strcmp(pair->asset, "USD") == 0) {
		// see preprocess_pair()
		sprintf(*str, "%s-USDC", pair->currency);
	} else { // USDT-BTC   -> BTC-USDT
		sprintf(*str, "%s-%s", pair->asset, pair->currency);
	}
	urn_s_upcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) {
	sprintf(s, "Coinbase");
}

void mkt_data_set_wss_url(char *s) {
	sprintf(s, "wss://ws-feed.pro.coinbase.com/");
}

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	/*
	"type": "subscribe",
	"product_ids": [ "...", "...", ... ],
	"channels": [
		"level2",
		"ticker"
	]
	*/

	int batch_sz = max_pair_id;
	int batch = 0;
	int cmd_ct = 0;
	for (; batch <= (chn_ct/batch_sz+1); batch++) {
		int i_s = batch*batch_sz;
		int i_e = MIN((batch+1)*batch_sz, chn_ct);
		if (i_s >= i_e) {
			batch--; break;
		}

		yyjson_mut_doc *wss_req_j = yyjson_mut_doc_new(NULL);
		yyjson_mut_val *jroot = yyjson_mut_obj(wss_req_j);
		if (jroot == NULL)
			return URN_FATAL("Error in creating json root", EINVAL);
		yyjson_mut_doc_set_root(wss_req_j, jroot);
		yyjson_mut_obj_add_str(wss_req_j, jroot, "type", "subscribe");
		URN_LOGF("preparing wss req batch %d:", batch);

		const char *batch_chn[(i_e-i_s)];
		for (int i=i_s; i<i_e; i++) {
			URN_DEBUGF("\tchn %d %s %s %s", i, odbk_chns[i], odbk_snpsht_chns[i], tick_chns[i]);
			// coinbase only needs symbol
			batch_chn[(i-i_s)] = odbk_chns[i];
		}

		yyjson_mut_val *target_chns = yyjson_mut_arr_with_strcpy(
			wss_req_j, batch_chn, (i_e-i_s));
		if (target_chns == NULL)
			return URN_FATAL("Error in strcpy json array", EINVAL);
		yyjson_mut_obj_add_val(wss_req_j, jroot, "product_ids", target_chns);

		const char *channel_types[2] = { "level2", "ticker"};
		yyjson_mut_val *target_chn_types = yyjson_mut_arr_with_strcpy(
			wss_req_j, channel_types, 2);
		if (target_chn_types == NULL)
			return URN_FATAL("Error in strcpy json array", EINVAL);
		yyjson_mut_obj_add_val(wss_req_j, jroot, "channels", target_chn_types);

		// save command
		char *cmd = yyjson_mut_write(wss_req_j, 0, NULL); // one-line json
		wss_req_s[cmd_ct] = cmd;
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;
		yyjson_mut_doc_free(wss_req_j);
	}
	wss_req_s[cmd_ct] = NULL;

	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	wss_req_interval_e = 1;
	wss_req_interval_ms = 100;
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

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "type"), "No type", EINVAL);
	const char *type = yyjson_get_str(jval);
	URN_RET_ON_NULL(type, "No type", EINVAL);

	const char *timestr = NULL;
	long ts_e6 = 0;
	jval = yyjson_obj_get(jroot, "time");
	if (jval != NULL) {
		timestr = yyjson_get_str(jval);
		ts_e6 = parse_coinbase_time_to_e6(timestr);
		URN_DEBUGF("on_wss_msg %s %ld %s", timestr, ts_e6, type);
		if (ts_e6 <= 0) {
			URN_WARNF("parse_coinbase_time_to_e6 failed %s", timestr);
			rv = EINVAL;
			goto final;
		}
		wss_mkt_ts = ts_e6;
	}

	const char *product_id = NULL;
	jval = yyjson_obj_get(jroot, "product_id");
	if (jval != NULL)
		product_id = yyjson_get_str(jval);

	if (strcmp(type, "snapshot") == 0) {
		URN_RET_ON_NULL(product_id, "No product_id", EINVAL);

		uintptr_t pairid = 0;
		urn_hmap_getptr(depth_snpsht_chn_to_pairid, product_id, &pairid);
		if (pairid != 0) {
			// No E in snapshot data.
			char *depth_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk pair snapshot %lu %s", pairid, depth_pair);
			URN_GO_FINAL_ON_RV(on_odbk(pairid, NULL, jroot), "Err in odbk handling")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
			goto final;
		}
	} else if (strcmp(type, "l2update") == 0) {
		URN_RET_ON_NULL(product_id, "No product_id", EINVAL);
		URN_RET_ON_NULL(timestr, "No time", EINVAL);

		uintptr_t pairid = 0;
		urn_hmap_getptr(depth_chn_to_pairid, product_id, &pairid);
		if (pairid != 0) {
			odbk_t_arr[pairid] = ts_e6;

			char *depth_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk pair delta    %lu %s %ld", pairid, depth_pair, ts_e6);
			URN_GO_FINAL_ON_RV(on_odbk_update(pairid, NULL, jroot), "Err in odbk handling")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
			goto final;
		}
	} else if (strcmp(type, "ticker") == 0) {
		URN_RET_ON_NULL(product_id, "No product_id", EINVAL);
		URN_RET_ON_NULL(timestr, "No time", EINVAL);

		uintptr_t pairid = 0;
		urn_hmap_getptr(trade_chn_to_pairid, product_id, &pairid);
		if (pairid != 0) {
			char *trade_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk pair ticker   %lu %s", pairid, trade_pair, ts_e6);
			URN_GO_FINAL_ON_RV(on_tick(pairid, ts_e6, jroot), "Err in tick_updated()")
			URN_GO_FINAL_ON_RV(tick_updated(pairid), "Err in tick_updated()")
			goto final;
		}
	} else if (strcmp(type, "subscriptions") == 0) {
		URN_INFOF("on_wss_msg omit %.*s", (int)len, msg);
		goto final;
	} else if (strcmp(type, "error") == 0) {
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "message"), "No error message", EINVAL);
		const char *message= yyjson_get_str(jval);
		if (strcmp(message, "Failed to subscribe") == 0) {
			URN_WARNF("on_wss_msg omit %.*s", (int)len, msg);
			goto final;
		}
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
int parse_n_mod_odbk_porder(int pairid, const char *type, yyjson_val *v, int op_type) {
	urn_inum *p=NULL, *s=NULL; // must free by hand if insertion failed.
	bool need_free_ps = true; // free when delete, or update failed.

	char prices[32], sizes[32];

	bool side_price_size = false;
	bool buy;
	int rv = 0;

	if (type == NULL)
		side_price_size = true; // v=["buy"/"sell", "price", "size"]
	else if ((*type) == 'B')
		buy = true;
	else if ((*type) == 'A')
		buy = false;
	else
		return URN_FATAL("Unexpected order type", EINVAL);

	if (op_type != 4)
		return URN_FATAL("Unexpected op_type, Coinbase only has guess type", EINVAL);

	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];

	// json in v: ['0.22775','9899.0'] : [price, size]
	// when side_price_size = true; // v=["buy"/"sell", "price", "size"]
	yyjson_val *v_el;
	int el_ct = 0;
	size_t idx, max;
	const char *side;
	yyjson_arr_foreach(v, idx, max, v_el) {
		if (side_price_size) {
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
		} else {
			if (el_ct == 0) { // price
				urn_inum_alloc(&p, yyjson_get_str(v_el));
			} else if (el_ct == 1) { // size
				urn_inum_alloc(&s, yyjson_get_str(v_el));
			} else
				return URN_FATAL("Unexpected attr ct in order, should be 2", EINVAL);
		}
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
	mkt_wss_odbk_purge(pairid);

	URN_DEBUGF("\ton_odbk %d %s", pairid, type);
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

int on_odbk_update(int pairid, const char *type, yyjson_val *jdata) {
	URN_DEBUGF("\ton_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *orders = NULL;

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "changes"), "No changes", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, NULL, v, 4), "Error in parse_n_mod_odbk_porder() for bids");
	}
	return 0;
}

int on_tick(int pairid, long ts_e6, yyjson_val *jdata) {
	URN_DEBUGF("\ton_tick %d", pairid);
	char* pair = pair_arr[pairid];
	int rv = 0;
	const char *pstr=NULL, *sstr=NULL, *side=NULL;
	yyjson_val *jval = NULL;

	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "price"), "No price", EINVAL);
	URN_RET_ON_NULL(pstr = yyjson_get_str(jval), "No price str", EINVAL);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jdata, "last_size"), "No last_size", EINVAL);
	URN_RET_ON_NULL(sstr = yyjson_get_str(jval), "No last_size str", EINVAL);
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

	urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);

	return 0;
}
