// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

// local options
#define PUB_LESS_ON_ZERO_LISTENER
//#undef  PUB_LESS_ON_ZERO_LISTENER

//#define PUB_NO_REDIS
//#define WRITE_NO_SHRMEM

#include "mkt_wss.h"
#include "enc.h"

int   on_tick(char *msg, size_t msg_len);
int   on_odbk(int pairid, yyjson_val *jdata);
int   on_odbk_update(char *msg, size_t msg_len);

char *preprocess_pair(char *pair) { return pair; }

int exchange_sym_alloc(urn_pair *pair, char **str) {
	int slen = strlen(pair->name);
	*str = malloc(slen+1);
	if ((*str) == NULL) return ENOMEM;
	if (pair->expiry != NULL)
		return URN_FATAL("Pair with expiry in Gemini", EINVAL);
	// USDT-BTC   -> BTC-USDT
	sprintf(*str, "%s-%s", pair->asset, pair->currency);
	urn_s_upcase(*str, slen);
	return 0;
}

void mkt_data_set_exchange(char *s) { sprintf(s, "Bittrex"); }

void mkt_data_set_wss_url(char *s) {
	// Bittrex is not so active, wait longer.
	wss_stat_max_msg_t = 900;

	// signalR via wss: negotiate a WSS token first
	char *signalr = "https://socket-v3.bittrex.com/signalr/negotiate?connectionData=[{\"name\":\"c3\"}]&clientProtocol=1.5";
	uint16_t http_code = 0;
	char *http_res = NULL;
	size_t res_len = 0;
	nng_https_get(signalr, &http_code, &http_res, &res_len);
	if (http_code != 200) {
		URN_WARNF("Error in negotiating signalR token at %s\nHTTP RES: %s\nHTTP CODE %hu", 
			signalr, http_res, http_code);
		URN_FATAL("Abort bittrex set wss url", EINVAL);
	}
	URN_INFOF("SignalR negotiation: %s", http_res);
	/* HTTP response:
	 * "Url":"/signalr",
	 * "ConnectionToken":"6lATfsJgE1YGkBGh3Gjc",
	 * "ConnectionId":"a6f71467-9002-4d41-8cc7-22fd71fa6766",
	 * "KeepAliveTimeout":20.0,
	 * "DisconnectTimeout":30.0,
	 * "ConnectionTimeout":110.0,
	 * "TryWebSockets":true,"ProtocolVersion":"1.5","TransportConnectTimeout":5.0,"LongPollDelay":0.0}
	 */
	yyjson_doc *jdoc = yyjson_read(http_res, res_len, 0);
	yyjson_val *jroot = yyjson_doc_get_root(jdoc);
	yyjson_val *jval = yyjson_obj_get(jroot, "ConnectionToken");
	if (jval == NULL)
		URN_FATAL(http_res, EINVAL);
	const char *signalr_token = yyjson_get_str(jval);
	size_t slen = strlen(signalr_token);
	char token_escaped[slen*2];
	int j=0; // writing pos
	for(int i=0; i<slen; i++) { // URL escaping
		char c = signalr_token[i];
		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
			token_escaped[j++] = c;
		} else if (c == '+') {
			token_escaped[j++] = '%';
			token_escaped[j++] = '2';
			token_escaped[j++] = 'B';
		} else if (c == '/') {
			token_escaped[j++] = '%';
			token_escaped[j++] = '2';
			token_escaped[j++] = 'F';
		} else if (c == '\\') {
			token_escaped[j++] = '%';
			token_escaped[j++] = '5';
			token_escaped[j++] = 'C';
		} else {
			URN_FATAL(signalr_token, ERANGE);
		}
	}
	token_escaped[j++] = '\0';
	char *data_escaped = "%5B%7B%22name%22%3A%22c3%22%7D%5D";
	sprintf(s, "wss://socket-v3.bittrex.com/signalr/connect?"
		"clientProtocol=1.5&transport=webSockets&connectionToken=%s"
		"&connectionData=%s&tid=10", token_escaped, data_escaped);
