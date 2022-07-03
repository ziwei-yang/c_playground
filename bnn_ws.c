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

int bnn_wss_mode = -1; // 0: Binance 1: BNUM 2: BNCM

char *preprocess_pair(char *pair) {
	if (pair == NULL) URN_FATAL("pair is NULL", EINVAL);
	int slen = strlen(pair);
	if (bnn_wss_mode == 0) { // SPOT mode
		// BUSD-ASSET -> USD-ASSET
		if (slen > 5 && pair[0] == 'B' && pair[1] == 'U' &&
				pair[2] == 'S' && pair[3] == 'D' && pair[4] == '-') {
			// move 1 forward, strcpy(n, n+1) crashes on macos
			for (int i=0; i<slen; i++)
				pair[i] = pair[i+1];
			return pair;
		}
		return pair;
	}
	return pair;
}

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+2);
	if ((*str) == NULL) return ENOMEM;
	if (bnn_wss_mode == 0) { // SPOT mode
		if (pair->expiry != NULL)
			return URN_FATAL("Pair with expiry in spot mode", EINVAL);
		// USDT-BTC   -> btcusdt
		// USD-BTC   ->  btcbusd
		if (strcmp(pair->currency, "USD") == 0) {
			// see preprocess_pair()
			sprintf(*str, "%sBUSD", pair->asset);
		} else
			sprintf(*str, "%s%s", pair->asset, pair->currency);
		urn_s_downcase(*str, slen);
	} else if (bnn_wss_mode == 1 || bnn_wss_mode == 2) {
		// perpetual contracts only.
		// BNUM USDT-BTC@P -> btcusdt
		// BNCM USDT-BTC@P -> btcusdt_perp
		if (pair->expiry == NULL) {
			return URN_FATAL("Spot pair in BNUM mode", EINVAL);
		} else if (strcmp(pair->expiry, "P") != 0) {
			return URN_FATAL("Not perpetual pair in BNUM mode", EINVAL);
		}
		if (bnn_wss_mode == 1) // BNUM
			sprintf(*str, "%s%s", pair->asset, pair->currency);
		else if (bnn_wss_mode == 2) // BNCM
			sprintf(*str, "%s%s_perp", pair->asset, pair->currency);
		else
			URN_FATAL(exchange, EINVAL);
		urn_s_downcase(*str, slen);
	} else
		URN_FATAL(exchange, EINVAL);
	return 0;
}

void mkt_data_set_exchange(char *s) {
	char *exch = getenv("uranus_spider_exchange");
	if (exch == NULL) {
		bnn_wss_mode = 0;
		sprintf(s, "Binance");
	} else if (strcasecmp(exch, "bnum") == 0) {
		bnn_wss_mode = 1;
		sprintf(s, "BNUM");
	} else if (strcasecmp(exch, "bncm") == 0) {
		bnn_wss_mode = 2;
		sprintf(s, "BNCM");
	} else
		URN_FATAL(exch, EINVAL);
}

