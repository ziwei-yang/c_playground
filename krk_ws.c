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
int   on_odbk(int pairid, const char *type, yyjson_val *asks, yyjson_val *bids);
int   on_odbk_update(int pairid, const char *type, yyjson_val *asks, yyjson_val *bids);

// Usually kraken channel ID < 9999, seen 211288064 from 20250322
#define krk_max_chnid 230000000 // use hmap
// store given channel ID -> pair
int krk_odbk_pairs[krk_max_chnid];
int krk_tick_pairs[krk_max_chnid]; // store given channel ID -> pairid
const char *krk_symbol_arr[MAX_PAIRS];

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen);
	if ((*str) == NULL) return ENOMEM;
	if (pair->expiry != NULL)
		return URN_FATAL("Pair with expiry in coinbase", EINVAL);
	if ((strcmp(pair->currency, "BTC") == 0) && (strcmp(pair->asset, "DOGE") == 0)) {
		sprintf(*str, "XDG/XBT");
	} else if (strcmp(pair->asset, "BTC") == 0) {
		sprintf(*str, "XBT/%s", pair->currency);
	} else if (strcmp(pair->currency, "BTC") == 0) {
		sprintf(*str, "%s/XBT", pair->asset);
	} else if (strcmp(pair->asset, "DOGE") == 0) {
		sprintf(*str, "XDG/%s", pair->currency);
	} else { // USDT-ETH   -> ETH/USDT
		sprintf(*str, "%s/%s", pair->asset, pair->currency);
	}
	urn_s_upcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) {
	// not a active exchange, wait longer.
	wss_stat_max_msg_t = -1;
	wss_stat_warn_msg_t = 600;
	sprintf(s, "Kraken");
	for (int i=0; i <= krk_max_chnid; i++) {
		krk_odbk_pairs[i] = 0;
		krk_tick_pairs[i] = 0;
	}
	for (int i=0; i < MAX_PAIRS; i++)
		krk_symbol_arr[i] = NULL;
}

