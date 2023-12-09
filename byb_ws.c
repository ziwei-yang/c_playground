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
	// USDT-BTC@P -> BTCUSDT
	sprintf(*str, "%s%s", pair->asset, pair->currency);
	return 0;
}

int byb_wss_mode = -1; // 0: Bybit 1: BybitU
void mkt_data_set_exchange(char *s) {
	char *exch = getenv("uranus_spider_exchange");
	if (exch == NULL) {
		byb_wss_mode = 1;
		sprintf(s, "BybitL"); // Linear
	} else if (strcasecmp(exch, "bybitl") == 0) {
		byb_wss_mode = 1;
		sprintf(s, "BybitL"); // Linear
	} else if (strcasecmp(exch, "bybiti") == 0) {
		byb_wss_mode = 0;
		sprintf(s, "BybitI"); // Inverse
	} else
		URN_FATAL(exch, EINVAL);
}

void mkt_data_set_wss_url(char *s) {
	if (byb_wss_mode == 0)
		sprintf(s, "wss://stream.bybit.com/realtime");
	else if (byb_wss_mode == 1)
		sprintf(s, "wss://stream.bybit.com/realtime_public");
	else
		URN_FATAL("Unexpected byb_wss_mode", EINVAL);
}

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "orderBookL2_25.%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { chn[0] = '\0'; }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "trade.%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	// Send no more than 40 channels per request.
	int batch_sz = 20; // 20 odbk + 20 tick = 40 requests
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
		yyjson_mut_obj_add_str(wss_req_j, jroot, "op", "subscribe");
		URN_LOGF("preparing wss req batch %d:", batch);

		const char *batch_chn[(i_e-i_s)*2];
		for (int i=i_s; i<i_e; i++) {
			URN_LOGF("\tchn %d %s %s", i, odbk_chns[i], tick_chns[i]);
			batch_chn[(i-i_s)*2]   = odbk_chns[i];
			batch_chn[(i-i_s)*2+1] = tick_chns[i];
		}
		yyjson_mut_val *target_chns = yyjson_mut_arr_with_strcpy(
			wss_req_j, batch_chn, (i_e-i_s)*2);
		if (target_chns == NULL)
			return URN_FATAL("Error in strcpy json array", EINVAL);
		yyjson_mut_obj_add_val(wss_req_j, jroot, "args", target_chns);
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
	wss_req_interval_e = 10; // 1req/1024rd or bybit may disconnect
	wss_req_interval_ms = 1000;
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

	// request successful ack.
	if (yyjson_obj_get(jroot, "request") != NULL) {
		URN_INFOF("on_wss_msg omit %zu %.*s", len, (int)len, msg);
		goto final;
	}

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "topic"), "No topic", EINVAL);
	const char *topic = yyjson_get_str(jval);

	URN_RET_ON_NULL(jcore_data = yyjson_obj_get(jroot, "data"), "No data", EINVAL);

	// Most cases: depth channel
	uintptr_t pairid = 0;
	urn_hmap_getptr(depth_chn_to_pairid, topic, &pairid);
	if (pairid != 0) {
		char *depth_pair = pair_arr[pairid];
		URN_DEBUGF("depth_pair id %lu %s for topic %s", pairid, depth_pair, topic);

		// must have timestamp_e6 and type in depth message.
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "type"), "No type", EINVAL);
		const char *type = yyjson_get_str(jval);
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "timestamp_e6"), "No timestamp_e6", EINVAL);
		if (byb_wss_mode == 0) {
			long ts_e6 = yyjson_get_uint(jval);
			odbk_t_arr[pairid] = ts_e6;
			if (ts_e6 == 0) {
				URN_WARNF("depth_pair id %lu %s for topic %s tse6 %ld", pairid, depth_pair, topic, ts_e6);
				URN_GO_FINAL_ON_RV(EINVAL, "No t_e6");
			}
		} else if (byb_wss_mode == 1) {
			const char *ts_e6 = yyjson_get_str(jval);
			odbk_t_arr[pairid] = strtol(ts_e6, NULL, 10);
			if (ts_e6 == NULL) {
				URN_WARNF("depth_pair id %lu %s for topic %s tse6 %s", pairid, depth_pair, topic, ts_e6);
				URN_GO_FINAL_ON_RV(EINVAL, "No t_e6");
			}
		}
		wss_mkt_ts = odbk_t_arr[pairid];

		if (depth_pair == NULL) {
			URN_WARNF("NO depth_pair id %lu for topic %s", pairid, topic);
			URN_GO_FINAL_ON_RV(EINVAL, topic);
		} else if (strcmp(type, "snapshot") == 0) {
			URN_DEBUGF("\t odbk pair snapshot %lu %s", pairid, depth_pair);
			URN_GO_FINAL_ON_RV(on_odbk(pairid, type, jcore_data), "Err in odbk handling")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
		} else if (strcmp(type, "delta") == 0) {
			URN_DEBUGF("\t odbk pair delta    %lu %s", pairid, depth_pair);
			URN_GO_FINAL_ON_RV(on_odbk_update(pairid, type, jcore_data), "Err in odbk handling")
			URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()")
		} else
			URN_GO_FINAL_ON_RV(EINVAL, type);
		goto final;
	}

	// If not depth chn, might be trade chn.
	urn_hmap_getptr(trade_chn_to_pairid, topic, &pairid);
	if (pairid != 0) {
		char *trade_pair = pair_arr[pairid];
		URN_DEBUGF("trade_pair id %lu %s for topic %s", pairid, trade_pair, topic);
		URN_DEBUGF("\t trade data %s %.*s", trade_pair, len, msg);
		URN_GO_FINAL_ON_RV(on_tick(pairid, jcore_data), "Err in tick handling")
		goto final;
	}

	// Unknown topic
	URN_GO_FINAL_ON_RV(EINVAL, msg);

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}