final:
	yyjson_doc_free(jdoc);
	free(http_res);
}

void mkt_data_odbk_channel(char *sym, char *chn) { sprintf(chn, "orderbook_%s_25", sym); }

void mkt_data_odbk_snpsht_channel(char *sym, char *chn) { sprintf(chn, "%s", sym); }

void mkt_data_tick_channel(char *sym, char *chn) { sprintf(chn, "trade_%s", sym); }

int mkt_wss_prepare_reqs(int chn_ct, const char **odbk_chns, const char **odbk_snpsht_chns, const char**tick_chns) {
	/* SignalR req format:
		H: hubName, // c3 for bittrex
		M: methodName, // Subscribe
		A: args, // [["Heartbeat","orderbook_BTC-USDT_25","trade_BTC-USDT"]]
		I: messageId // always incr from 1
	*/
	bool req_trades = recv_trades && (odbk_shmptr != NULL);

	yyjson_mut_doc *wss_req_j = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *jroot = yyjson_mut_obj(wss_req_j);
	if (jroot == NULL)
		return URN_FATAL("Error in creating json root", EINVAL);
	yyjson_mut_doc_set_root(wss_req_j, jroot);
	yyjson_mut_obj_add_str(wss_req_j, jroot, "H", "c3");
	yyjson_mut_obj_add_str(wss_req_j, jroot, "M", "Subscribe");

	const char *channels[req_trades ? (2*chn_ct+1) : (chn_ct+1)];
	channels[0] = "Heartbeat";
	for(int i=0; i<=chn_ct; i++) {
		if (req_trades) {
			channels[2*i+1] = odbk_chns[i];
			channels[2*i+2] = tick_chns[i];
		} else
			channels[i+1] = odbk_chns[i];
	}
	yyjson_mut_val *target_chns = yyjson_mut_arr_with_strcpy(wss_req_j, channels,
		req_trades ? (2*chn_ct+1) : (chn_ct+1));
	yyjson_mut_val *a_arr = yyjson_mut_arr_with_strcpy(wss_req_j, NULL, 0);
	yyjson_mut_arr_append(a_arr,target_chns);
	yyjson_mut_obj_add_val(wss_req_j, jroot, "A", a_arr);
	yyjson_mut_obj_add_int(wss_req_j, jroot, "I", 1);

	// save command
	int cmd_ct = 0;
	char *cmd = yyjson_mut_write(wss_req_j, 0, NULL); // one-line json
	yyjson_mut_doc_free(wss_req_j);
	wss_req_s[cmd_ct] = cmd;
	URN_DEBUGF("req %d : %s", cmd_ct, cmd);
	cmd_ct++;

	URN_INFOF("Parsing ARGV end, %d req str prepared, req_trades %hhu recv_trades %hhu, odbk_shmptr %p",
		cmd_ct, req_trades, recv_trades, odbk_shmptr);
	wss_req_interval_e = 1;
	wss_req_interval_ms = 100;
	return 0;
}

// A -> decodeA -> decompressA
#define tmp_A_sz 65536
unsigned char decodeA[tmp_A_sz];
unsigned char decompressA[tmp_A_sz];

