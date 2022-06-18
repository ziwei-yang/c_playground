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

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	*str = malloc(strlen(pair->name));
	if ((*str) == NULL) return ENOMEM;
	// USD-BTC@P -> BTC-PERP
	// USD-BTC   -> BTC/USD
	if (pair->expiry == NULL)
		sprintf(*str, "%s/%s", pair->asset, pair->currency);
	else {
		if (strcmp(pair->currency, "USD") == 0) {
			sprintf(*str, "%s-PERP", pair->asset);
		} else {
			urn_pair_print(*pair);
			return URN_FATAL("Not expected pair", EINVAL);
		}
	}
	return 0;
}

void mkt_data_set_exchange(char *s) { sprintf(s, "FTX"); }

void mkt_data_set_wss_url(char *s) { sprintf(s, "wss://ftx.com/ws/"); }

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { chn[0] = '\0'; }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	int cmd_ct = 0;
	for(int i=0; i<chn_ct; i++) {
		char *cmd = malloc(128);
		sprintf(cmd, "{\"op\":\"subscribe\",\"channel\":\"orderbook\",\"market\":\"%s\"}", odbk_chns[i]);
		wss_req_s[cmd_ct] = cmd;
		URN_INFOF("%d: %s", cmd_ct, cmd);
		cmd_ct++;

		cmd = malloc(128);
		sprintf(cmd, "{\"op\":\"subscribe\",\"channel\":\"trades\",\"market\":\"%s\"}", tick_chns[i]);
		wss_req_s[cmd_ct] = cmd;
		URN_INFOF("%d: %s", cmd_ct, cmd);
		cmd_ct++;
	}
	wss_req_s[cmd_ct] = NULL;
	URN_INFOF("Parsing ARGV end, %d req str prepared.", cmd_ct);
	wss_req_interval_e = 1;
	wss_req_interval_ms = 3;
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

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "type"), "No type", EINVAL);
	const char *type = yyjson_get_str(jval); // subscribed

	// omit allowed error.
	if (strcmp(type, "error") == 0) {
		jval = yyjson_obj_get(jroot, "code");
		if (jval == NULL) {
			rv = EINVAL;
			goto final;
		}
		int errcode = yyjson_get_uint(jval);
		URN_WARNF("on_wss_msg omit error %d %.*s", errcode, (int)len, msg);
		if (errcode == 404) {
			;
		} else
			rv = EINVAL;
		goto final;
	}

	if (strcmp(type, "subscribed") == 0) {
		URN_INFOF("on_wss_msg omit %zu %.*s", len, (int)len, msg);
		goto final;
	}

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "channel"), "No channel", EINVAL);
	const char *channel = yyjson_get_str(jval); // orderbook or trades

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "market"), "No market", EINVAL);
	const char *market = yyjson_get_str(jval); // which market

	URN_RET_ON_NULL(jcore_data = yyjson_obj_get(jroot, "data"), "No data", EINVAL);

	uintptr_t pairid = 0;
	if (strcmp(channel, "orderbook") == 0) {
		urn_hmap_get(depth_chn_to_pairid, market, &pairid);
		if (pairid == 0) {
			URN_WARNF("on_wss_msg pairid could not located %s\n%.*s", market, (int)len, msg);
			goto final;
		}
		char *depth_pair = pair_arr[pairid];
		if (depth_pair == NULL) {
			URN_WARNF("on_wss_msg depth_pair could not located %s\n%.*s", market, (int)len, msg);
			goto final;
		}
		URN_DEBUGF("depth_pair id %lu %s", pairid, depth_pair);

		// must have timestamp_e6 and type in depth message.
		URN_RET_ON_NULL(jval = yyjson_obj_get(jcore_data, "action"), "No action", EINVAL);
		const char *action = yyjson_get_str(jval);

		URN_RET_ON_NULL(jval = yyjson_obj_get(jcore_data, "time"), "No time", EINVAL);
		double ts = yyjson_get_real(jval);
		if (ts == 0) {
			URN_WARNF("depth_pair id %lu %s for action %s time %lf", pairid, depth_pair, action, ts);
			URN_GO_FINAL_ON_RV(EINVAL, "No time");
		}
		long ts_e6 = (long)(ts*1000000l);
		odbk_t_arr[pairid] = ts_e6;

		newodbk_arr[pairid] ++;
		wss_stat_mkt_ts = odbk_t_arr[pairid];

		if (strcmp(action, "partial") == 0) {
			URN_DEBUGF("\t -> odbk pair snapshot %lu %s %ld", pairid, depth_pair, ts_e6);
			URN_GO_FINAL_ON_RV(on_odbk(pairid, action, jcore_data), "Err in odbk handling")
		} else if (strcmp(action, "update") == 0) {
			URN_DEBUGF("\t -> odbk pair delta    %lu %s %ld", pairid, depth_pair, ts_e6);
			URN_GO_FINAL_ON_RV(on_odbk_update(pairid, action, jcore_data), "Err in odbk handling")
		} else
			URN_GO_FINAL_ON_RV(EINVAL, action);
		goto final;
	}

	if (strcmp(channel, "trades") == 0) {
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
	double pricef=-1, sizef=-1;
	int pricei=-1, sizei=-1;

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
	// [0.22775,9899.0] : [price, size]
	yyjson_val *v_el;
	int el_ct = 0;
	size_t idx, max;
	yyjson_arr_foreach(v, idx, max, v_el) {
		if (el_ct == 0) { // price
			pricef = yyjson_get_real(v_el);
			if (pricef != 0) {
				sprintf(prices, "%12.12f", pricef); // %f loses precision
			} else {
				pricei = yyjson_get_uint(v_el);
				sprintf(prices, "%d", pricei);
			}
			urn_inum_alloc(&p, prices);
		} else if (el_ct == 1) { // size
			sizef = yyjson_get_real(v_el);
			if (sizef != 0) {
				if (sizef == 0) {
					op_type = 3; // delete
				} else {
					op_type = 2; // update
					sprintf(sizes, "%12.12f", sizef); // %f loses precision
					urn_inum_alloc(&s, sizes);
				}
			} else {
				sizei = yyjson_get_uint(v_el);
				if (sizei == 0) {
					op_type = 3; // delete
				} else {
					op_type = 2; // update
					sprintf(sizes, "%d", sizei);
					urn_inum_alloc(&s, sizes);
				}
			}
		} else
			return URN_FATAL("Unexpected attr ct in FTX order, should be 2", EINVAL);
		el_ct ++;
	}

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

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "bids"), "No jdata/bids", EINVAL);
	char ask_or_bid = 0;
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'B';
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4), "Error in parse_n_mod_odbk_porder() for bids");
	}

	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "asks"), "No jdata/asks", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'A';
		URN_RET_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4), "Error in parse_n_mod_odbk_porder() for asks");
	}
	return 0;
}
