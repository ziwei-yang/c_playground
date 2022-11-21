// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

// local options
#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

//#define PUB_NO_REDIS

#include "mkt_wss.h"

int   on_tick(int pairid, double lot, yyjson_val *jdata);
int   on_odbk(int pairid, double lot, yyjson_val *jdata, char type);
int   on_instruments(yyjson_val *jdata);

double lot_arr[MAX_PAIRS] = {0}; // OKX uses different lots for contracts/pairs

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+16);
	if ((*str) == NULL) return ENOMEM;
	// USDT-BTC   -> BTC-USDT
	// USDT-BTC@P -> BTC-USDT-SWAP
	if (pair->expiry == NULL)
		sprintf(*str, "%s-%s", pair->asset, pair->currency);
	else {
		if (strcmp(pair->expiry, "P") == 0) {
			sprintf(*str, "%s-%s-SWAP", pair->asset, pair->currency);
		} else {
			urn_pair_print(*pair);
			return URN_FATAL("Not expected pair", EINVAL);
		}
	}
	urn_s_upcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) { sprintf(s, "OKX"); }

void mkt_data_set_wss_url(char *s) { sprintf(s, "wss://ws.okx.com:8443/ws/v5/public"); }

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	// Command includes header, instrument(4), [odbk(2), trade(1)] x chn_ct
	char *cmd = malloc(64*(6+chn_ct));
	int ct = 0;
	ct += sprintf(cmd+ct, "{\"op\":\"subscribe\",\"args\":[\n");
	ct += sprintf(cmd+ct, "  {\"channel\":\"instruments\",\"instType\":\"SWAP\"},\n");
	ct += sprintf(cmd+ct, "  {\"channel\":\"instruments\",\"instType\":\"FUTURES\"},\n");
	ct += sprintf(cmd+ct, "  {\"channel\":\"instruments\",\"instType\":\"SPOT\"},\n");
	for(int i=0; i<chn_ct; i++) {
		ct += sprintf(cmd+ct, "  {\"channel\":\"trades\",\"instId\":\"%s\"},\n", tick_chns[i]);
		ct += sprintf(cmd+ct, "  {\"channel\":\"books5\",\"instId\":\"%s\"},\n", odbk_snpsht_chns[i]);
		ct += sprintf(cmd+ct, "  {\"channel\":\"bbo-tbt\",\"instId\":\"%s\"},\n", odbk_chns[i]);
	}
	ct += sprintf(cmd+ct, "  {\"channel\":\"instruments\",\"instType\":\"OPTION\"}\n");
	ct += sprintf(cmd+ct, "]}");
	wss_req_s[0] = cmd;
	wss_req_s[1] = NULL;
	URN_INFOF("Parsing ARGV end, 1 req str prepared.");
	return 0;
}

int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jcore_data = NULL;
	yyjson_val *jcore_arg = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	jval = yyjson_obj_get(jroot, "event");
	if (jval != NULL) {
		const char *event = yyjson_get_str(jval);
		if (strcmp(event, "subscribe") == 0) {
			URN_INFO(msg);
			return 0;
		} else {
			URN_WARNF("Unknown event %s", msg);
			return 0;
		}
	}

	URN_GO_FINAL_ON_NULL(jcore_arg = yyjson_obj_get(jroot, "arg"), "No arg");
	URN_GO_FINAL_ON_NULL(jval = yyjson_obj_get(jcore_arg, "channel"), "No arg/channel");
	const char *channel = yyjson_get_str(jval);

	jval = yyjson_obj_get(jcore_arg, "instId");
	const char *inst_id = (jval == NULL) ? NULL : yyjson_get_str(jval);
	URN_DEBUGF("%s %s", channel, inst_id);

	URN_RET_ON_NULL(jcore_data = yyjson_obj_get(jroot, "data"), "No data", EINVAL);
	if (strcmp(channel, "instruments") == 0) {
		on_instruments(jcore_data);
		goto final;
	}

	uintptr_t pairid = 0;
	double lot = 0; // contract value
	if (inst_id != NULL) {
		urn_hmap_getptr(symb_to_pairid, inst_id, &pairid);
		if (pairid == 0) {
			URN_WARNF("Could not find %s pairid in record", inst_id);
			goto final;
		}
		lot = lot_arr[pairid];
		if (pairid == 0) {
			URN_WARNF("Could not find %s ctVal in record", inst_id);
			goto final;
		}
	}

	if (strcmp(channel, "bbo-tbt") == 0 ) {
		URN_GO_FINAL_ON_RV(on_odbk(pairid, lot, jcore_data, 'T'), "Err in top-odbk handling");
		URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()");
		goto final;
	}
	if (strcmp(channel, "books5") == 0 ) {
		URN_GO_FINAL_ON_RV(on_odbk(pairid, lot, jcore_data, 'S'), "Err in odbk handling");
		URN_GO_FINAL_ON_RV(odbk_updated(pairid), "Err in odbk_updated()");
		goto final;
	}
	if (strcmp(channel, "trades") == 0 ) {
		URN_GO_FINAL_ON_RV(on_tick(pairid, lot, jcore_data), "Err in tick handling");
		URN_GO_FINAL_ON_RV(tick_updated(pairid), "Err in tick_updated()");
		goto final;
	}

	// Unknown type
	URN_GO_FINAL_ON_RV(EINVAL, msg);

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}