int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;

	URN_DEBUGF("on_wss_msg %zu %.*s", len, URN_MIN(1024, ((int)len)), msg);

	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	yyjson_val *m_arr = yyjson_obj_get(jroot, "M");
	if (m_arr == NULL) {
		URN_INFOF("on_wss_msg omit %.*s", (int)len, msg);
		goto final;
	} else if (yyjson_is_arr(m_arr)) {
		if (yyjson_arr_size(m_arr) == 0) {
			URN_INFOF("on_wss_msg omit %.*s", (int)len, msg);
			goto final;
		}
		// keep parsing M array.
	} else {
		URN_WARNF("M is not array: %.*s", (int)len, msg);
		rv = EINVAL;
		goto final;
	}

	// keep parsing M array.
	yyjson_val *m_el;
	size_t idx, max, processed=0;
	yyjson_arr_foreach(m_arr, idx, max, m_el) {
		// "H":"C3","M":"orderBook","A":["base64(compress(str))"]
		URN_RET_ON_NULL(jval = yyjson_obj_get(m_el, "H"), "No M/idx/H", EINVAL);
		const char *h = yyjson_get_str(jval);
		URN_RET_ON_NULL(jval = yyjson_obj_get(m_el, "M"), "No M/idx/M", EINVAL);
		const char *m = yyjson_get_str(jval);
		URN_RET_ON_NULL(jval = yyjson_obj_get(m_el, "A"), "No M/idx/A", EINVAL);
		if (h == NULL || m == NULL) {
			URN_WARNF("M/%zu null H/M str %.*s", idx, (int)len, msg);
			rv = EINVAL;
			goto final;
		} else if (strcmp(m, "heartbeat") == 0) {
			processed++;
			continue;
		} else if (!(yyjson_is_arr(m_arr))) {
			URN_WARNF("M/%zu is not array: %.*s", idx, (int)len, msg);
			rv = EINVAL;
			goto final;
		} else if (yyjson_arr_size(jval) == 0) {
			// {}
			processed++;
			continue;
		} else if (yyjson_arr_size(jval) != 1) {
			URN_WARNF("M/%zu size = %zu: %.*s", idx, yyjson_arr_size(jval), (int)len, msg);
			rv = EINVAL;
			goto final;
		}
		jval= yyjson_arr_get_first(jval);
		const char *a = yyjson_get_str(jval);
		if (a == NULL) {
			URN_WARNF("M/%zu null A str %.*s", idx, (int)len, msg);
			rv = EINVAL;
			goto final;
		}
		// decompress(decode64(A))
		size_t decoded_sz = 0;
		rv = base64_decode_to((unsigned char*)a, decodeA, &decoded_sz);
		if (rv != 0) {
			URN_WARNF("M/%zu decode64(A) error str %.*s", idx, (int)len, msg);
			goto final;
		}
		size_t inflated_sz = 0;
		rv = zlib_inflate_raw(decodeA, decoded_sz, decompressA, tmp_A_sz, &inflated_sz);
		if (rv != 0) {
			URN_WARNF("M/%zu inflate(A) error str %.*s", idx, (int)len, msg);
			goto final;
		}
		if (decompressA[inflated_sz-1] == '}') {
			decompressA[inflated_sz] = '\0';
		} else {
			// Add '}' end, sometimes inflated data missed this.
			decompressA[inflated_sz] = '}';
			inflated_sz ++;
			decompressA[inflated_sz] = '\0';
		}
		URN_DEBUGF("H %s M %s A %zu/%d %s", h, m, inflated_sz, tmp_A_sz, decompressA);
		if (strcmp(m, "orderBook") == 0) {
			rv = on_odbk_update((char *)decompressA, inflated_sz);
			processed++;
			continue;
		} else if (strcmp(m, "trade") == 0) {
			rv = on_tick((char *)decompressA, inflated_sz);
			processed++;
			continue;
		} else {
			URN_WARNF("M/%zu unknown M %.*s", idx, (int)len, msg);
			rv = EINVAL;
			goto final;
		}
	}
	if (max == processed) // All M are processed.
		goto final;

	// Unknown type
	URN_WARNF("%lu / %lu processed", processed, max);
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
	bool buy;
	int rv = 0;

	if ((*type) == 'B')
		buy = true;
	else if ((*type) == 'A')
		buy = false;
	else
		return URN_FATAL("Unexpected order type", EINVAL);

	if (op_type != 4)
		return URN_FATAL("Unexpected op_type, Bittrex only has guess type", EINVAL);

	GList *bids = bids_arr[pairid];
	GList *asks = asks_arr[pairid];

	// json in v: "quantity":"0","rate":"1052.577000000000"
	yyjson_val *jval;
	URN_RET_ON_NULL(jval = yyjson_obj_get(v, "quantity"), "No quantity", EINVAL);
	urn_inum_alloc(&s, yyjson_get_str(jval));
	URN_RET_ON_NULL(jval = yyjson_obj_get(v, "rate"), "No rate", EINVAL);
	urn_inum_alloc(&p, yyjson_get_str(jval));

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