void mkt_data_set_wss_url(char *s) {
	sprintf(s, "wss://ws.kraken.com/");
}

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	/*
	"event": "subscribe",
	"pair": [symbol ... ], // no more than 3
	"subscription": { "name": "book"}
	OR
	"subscription": { "name": "trade"}
	*/

	// Kraken WSS has lantency problem when listening more than 4 pairs in one WSS.
	int batch_sz = 3;
	int batch = 0;
	int cmd_ct = 0;
	for (; batch <= (chn_ct/batch_sz+1); batch++) {
		int i_s = batch*batch_sz;
		int i_e = MIN((batch+1)*batch_sz, chn_ct);
		if (i_s >= i_e) {
			batch--; break;
		}
		char *cmd;
		const char *batch_chn[(i_e-i_s)];

		////////////// BOOK ///////////////

		yyjson_mut_doc *wss_req_j = yyjson_mut_doc_new(NULL);
		yyjson_mut_val *jroot = yyjson_mut_obj(wss_req_j);
		if (jroot == NULL)
			return URN_FATAL("Error in creating json root", EINVAL);
		yyjson_mut_doc_set_root(wss_req_j, jroot);
		yyjson_mut_obj_add_str(wss_req_j, jroot, "event", "subscribe");
		URN_LOGF("preparing wss req batch %d:", batch);

		// "pair": [symbol ... ], // no more than 3
		for (int i=i_s; i<i_e; i++) {
			URN_DEBUGF("\tchn %d %s %s %s", i, odbk_chns[i], odbk_snpsht_chns[i], tick_chns[i]);
			batch_chn[(i-i_s)] = odbk_chns[i];
			krk_symbol_arr[i+1] = odbk_chns[i];
		}

		yyjson_mut_val *target_chns = yyjson_mut_arr_with_strcpy(
			wss_req_j, batch_chn, (i_e-i_s));
		if (target_chns == NULL)
			return URN_FATAL("Error in strcpy json array", EINVAL);
		yyjson_mut_obj_add_val(wss_req_j, jroot, "pair", target_chns);

		// "subscription": { "name": "book"}
		yyjson_mut_val *jname = yyjson_mut_obj(wss_req_j);
		yyjson_mut_obj_add_str(wss_req_j, jname, "name", "book");
		yyjson_mut_obj_add_val(wss_req_j, jroot, "subscription", jname);

		// save command
		cmd = yyjson_mut_write(wss_req_j, 0, NULL); // one-line json
		wss_req_s[cmd_ct] = cmd;
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;
		yyjson_mut_doc_free(wss_req_j);

		////////////// TRADE ///////////////

		wss_req_j = yyjson_mut_doc_new(NULL);
		jroot = yyjson_mut_obj(wss_req_j);
		if (jroot == NULL)
			return URN_FATAL("Error in creating json root", EINVAL);
		yyjson_mut_doc_set_root(wss_req_j, jroot);
		yyjson_mut_obj_add_str(wss_req_j, jroot, "event", "subscribe");
		URN_LOGF("preparing wss req batch %d:", batch);

		// "pair": [symbol ... ], // no more than 3
		for (int i=i_s; i<i_e; i++) {
			URN_DEBUGF("\tchn %d %s %s %s", i, odbk_chns[i], odbk_snpsht_chns[i], tick_chns[i]);
			batch_chn[(i-i_s)] = odbk_chns[i];
			krk_symbol_arr[i+1] = odbk_chns[i];
		}

		target_chns = yyjson_mut_arr_with_strcpy(
			wss_req_j, batch_chn, (i_e-i_s));
		if (target_chns == NULL)
			return URN_FATAL("Error in strcpy json array", EINVAL);
		yyjson_mut_obj_add_val(wss_req_j, jroot, "pair", target_chns);

		// "subscription": { "name": "trade"}
		jname = yyjson_mut_obj(wss_req_j);
		yyjson_mut_obj_add_str(wss_req_j, jname, "name", "trade");
		yyjson_mut_obj_add_val(wss_req_j, jroot, "subscription", jname);

		// save command
		cmd = yyjson_mut_write(wss_req_j, 0, NULL); // one-line json
		wss_req_s[cmd_ct] = cmd;
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;
		yyjson_mut_doc_free(wss_req_j);
	}
	wss_req_s[cmd_ct] = NULL;

	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	wss_req_interval_e = 3;
	wss_req_interval_ms = 500;
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

	if (yyjson_is_obj(jroot)) {
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "event"), "No event", EINVAL);
		const char *event = yyjson_get_str(jval);
		URN_RET_ON_NULL(event, "No event", EINVAL);

		if (strcmp(event, "heartbeat") == 0) {
			URN_DEBUGF("on_wss_msg omit %.*s", (int)len, msg);
			goto final;
		} else if (strcmp(event, "systemStatus") == 0) {
			URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "status"), "systemStatus no status", EINVAL);
			const char *status = yyjson_get_str(jval);
			if (strcmp(status, "online") == 0) {
				URN_DEBUGF("on_wss_msg omit %.*s", (int)len, msg);
				goto final;
			} else {
				// Unknown status
				URN_GO_FINAL_ON_RV(EINVAL, msg);
			}
		} else if (strcmp(event, "subscriptionStatus") == 0) {
			/*
			   "channelID":1536,
			   "channelName":"book-10",
			   "event":"subscriptionStatus",
			   "pair":"ATOM/XBT","status":"subscribed",
			   "subscription":{"depth":10,"name":"book"}
			   */
			URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "status"), "subscriptionStatus no status", EINVAL);
			const char *status = yyjson_get_str(jval);
			// {"errorMessage":"Currency pair not supported LSK/XBT","event":"subscriptionStatus","pair":"LSK/XBT","status":"error","subscription":{"name":"book"}}
			if (strcmp(status, "error") == 0) {
				URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "errorMessage"), "subscriptionStatus error but no message", EINVAL);
				const char *error_msg = yyjson_get_str(jval);
				URN_WARNF("%s", error_msg);
				if (strstr(error_msg, "Currency pair not supported") != NULL)
					goto final;
				URN_GO_FINAL_ON_RV(EINVAL, msg);
			} else if (strcmp(status, "subscribed") != 0) {
				URN_GO_FINAL_ON_RV(EINVAL, msg);
			}
			URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "channelID"), "subscriptionStatus no channelID", EINVAL);
			int chn_id = yyjson_get_int(jval);
			if (chn_id <= 0 || chn_id >= krk_max_chnid) { // out of predefined range
				URN_WARNF("Out of predefined range %d %d", chn_id, krk_max_chnid);
				URN_GO_FINAL_ON_RV(ERANGE, msg);
			}
			URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "channelName"), "subscriptionStatus no channelName", EINVAL);
			const char *chn_name = yyjson_get_str(jval);
			URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "pair"), "subscriptionStatus no pair", EINVAL);
			const char *chn_symbol = yyjson_get_str(jval);
			uintptr_t pairid = 0;
			if (strcmp(chn_name, "book-10") == 0) {
				urn_hmap_getptr(depth_chn_to_pairid, chn_symbol, &pairid);
				krk_odbk_pairs[chn_id] = (int)pairid;
			} else if (strcmp(chn_name, "trade") == 0) {
				urn_hmap_getptr(trade_chn_to_pairid, chn_symbol, &pairid);
				krk_tick_pairs[chn_id] = (int)pairid;
			}
			chn_symbol = yyjson_get_str(jval);
			if (pairid <= 0) {
				URN_WARNF("Could not locate pairid for %s", chn_symbol);
				URN_GO_FINAL_ON_RV(ERANGE, msg);
			}
			URN_INFOF("channel %12s krk_id %6d - %12s - pairid %lu %s",
				chn_name, chn_id, chn_symbol, pairid, pair_arr[pairid]);
			goto final;
		}
	} else if (yyjson_is_arr(jroot)) {
		// [channelID, {} .. {}, type, symbol]
		int jroot_size = yyjson_arr_size(jroot);
		if (jroot_size < 4) {
			URN_WARNF("msg expect: [channelID, {}, type, symbol], got: %d", jroot_size);
			URN_GO_FINAL_ON_RV(EINVAL, msg);
		}
		// get channel id
		yyjson_val *chn_id_val = yyjson_arr_get(jroot, 0);
		int chn_id = yyjson_get_int(chn_id_val);
		if (chn_id < 0 || chn_id > krk_max_chnid) { // out of predefined range
			URN_WARNF("Out of predefined range %d %d", chn_id, krk_max_chnid);
			URN_GO_FINAL_ON_RV(ERANGE, msg);
		}
		// get channel name (type)
		yyjson_val *chn_name_val = yyjson_arr_get(jroot, jroot_size-2);
		const char* chn_name = yyjson_get_str(chn_name_val);
		int pairid = 0;
		bool is_trade = false;
		if (strcmp(chn_name, "book-10") == 0) {
			pairid = krk_odbk_pairs[chn_id];
		} else if (strcmp(chn_name, "trade") == 0) {
			is_trade = true;
			pairid = krk_tick_pairs[chn_id];
		} else {
			URN_WARNF("unexpect msg name: %s", chn_name);
			URN_GO_FINAL_ON_RV(EINVAL, msg);
		}
		// verify channel symbol
		yyjson_val *chn_symb_val = yyjson_arr_get(jroot, jroot_size-1);
		const char* chn_symb = yyjson_get_str(chn_symb_val);
		char *pair = pair_arr[pairid];
		const char *symbol = krk_symbol_arr[pairid];
		if (symbol == NULL) {
			URN_WARNF("chn_id %d of %s symbol %s -> pairid %d %s -> NULL",
				chn_id, chn_name, chn_symb, pairid, pair);
			URN_GO_FINAL_ON_RV(ERANGE, msg);
		} else if (strcmp(symbol, chn_symb) != 0) {
			URN_WARNF("chn_id %d of %s symbol %s -> pairid %d %s -> %s XXX",
					chn_id, chn_name, chn_symb, pairid, pair, symbol);
			URN_GO_FINAL_ON_RV(EINVAL, msg);
		}
		// symbol verified
		URN_DEBUGF("chn_id %d of %s symbol %s -> pairid %d %s -> %s",
			chn_id, chn_name, chn_symb, pairid, pair, symbol);

		if (is_trade) {
			// [chn_id, [[trade]...], "trade", "symbol"]
			yyjson_val *jdata = yyjson_arr_get(jroot, 1);
			if (jdata == NULL) {
				URN_WARNF("on_wss_msg %.*s no [1]", (int)len, msg);
				URN_GO_FINAL_ON_RV(EINVAL, msg);
			}
			URN_GO_FINAL_ON_RV(on_tick(pairid, jdata), "Err in tick handling")
			URN_GO_FINAL_ON_RV(tick_updated(pairid), "Err in tick_updated()")
			goto final;
		}

		// parse jroot[1..-3] data
		// Might be [ {'as','bs'} ]
		// Might be [ {'a'}, AND/OR {'b'} ]
		// Just prepare all slots and fill in.
		yyjson_val *as=NULL, *bs=NULL, *a=NULL, *b=NULL;
		for (int i=1; i<jroot_size-2; i++) {
			yyjson_val *data = yyjson_arr_get(jroot, i);
			if (as == NULL)
				as = yyjson_obj_get(data, "as");
			else if (yyjson_obj_get(data, "as") != NULL)
				URN_GO_FINAL_ON_RV(EINVAL, "as duplicated in array msg");
			if (bs == NULL)
				bs = yyjson_obj_get(data, "bs");
			else if (yyjson_obj_get(data, "bs") != NULL)
				URN_GO_FINAL_ON_RV(EINVAL, "bs duplicated in array msg");
			if (a == NULL)
				a = yyjson_obj_get(data, "a");
			else if (yyjson_obj_get(data, "a") != NULL)
				URN_GO_FINAL_ON_RV(EINVAL, "a duplicated in array msg");
			if (b == NULL)
				b = yyjson_obj_get(data, "b");
			else if (yyjson_obj_get(data, "b") != NULL)
				URN_GO_FINAL_ON_RV(EINVAL, "b duplicated in array msg");
		}
		bool processed = false;
		if ((as != NULL) || (bs != NULL)) {
			URN_DEBUGF("\t -> odbk pair snapshot %d %s", pairid, pair);
			URN_GO_FINAL_ON_RV(on_odbk(pairid, NULL, as, bs), "Err in odbk handling")
			processed = true;
		}
		if ((a != NULL) || (b != NULL)) {
			URN_DEBUGF("\t -> odbk pair delta    %d %s", pairid, pair);
			URN_GO_FINAL_ON_RV(on_odbk_update(pairid, NULL, a, b), "Err in odbk handling")
			processed = true;
		}
		if (processed) {
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
		} else {
			URN_GO_FINAL_ON_RV(EINVAL, "nothing processed in array msg");
		}
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
int parse_n_mod_odbk_porder(int pairid, const char *type, yyjson_val *v, int op_type) {
	urn_inum *p=NULL, *s=NULL; // must free by hand if insertion failed.
	bool need_free_ps = true; // free when delete, or update failed.
	char prices[32], sizes[32];
	long ts_e6 = 0;
	bool buy;
	int rv = 0;

	if ((*type) == 'B')
		buy = true;
	else if ((*type) == 'A')
		buy = false;
	else
		return URN_FATAL("Unexpected order type", EINVAL);

	if (op_type != 4)
		return URN_FATAL("Unexpected op_type, Kraken only has guess type", EINVAL);

	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];

	// json in v: ['0.22775','9899.0',"1656487745.974814", "r"(optional)] : [price, size, ts]
	yyjson_val *v_el;
	size_t idx, max;
	const char *side;
	yyjson_arr_foreach(v, idx, max, v_el) {
		if (idx == 0) { // price
			urn_inum_alloc(&p, yyjson_get_str(v_el));
		} else if (idx == 1) { // size
			urn_inum_alloc(&s, yyjson_get_str(v_el));
		} else if (idx == 2) { // ts
			double ts;
			sscanf(yyjson_get_str(v_el), "%lf", &ts);
			ts_e6 = (long)(ts * 1000000);
			if (odbk_t_arr[pairid] < ts_e6) {
				odbk_t_arr[pairid] = ts_e6;
				wss_mkt_ts = ts_e6;
			}
		} else if (idx == 3) { // optional r
			;
		} else {
			URN_WARNF("idx %zu max %zu v_el %s", idx, max, yyjson_get_str(v_el));
			return URN_FATAL("Unexpected attr ct in order, should be 3 or 4", EINVAL);
		}
	}

	if ((s->intg == 0) && (s->frac_ext == 0))
		op_type = 3;
	else
		op_type = 2;

	URN_DEBUGF("\tOP %d %d %s %s %ld", op_type, buy, urn_inum_str(p), urn_inum_str(s), ts_e6);

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

int on_odbk(int pairid, const char *type, yyjson_val *asks, yyjson_val *bids) {
	// clear bids and asks for new snapshot.
	mkt_wss_odbk_purge(pairid);
	return on_odbk_update(pairid, type, asks, bids);
}

int on_odbk_update(int pairid, const char *type, yyjson_val *asks, yyjson_val *bids) {
	URN_DEBUGF("\ton_odbk %d %s", pairid, type);
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;

	char ask_or_bid = 0;
	if (bids != NULL) {
		yyjson_arr_foreach(bids, idx, max, v) {
			ask_or_bid = 'B';
			URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4),
				"Error in parse_n_mod_odbk_porder() for bids");
		}
		mkt_wss_odbk_trim_abnormal_px(pairid, false);
	}

	if (asks != NULL) {
		yyjson_arr_foreach(asks, idx, max, v) {
			ask_or_bid = 'A';
			URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4),
					"Error in parse_n_mod_odbk_porder() for asks");
		}
		mkt_wss_odbk_trim_abnormal_px(pairid, true);
	}
	return 0;
}