// type: no use here
// op_type: 0 order_book - snapshot
// op_type: 1 insert     - delta
// op_type: 2 update     - delta
// op_type: 3 delete     - delta
int parse_n_mod_odbk_porder(int pairid, const char *type, yyjson_val *v, int op_type) {
	urn_inum *p=NULL, *s=NULL; // must free by hand if insertion failed.
	bool need_free_ps = true; // free when delete, or update failed.

	yyjson_val *pricev, *symbolv, *sidev, *sizev;
	const char *price, *symbol, *side, *sizestr;
	char sizestr2[32];
	double sizef=0;
	int sizei = 0;
	bool buy;
	int rv = 0;

	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];

	// json in v:
	// "price":"23900.00","symbol":"BTCUSDT", "id":"239000000","side":"Buy","size":1.705
	URN_RET_ON_NULL(pricev = yyjson_obj_get(v, "price"), "No price", EINVAL);
	URN_RET_ON_NULL(symbolv= yyjson_obj_get(v, "symbol"), "No symbol", EINVAL);
	URN_RET_ON_NULL(sidev  = yyjson_obj_get(v, "side"), "No side", EINVAL);
	sizev = yyjson_obj_get(v, "size");
	if ((op_type != 3) && (sizev == NULL)) { // deletion has no size
		URN_FATAL("No size", EINVAL);
	}

	price = yyjson_get_str(pricev);
	symbol= yyjson_get_str(symbolv);
	side  = yyjson_get_str(sidev);
	if (sizev != NULL) { // deletion has no size
		// integer size might make sizef zero, could be anyone of below case.
		sizestr = yyjson_get_raw(sizev);
		sizef = yyjson_get_real(sizev);
		sizei = yyjson_get_uint(sizev);
		if (sizestr != NULL) {
			urn_inum_alloc(&s, sizestr);
		} else if (sizef != 0) {
			// check float value before int, in case of 1.9 -> 1, 0.9 -> 0
			sprintf(sizestr2, "%12.12f", sizef); // %f loses precision
			urn_inum_alloc(&s, sizestr2);
		} else if (sizei != 0) {
			sprintf(sizestr2, "%d", sizei);
			urn_inum_alloc(&s, sizestr2);
		}
		URN_DEBUGF("OP %d %s %s %s %s sizes %s %s %12.12f %d", op_type, price, symbol, side, yyjson_get_type_desc(sizev), sizestr, sizestr2, sizef, sizei);
	} else
		URN_DEBUGF("OP %d %s %s %s no size", op_type, price, symbol, side);

	if (strcmp(side, "Buy") == 0)
		buy = 1;
	else if (strcmp(side, "Sell") == 0)
		buy = 0;
	else
		URN_FATAL(side, EINVAL);

	urn_inum_alloc(&p, price);

	// Insertion, deletion and overwriting.
	if (op_type == 2 || op_type == 3) { // update or delete
		bool op_done = mkt_wss_odbk_update_or_delete(pairid, p, s, buy, (op_type==2));
		if ((op_type == 2) && op_done)
			need_free_ps = false;
	} else if (op_type == 0 || op_type == 1) { // snapshot or insert
		need_free_ps = false;
		mkt_wss_odbk_insert(pairid, p, s, buy);
	}