int parse_n_insert_odbk_porder(int pairid, bool buy, double lot, yyjson_val *v, bool is_top) {
	urn_inum *p=NULL, *s=NULL; // must free by hand if insertion failed.

	int rv = 0;

	// json in v:
	// ["0.22775","9899.0",...] : [price, size]
	yyjson_val *v_el;
	int el_ct = 0;
	size_t idx, max;
	yyjson_arr_foreach(v, idx, max, v_el) {
		if (el_ct == 0) { // price
			urn_inum_alloc(&p, yyjson_get_str(v_el));
		} else if (el_ct == 1) { // size
			urn_inum_alloc(&s, yyjson_get_str(v_el));
			if (lot != 1)
				urn_inum_from_db(s, lot * urn_inum_to_db(s));
		}
		el_ct ++;
	}

	if (is_top) {
		URN_DEBUGF("\tTOP    %s %s %s", (buy ? "BUY" : "SEL"), urn_inum_str(p), urn_inum_str(s));
		mkt_wss_odbk_update_top(pairid, p, s, buy);
	} else {
		URN_DEBUGF("\tINSERT %s %s %s", (buy ? "BUY" : "SEL"), urn_inum_str(p), urn_inum_str(s));
		mkt_wss_odbk_insert(pairid, p, s, buy);
	}

final:
	print_odbk(pairid);
	return 0;
}

int on_odbk(int pairid, double lot, yyjson_val *jdata, char type) {
	char* pair = pair_arr[pairid];
	URN_DEBUGF("\ton_odbk %c %d %s %lf", type, pairid, pair, lot);

	int rv = 0;
	bool is_snapshot = (type == 'S');
	bool is_top = (type == 'T');

	// clear bids and asks for new snapshot.
	if (is_snapshot)
		mkt_wss_odbk_purge(pairid);

	yyjson_val *_v;
	size_t _idx, _max;
	yyjson_arr_foreach(jdata, _idx, _max, _v) {
		yyjson_val *v;
		size_t idx, max;
		yyjson_val *orders = NULL;

		URN_RET_ON_NULL(orders = yyjson_obj_get(_v, "bids"), "No jdata[]/bids", EINVAL);
		yyjson_arr_foreach(orders, idx, max, v) {
			rv = parse_n_insert_odbk_porder(pairid, true, lot, v, is_top);
			URN_RET_ON_RV(rv, "Error in parse_n_insert_odbk_porder() for bids");
		}

		URN_RET_ON_NULL(orders = yyjson_obj_get(_v, "asks"), "No jdata[]/asks", EINVAL);
		yyjson_arr_foreach(orders, idx, max, v) {
			rv = parse_n_insert_odbk_porder(pairid, false, lot, v, is_top);
			URN_RET_ON_RV(rv, "Error in parse_n_insert_odbk_porder() for asks");
		}

		URN_RET_ON_NULL(v = yyjson_obj_get(_v, "ts"), "No ts", EINVAL);
		long ts = 0;
		sscanf(yyjson_get_str(v), "%ld", &ts);
		odbk_t_arr[pairid] = ts * 1000l;
	}
	odbk_updated(pairid);
	return 0;
}

int on_tick(int pairid, double lot, yyjson_val *jdata) {
	char* pair = pair_arr[pairid];
	URN_DEBUGF("\ton_tick %d %s", pairid, pair);
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *jval = NULL;

	yyjson_arr_foreach(jdata, idx, max, v) {
		urn_inum p, s;
		const char *side, *tstr, *pstr, *szstr;

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "ts"), "No data[]/ts", EINVAL);
		URN_RET_ON_NULL(tstr = yyjson_get_str(jval), "No ts str", EINVAL);
		long ts_e6 = 0;
		sscanf(tstr, "%ld", &ts_e6);
		ts_e6 *= 1000;

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "side"), "No side", EINVAL);
		URN_RET_ON_NULL(side = yyjson_get_str(jval), "No side str", EINVAL);
		bool buy = false;
		if (strcmp(side, "buy") == 0)
			buy = true;
		else if (strcmp(side, "sell") == 0)
			buy = false;
		else
			URN_RET_ON_NULL(NULL, "Unexpected data/side", EINVAL);

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "px"), "No px", EINVAL);
		URN_RET_ON_NULL(pstr = yyjson_get_str(jval), "No px str", EINVAL);
		urn_inum_parse(&p, pstr);

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "sz"), "No sz", EINVAL);
		URN_RET_ON_NULL(szstr = yyjson_get_str(jval), "No sz str", EINVAL);
		urn_inum_parse(&s, szstr);
		if (lot != 1)
			urn_inum_from_db(&s, lot * urn_inum_to_db(&s));

		urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);

	}
	return 0;
}

int on_instruments(yyjson_val *jdata) {
	int rv = 0;

	yyjson_val *v;
	size_t idx, max;
	yyjson_val *jval = NULL;
	urn_inum i; // parsing float ctVal from string
	yyjson_arr_foreach(jdata, idx, max, v) {
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "instId"), "No instId", EINVAL);
		const char* inst_id = yyjson_get_str(jval);
		uintptr_t pairid = 0;
		urn_hmap_getptr(symb_to_pairid, inst_id, &pairid);
		if (pairid == 0)
			continue; // Not the one in targets.

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "ctVal"), "No ctVal", EINVAL);
		const char* lot = yyjson_get_str(jval);
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "instType"), "No instType", EINVAL);
		const char* inst_type = yyjson_get_str(jval);
		if (strcmp(inst_type, "SPOT") == 0) {
			lot_arr[pairid] = 1;
			continue;
		}
		if (strcmp(lot, "") == 0) {
			URN_WARNF("No ctVal for non-SPOT pair %s %s %s", inst_type, inst_id, lot);
			continue;
		}
		urn_inum_parse(&i, lot);
		lot_arr[pairid] = urn_inum_to_db(&i);
		URN_INFOF("ctVal non-SPOT pair %s %s %s -> %lf", inst_type, inst_id, lot, lot_arr[pairid]);
	}
	return 0;
}