int on_tick(int pairid, yyjson_val *jdata) {
	// [["0.00023900","0.19649514","1657251478.152315","b","l",""]]
	// [[price, volume, time, side, order_type, misc], [] ...]
	URN_DEBUGF("on_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *v, *jval;
	size_t idx, max;
	yyjson_arr_foreach(jdata, idx, max, v) {
		const char *pstr, *sstr, *timestr, *side;
		URN_RET_ON_NULL(jval = yyjson_arr_get(v, 0), "No data/i/0", EINVAL);
		URN_RET_ON_NULL(pstr = yyjson_get_str(jval), "No data/i/0 str", EINVAL);
		URN_RET_ON_NULL(jval = yyjson_arr_get(v, 1), "No data/i/1", EINVAL);
		URN_RET_ON_NULL(sstr = yyjson_get_str(jval), "No data/i/1 str", EINVAL);
		URN_RET_ON_NULL(jval = yyjson_arr_get(v, 2), "No data/i/2", EINVAL);
		URN_RET_ON_NULL(timestr = yyjson_get_str(jval), "No data/i/2 str", EINVAL);
		URN_RET_ON_NULL(jval = yyjson_arr_get(v, 3), "No data/i/3", EINVAL);
		URN_RET_ON_NULL(side = yyjson_get_str(jval), "No data/i/3 str", EINVAL);

		urn_inum p, s;
		urn_inum_parse(&p, pstr);
		urn_inum_parse(&s, sstr);

		// timestamp in num
		double ts;
		sscanf(timestr, "%lf", &ts);
		double ts_e6 = (long)(ts * 1000000);

		bool buy = false;
		if (strcmp(side, "b") == 0)
			buy = true;
		else if (strcmp(side, "s") == 0)
			buy = false;
		else
			URN_RET_ON_NULL(NULL, "Unexpected data/i/side", EINVAL);
		urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);
	}
	return 0;
}