int mkt_data_req_depth_snapshot(int pairid) {
	int rv = 0;
	// signalR via wss: negotiate a WSS token first
	char url[256];
	char *symb = symb_arr[pairid];
	sprintf(url, "https://api.bittrex.com/v3/markets/%s/orderbook?depth=25", symb);
	URN_INFOF("Depth snapshot requesting %s %s", symb, url);
	uint16_t http_code = 0;
	char *http_res = NULL;
	size_t res_len = 0;
	nng_https_get(url, &http_code, &http_res, &res_len);
	if (http_code != 200) {
		URN_WARNF("Error in requesting %s depth snapshot at %s\nHTTP RES: %s\nHTTP CODE %hu", 
			symb, url, http_res, http_code);
		goto final;
	}
	URN_INFOF("Depth snapshot %s len %lu", symb, res_len);

	/* HTTP response:
	 * {"bid":[{"quantity":"0.18500000","rate":"20333.138000000000"} ...
	 * "ask":[{"quantity":"0.18500000","rate":"20344.597000000000"},}
	 */
	yyjson_doc *jdoc = yyjson_read(http_res, res_len, 0);
	yyjson_val *jroot = yyjson_doc_get_root(jdoc);
	yyjson_val *jbid = yyjson_obj_get(jroot, "bid");
	yyjson_val *jask = yyjson_obj_get(jroot, "ask");
	if (jbid == NULL || jask == NULL) {
		URN_WARNF("No bid/ask in snapshot json %s\n%s", symb, http_res);
		rv = EINVAL;
		goto final;
	}
	rv = on_odbk(pairid, jroot);
final:
	yyjson_doc_free(jdoc);
	free(http_res);
	return rv;
}

int on_odbk(int pairid, yyjson_val *jroot) {
	int rv = 0;
	size_t idx, max;
	yyjson_val *v = NULL, *orders = NULL;

	URN_RET_ON_NULL(orders = yyjson_obj_get(jroot, "bid"), "No bid", EINVAL);
	char ask_or_bid = 0;
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'B';
		URN_GO_FINAL_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4),
				"Error in parse_n_mod_odbk_porder() for bids");
	}

	URN_RET_ON_NULL(orders = yyjson_obj_get(jroot, "ask"), "No ask", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'A';
		URN_GO_FINAL_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4),
				"Error in parse_n_mod_odbk_porder() for asks");
	}

	odbk_updated(pairid);

final:
	if (rv == 0) snapshot_init[pairid] = true;
	return rv;
}

int on_odbk_update(char *msg, size_t msg_len) {
	/*
	 * H C3 M orderBook A {
	 * "marketSymbol":"BTC-USDT","depth":25,"sequence":1233757,
	 * "bidDeltas":[{"quantity":"0.16000000","rate":"19163.179954930000"}],
	 * "askDeltas":[]}
	 */
	struct timeval odbk_mkt_t; // no mkt_t, use local time instead.
	gettimeofday(&odbk_mkt_t, NULL);

	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;

	// Parsing key values from json
	jdoc = yyjson_read(msg, msg_len, 0);
	jroot = yyjson_doc_get_root(jdoc);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "marketSymbol"), "No marketSymbol", EINVAL);
	const char *symb = yyjson_get_str(jval);
	uintptr_t pairid = 0;
	urn_hmap_getptr(symb_to_pairid, symb, &pairid);
	if (pairid == 0) {
		rv = ERANGE;
		URN_WARNF("Symbol %s not found", symb);
		goto final;
	}
	odbk_t_arr[pairid] = (long)(odbk_mkt_t.tv_sec) * 1000000l + (long)(odbk_mkt_t.tv_usec);
	URN_DEBUGF("\ton_odbk_update %d %s", (int)pairid, symb);
	if (!snapshot_init[pairid])
		mkt_data_req_depth_snapshot(pairid);

	size_t idx, max;
	yyjson_val *v = NULL, *orders = NULL;

	URN_RET_ON_NULL(orders = yyjson_obj_get(jroot, "bidDeltas"), "No bidDeltas", EINVAL);
	char ask_or_bid = 0;
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'B';
		URN_GO_FINAL_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4),
				"Error in parse_n_mod_odbk_porder() for bids");
	}

	URN_RET_ON_NULL(orders = yyjson_obj_get(jroot, "askDeltas"), "No askDeltas", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		ask_or_bid = 'A';
		URN_GO_FINAL_ON_RV(parse_n_mod_odbk_porder(pairid, &ask_or_bid, v, 4),
				"Error in parse_n_mod_odbk_porder() for asks");
	}

	odbk_updated(pairid);

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}