final:
	print_odbk(pairid);
	if (need_free_ps) {
		if (p != NULL) free(p);
		if (s != NULL) free(s);
	}
	return 0;
}

int on_odbk(int pairid, const char *type, yyjson_val *jdata) {
	URN_DEBUGF("on_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	// clear bids and asks for new snapshot.
	mkt_wss_odbk_purge(pairid);

	// Parse snapshot data
	yyjson_val *orders = NULL;
	if (byb_wss_mode == 0) {
		orders = jdata;
	} else if (byb_wss_mode == 1) {
		URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "order_book"), "No jdata/order_book", EINVAL);
	}
	yyjson_val *v;
	size_t idx, max;
	yyjson_arr_foreach(orders, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, type, v, 0), "Error in parse_n_mod_odbk_porder()");
	}
	print_odbk(pairid);
	return 0;
}

int on_odbk_update(int pairid, const char *type, yyjson_val *jdata) {
	URN_DEBUGF("on_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *orders = NULL;

	// delete
	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "delete"), "No jdata/delete", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, type, v, 3), "Error in parse_n_mod_odbk_porder() to delete");
	}

	// update
	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "update"), "No jdata/update", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, type, v, 2), "Error in parse_n_mod_odbk_porder() to update");
	}

	// insert
	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "insert"), "No jdata/insert", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, type, v, 1), "Error in parse_n_mod_odbk_porder() to insert");
	}

	return 0;
}

int on_tick(int pairid, yyjson_val *jdata) {
	URN_DEBUGF("on_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	yyjson_val *v, *jval;
	size_t idx, max;
	yyjson_arr_foreach(jdata, idx, max, v) {
		unsigned long ts_e6 = 0;
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "trade_time_ms"), "No data/trade_time_ms", EINVAL);
		if (byb_wss_mode == 0) {
			// trade_time_ms in integer
			ts_e6 = yyjson_get_uint(jval) * 1000;
		} else {
			// trade_time_ms in string
			const char *timestr = yyjson_get_str(jval);
			URN_RET_ON_NULL(timestr, "No data/trade_time_ms str", EINVAL);
			ts_e6 = strtol(timestr, NULL, 10) * 1000;
		}
		URN_RET_IF(ts_e6 <= 0, "No data/trade_time_ms valid value", EINVAL);

		urn_inum p, s;
		const char *sstr=NULL, *pstr=NULL;
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "price"), "No data/price", EINVAL);
		if (byb_wss_mode == 0) {
			// price in num
			double pd = yyjson_get_real(jval);
			if (pd <= 0)
				pd = (double)(yyjson_get_uint(jval));
			URN_RET_IF(pd <= 0, "No data/price double", EINVAL);
			urn_inum_from_db(&p, pd);
		} else {
			// price in string
			URN_RET_ON_NULL((pstr = yyjson_get_str(jval)), "No data/price str", EINVAL);
			urn_inum_parse(&p, pstr);
		}

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "size"), "No data/size", EINVAL);
		// size always in num.
		double sd = yyjson_get_real(jval);
		if (sd <= 0)
			sd = (double)(yyjson_get_uint(jval));
		URN_RET_IF(sd <= 0, "No data/size double", EINVAL);
		urn_inum_from_db(&s, sd);

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "side"), "No data/side", EINVAL);
		const char *side = yyjson_get_str(jval);
		URN_RET_ON_NULL(side, "No data/side str", EINVAL);
		bool buy = false;
		if (strcmp(side, "Buy") == 0)
			buy = true;
		else if (strcmp(side, "Sell") == 0)
			buy = false;
		else
			URN_RET_ON_NULL(NULL, "Unexpected data/side", EINVAL);
		urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);
	}
	return 0;
}
