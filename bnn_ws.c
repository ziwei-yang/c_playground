// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

// local options
#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

//#define PUB_NO_REDIS

#include "mkt_wss.h"

int   on_odbk(int pairid, const char *type, yyjson_val *jdata);
int   on_odbk_update(int pairid, const char *type, yyjson_val *jdata);

char *preprocess_pair(char *pair) {
	// BUSD-ASSET -> USD-ASSET
	if (pair == NULL) URN_FATAL("pair is NULL", EINVAL);
	int slen = strlen(pair);
	if (slen > 5 && pair[0] == 'B' && pair[1] == 'U' &&
		pair[2] == 'S' && pair[3] == 'S' && pair[4] == '-') {
		char *new_p = malloc(slen+1-1);
		strcpy(new_p, pair+1);
		free(pair);
		return new_p;
	}
	return pair;
}

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+2);
	if ((*str) == NULL) return ENOMEM;
	// USDT-BTC   -> btcusdt
	// USD-BTC   ->  btcbusd
	if (pair->expiry == NULL) {
		if (strcmp(pair->currency, "USD") == 0) {
			// see preprocess_pair()
			sprintf(*str, "%sBUSD", pair->asset);
		} else {
			sprintf(*str, "%s%s", pair->asset, pair->currency);
		}
		urn_s_downcase(*str, slen);
	} else {
		return URN_FATAL("Not expected pair", EINVAL);
	}
	return 0;
}

void mkt_data_set_exchange(char *s) { sprintf(s, "Binance"); }

void mkt_data_set_wss_url(char *s) { sprintf(s, "wss://stream.binance.com:9443/stream"); }

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s@depth@100ms", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s@depth10", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s@aggTrade", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	/*
	"method": "SUBSCRIBE",
	"params": [
		"btcusdt@aggTrade",
		"btcusdt@depth10" // 10 bid/ask snapshot, per 1000ms
		"btcusdt@depth@100ms" // odbk delta
	],
	"id": 1
	*/

	// Too many channels in one command may cause binance server NO response.
	//
	// Send no more than 60 channels per request.
	int batch_sz = 20; // 20 odbk + 20 snapshot + 20 tick = 60 requests
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
		yyjson_mut_obj_add_str(wss_req_j, jroot, "method", "SUBSCRIBE");
		URN_LOGF("preparing wss req batch %d:", batch);

		const char *batch_chn[(i_e-i_s)*3];
		for (int i=i_s; i<i_e; i++) {
			URN_LOGF("\tchn %d %s %s %s", i, odbk_chns[i], odbk_snpsht_chns[i], tick_chns[i]);
			batch_chn[(i-i_s)*3]   = odbk_chns[i];
			batch_chn[(i-i_s)*3+1] = odbk_snpsht_chns[i];
			batch_chn[(i-i_s)*3+2] = tick_chns[i];
		}
		yyjson_mut_val *target_chns = yyjson_mut_arr_with_strcpy(
			wss_req_j, batch_chn, (i_e-i_s)*3);
		if (target_chns == NULL)
			return URN_FATAL("Error in strcpy json array", EINVAL);
		yyjson_mut_obj_add_val(wss_req_j, jroot, "params", target_chns);
		yyjson_mut_obj_add_int(wss_req_j, jroot, "id", cmd_ct+1);
		// save command
		char *prettyj = yyjson_mut_write(wss_req_j, YYJSON_WRITE_PRETTY, NULL);
		URN_LOGF("req %d : %s", cmd_ct, prettyj);
		free(prettyj);
		char *cmd = yyjson_mut_write(wss_req_j, 0, NULL); // one-line json
		wss_req_s[cmd_ct] = cmd;
		URN_INFOF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;
		yyjson_mut_doc_free(wss_req_j);
	}
	wss_req_s[cmd_ct] = NULL;

	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	wss_req_interval_e = 9 ; // 1req/512or binance may disconnect
	return 0;
}

int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jcore_data = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, (int)len, msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	jval = yyjson_obj_get(jroot, "id");
	if (jval != NULL) { // response of wss_req
		URN_INFOF("on_wss_msg omit wss_req response %.*s", (int)len, msg);
		goto final;
	}

	jval = yyjson_obj_get(jroot, "stream");
	if (jval != NULL) {
		const char *channel = yyjson_get_str(jval);
		URN_RET_ON_NULL(jcore_data = yyjson_obj_get(jroot, "data"), "No data", EINVAL);

		uintptr_t pairid = 0;
		urn_hmap_get(depth_chn_to_pairid, channel, &pairid);
		if (pairid != 0) {
			URN_RET_ON_NULL(jval = yyjson_obj_get(jcore_data, "E"), "No data/E", EINVAL);
			long ts_e6 = yyjson_get_uint(jval) * 1000l;
			odbk_t_arr[pairid] = ts_e6;
			wss_stat_mkt_ts = ts_e6;
			newodbk_arr[pairid] ++;

			char *depth_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk pair delta    %lu %s %ld", pairid, depth_pair, ts_e6);
			URN_GO_FINAL_ON_RV(on_odbk_update(pairid, NULL, jcore_data), "Err in odbk handling")
			goto final;
		}

		urn_hmap_get(depth_snpsht_chn_to_pairid, channel, &pairid);
		if (pairid != 0) {
			// No E in snapshot data.
			char *depth_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk pair snapshot %lu %s", pairid, depth_pair);
//			URN_GO_FINAL_ON_RV(on_odbk(pairid, NULL, jcore_data), "Err in odbk handling")
			goto final;
		}

		urn_hmap_get(trade_chn_to_pairid, channel, &pairid);
		if (pairid != 0) {
			URN_RET_ON_NULL(jval = yyjson_obj_get(jcore_data, "E"), "No data/E", EINVAL);
			long ts_e6 = yyjson_get_int(jval) * 1000l;

			char *trade_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> tick pair          %lu %s %ld", pairid, trade_pair, ts_e6);
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

	bool buy;
	int rv = 0;

	if ((*type) == 'B')
		buy = true;
	else if ((*type) == 'A')
		buy = false;
	else
		return URN_FATAL("Unexpected order type", EINVAL);

	if (op_type != 4)
		return URN_FATAL("Unexpected op_type, FTX only has guess type", EINVAL);

	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];

	// json in v:
	// ['0.22775','9899.0'] : [price, size]
	yyjson_val *v_el;
	int el_ct = 0;
	size_t idx, max;
	yyjson_arr_foreach(v, idx, max, v_el) {
		if (el_ct == 0) { // price
			urn_inum_alloc(&p, yyjson_get_str(v_el));
		} else if (el_ct == 1) { // size
			urn_inum_alloc(&s, yyjson_get_str(v_el));
		} else
			return URN_FATAL("Unexpected attr ct in Binance order, should be 2", EINVAL);
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

	// same data format with on_odbk_update()
	return on_odbk_update(pairid, type, jdata);
}

int on_odbk_update(int pairid, const char *type, yyjson_val *jdata) {
	URN_DEBUGF("\ton_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *orders = NULL;

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "b"), "No jdata/b", EINVAL);
	char ask_or_bid = 0;
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'B';
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4), "Error in parse_n_mod_odbk_porder() for bids");
	}

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "a"), "No jdata/a", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'A';
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4), "Error in parse_n_mod_odbk_porder() for asks");
	}
	return 0;
}