struct tm _bittrex_tm;
int on_tick(char *msg, size_t msg_len) {
	/*
	 * {"deltas":[{
	 * "id":"02bcce7f-05a5-4e5c-935e-73d958d5c9c3",
	 * "executedAt":"2022-07-06T18:58:20.09Z",
	 * "quantity":"0.00011793","rate":"20348.124000000000",
	 * "takerSide":"SELL"
	 * }],"sequence":8969,"marketSymbol":"BTC-USD"}
	 */
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL, *jval = NULL, *jtrades = NULL, *v = NULL;

	// Parsing key values from json
	jdoc = yyjson_read(msg, msg_len, 0);
	jroot = yyjson_doc_get_root(jdoc);
	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "marketSymbol"), "No marketSymbol", EINVAL);
	const char *symb = yyjson_get_str(jval);
	uintptr_t pairid = 0;
	urn_hmap_getptr(symb_to_pairid, symb, &pairid);
	if (pairid == 0) {
		rv = ERANGE;
		URN_WARNF("Symbol %s not found", symb);
		goto final;
	}

	URN_RET_ON_NULL(jtrades = yyjson_obj_get(jroot, "deltas"), "No deltas", EINVAL);
	size_t idx, max;
	long latest_ts_e6 = 0;
	char ask_or_bid = 0;
	yyjson_arr_foreach(jtrades, idx, max, v) {
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "executedAt"), "No deltas/executedAt", EINVAL);
		const char *tstr = yyjson_get_str(jval);
		URN_RET_ON_NULL(tstr, "No deltas/executedAt str", EINVAL);
		long ts_e6 = parse_timestr_w_e6(&_bittrex_tm, tstr, "%Y-%m-%dT%H:%M:%S.");
		if (ts_e6 > latest_ts_e6)
			latest_ts_e6 = ts_e6;

		urn_inum p, s;
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "quantity"), "No deltas/quantity", EINVAL);
		URN_RET_ON_NULL(yyjson_get_str(jval), "No deltas/quantity str", EINVAL);
		urn_inum_parse(&s, yyjson_get_str(jval));
		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "rate"), "No deltas/rate", EINVAL);
		URN_RET_ON_NULL(yyjson_get_str(jval), "No deltas/rate str", EINVAL);
		urn_inum_parse(&p, yyjson_get_str(jval));

		URN_RET_ON_NULL(jval = yyjson_obj_get(v, "takerSide"), "No deltas/rate", EINVAL);
		const char *side = yyjson_get_str(jval);
		URN_RET_ON_NULL(side, "No deltas/takerSide str", EINVAL);
		bool buy = false;
		if (strcmp(side, "BUY") == 0)
			buy = true;
		else if (strcmp(side, "SELL") == 0)
			buy = false;
		else
			URN_RET_ON_NULL(NULL, "Unexpected deltas/takerSide", EINVAL);
		urn_tick_append(&(odbk_shmptr->ticks[pairid]), buy, &p, &s, ts_e6);
	}
	// Display market latency, only in this case.
	// Dont set to mkt_wss, bittrex is not active at all.
	clock_gettime(CLOCK_REALTIME, &_tmp_clock);
	long latency_ms = _tmp_clock.tv_sec * 1000l +
		_tmp_clock.tv_nsec/1000000l - latest_ts_e6/1000l;
	URN_LOGF("Bittrex %s %lu trades, +%ldms", pair_arr[pairid], max, latency_ms);

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}