void mkt_data_set_wss_url(char *s) {
	if (bnn_wss_mode == 0)
		sprintf(s, "wss://stream.binance.com:9443/stream");
	else if (bnn_wss_mode == 1)
		sprintf(s, "wss://fstream.binance.com/stream");
	else if (bnn_wss_mode == 2)
		sprintf(s, "wss://dstream.binance.com/stream");
	else
		URN_FATAL(exchange, EINVAL);
}

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s@depth@100ms", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s@depth10@500ms", sym); }

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
	// For Binance SPOT market, no more than 120 channels per request.
	int batch_sz = 40; // odbk + snapshot + tick = 120 requests
	// For BNUM and BNCM it is okay to send all in one.
	if (bnn_wss_mode != 0) batch_sz = max_pair_id;

	// Over than 180~200 subscribes in total makes BNUM server hang
	int chn_per_pair = 3;
	bool req_trades = true;
	if (bnn_wss_mode != 0 && (max_pair_id * 3 > 180)) {
		URN_WARNF("BNUM/BNCM %d pairs, give up trades listening", max_pair_id);
		req_trades = false;
		chn_per_pair = 2;
	}
	bool req_snapshot = true;
	if (bnn_wss_mode != 0 && (max_pair_id * 2 > 180)) {
		URN_WARNF("BNUM/BNCM %d pairs, give up snapshot listening", max_pair_id);
		req_snapshot = false;
		chn_per_pair = 1;
	}
	if (bnn_wss_mode != 0 && max_pair_id > 180)
		URN_WARNF("BNUM/BNCM %d pairs, more than 180~200 subscrptions might get hang there", max_pair_id);

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

		const char *batch_chn[(i_e-i_s)*chn_per_pair];
		for (int i=i_s; i<i_e; i++) {
			URN_DEBUGF("\tchn %d %s %s %s", i, odbk_chns[i], odbk_snpsht_chns[i], tick_chns[i]);
			batch_chn[(i-i_s)*chn_per_pair]   = odbk_chns[i];
			if (chn_per_pair >= 2)
			batch_chn[(i-i_s)*chn_per_pair+1] = odbk_snpsht_chns[i];
			if (chn_per_pair >= 3)
				batch_chn[(i-i_s)*chn_per_pair+2] = tick_chns[i];
		}
		yyjson_mut_val *target_chns = yyjson_mut_arr_with_strcpy(
			wss_req_j, batch_chn, (i_e-i_s)*chn_per_pair);
		if (target_chns == NULL)
			return URN_FATAL("Error in strcpy json array", EINVAL);
		yyjson_mut_obj_add_val(wss_req_j, jroot, "params", target_chns);
		yyjson_mut_obj_add_int(wss_req_j, jroot, "id", cmd_ct+1);
		// save command
		char *prettyj = yyjson_mut_write(wss_req_j, YYJSON_WRITE_PRETTY, NULL);
		URN_DEBUGF("req %d : %s", cmd_ct, prettyj);
		free(prettyj);
		char *cmd = yyjson_mut_write(wss_req_j, 0, NULL); // one-line json
		wss_req_s[cmd_ct] = cmd;
		URN_DEBUGF("req %d : %s", cmd_ct, cmd);
		cmd_ct++;
		yyjson_mut_doc_free(wss_req_j);
	}
	wss_req_s[cmd_ct] = NULL;

	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	if (bnn_wss_mode != 0) {
		wss_req_interval_e = 4;
		wss_req_interval_ms = 300;
	} else
		wss_req_interval_e = 6; // 1req/2^10or binance may disconnect
		wss_req_interval_ms = 500;
	return 0;
}

int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jcore_data = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

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
		urn_hmap_getptr(depth_chn_to_pairid, channel, &pairid);
		if (pairid != 0) {
			URN_RET_ON_NULL(jval = yyjson_obj_get(jcore_data, "E"), "No data/E", EINVAL);
			long ts_e6 = yyjson_get_uint(jval) * 1000l;
			odbk_t_arr[pairid] = ts_e6;
			wss_mkt_ts = ts_e6;

			char *depth_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk pair delta    %lu %s %ld", pairid, depth_pair, ts_e6);
			URN_GO_FINAL_ON_RV(on_odbk_update(pairid, NULL, jcore_data), "Err in odbk handling")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
			goto final;
		}

		urn_hmap_getptr(depth_snpsht_chn_to_pairid, channel, &pairid);
		if (pairid != 0) {
			// No E in snapshot data.
			char *depth_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> odbk pair snapshot %lu %s", pairid, depth_pair);
			URN_GO_FINAL_ON_RV(on_odbk(pairid, NULL, jcore_data), "Err in odbk handling")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
			goto final;
		}

		urn_hmap_getptr(trade_chn_to_pairid, channel, &pairid);
		if (pairid != 0) {
			URN_RET_ON_NULL(jval = yyjson_obj_get(jcore_data, "E"), "No data/E", EINVAL);
			long ts_e6 = yyjson_get_int(jval) * 1000l;

			char *trade_pair = pair_arr[pairid];
			URN_DEBUGF("\t -> tick pair          %lu %s %ld", pairid, trade_pair, ts_e6);
			URN_GO_FINAL_ON_RV(tick_updated(pairid), "Err in tick_updated()")
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
		return URN_FATAL("Unexpected op_type, Binance only has guess type", EINVAL);

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
