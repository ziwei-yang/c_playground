#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <mbedtls/mbedtls_config.h>
#include <nng/nng.h>
#include <nng/supplemental/tls/tls.h>

#include "yyjson.c"

#include "urn.h"
#include "order.h"
#include "wss.h"
#include "hmap.h"

// options
#undef URN_WSS_DEBUG
#define MAX_DEPTH 10 // max depth in each side.

int parse_args_build_idx(int argc, char **argv);
int to_bybit_sym(urn_pair *pair, char **str);
int wss_connect();
int on_wss_msg(char *msg, size_t len);
int on_odbk(const char *pair, const char *type, yyjson_val *jdata);
int on_odbk_update(const char *pair, const char *type, yyjson_val *jdata);

///////////// pair - symbol - channel hmap //////////////
hashmap* pair_to_sym;
hashmap* sym_to_pair;
hashmap* trade_chn_to_pair;
hashmap* depth_chn_to_pair;

///////////// data organized by pairid //////////////
char   **pair_arr; // pair = pair_arr[pairid]
hashmap* trade_chn_to_pairid;
hashmap* depth_chn_to_pairid;
urn_porder **bids_arr; // bids = bids_arr[pairid]
urn_porder **asks_arr; // asks = asks_arr[pairid]

char*    wss_req_s;

int main(int argc, char **argv) {
	int rv = 0;
	if ((rv = parse_args_build_idx(argc, argv)) != 0)
		return URN_FATAL("Error in parse_args_build_idx()", rv);

	URN_INFOF("nng_tls_engine_name %s", nng_tls_engine_name());
	URN_INFOF("nng_tls_engine_description %s", nng_tls_engine_description());
	if ((rv = wss_connect()) != 0)
		return URN_FATAL("Error in wss_connect()", rv);

	// values are upcase_s
	urn_hmap_free_with_keys(depth_chn_to_pair);
	urn_hmap_free_with_keys(trade_chn_to_pair);
	// same key/val in sym_to_pair and pair_to_sym
	urn_hmap_free_with_keyvals(sym_to_pair, free);
	urn_hmap_free(pair_to_sym);
	urn_hmap_free(depth_chn_to_pairid);
	urn_hmap_free(trade_chn_to_pairid);
	free(pair_arr);
	free(bids_arr);
	free(asks_arr);
	free(wss_req_s);
	return 0;
}

int wss_connect(urn_func_opt opt) {
	int rv = 0;
//	char *uri = "wss://stream.bybit.com/realtime"; // Bybit Coin M
	char *uri = "wss://stream.bybit.com/realtime_public"; // Bybit USDT M
//	char *uri = "wss://stream.binance.com:9443/ws/btcusdt@depth";
//	char *uri = "wss://ftx.com/ws/";
//	char *uri = "ws://localhost:8080/ws";
	nng_aio *dialer_aio = NULL;
	nng_stream_dialer *dialer = NULL;
	nng_stream *stream = NULL;

	int recv_buflen = 256*1024;
	char *recv_buf = malloc(recv_buflen);
	if (recv_buf == NULL)
		return URN_FATAL("Not enough memory for wss recv_buf", ENOMEM);

	nng_aio *recv_aio = NULL;
	nng_iov *recv_iov = NULL;
	size_t recv_bytes = 0;

	URN_DEBUG("Init wss conn");
	if ((rv = urn_ws_init(uri, &dialer, &dialer_aio)) != 0)
		return URN_FATAL_NNG(rv);

	URN_DEBUG("Wait conn stream");
	if ((rv = urn_ws_wait_stream(dialer_aio, &stream)) != 0)
		return URN_FATAL_NNG(rv);
	URN_DEBUG("Wss conn stream is ready");

	char *req_s = wss_req_s;
	URN_DEBUGF("Sending wss_req_s %lu bytes", strlen(req_s));
	if ((rv = urn_ws_send_sync(stream, req_s)) != 0)
		return URN_FATAL_NNG(rv);

	// Reuse iov buffer and aio to receive data.
	if ((rv = nngaio_init(recv_buf, recv_buflen, &recv_aio, &recv_iov)) != 0)
		return URN_FATAL_NNG(rv);
	URN_DEBUG("Recv aio and iov ready");

	int msg_ct = 0;
	while(1) {
#ifdef URN_WSS_DEBUG
		if ((rv = nngaio_recv_wait_res(stream, recv_aio, recv_iov, &recv_bytes, "<-- recv wait", "<-- recv poll")) != 0)
#else
		if ((rv = nngaio_recv_wait_res(stream, recv_aio, recv_iov, &recv_bytes, NULL, NULL)) != 0)
#endif
			return rv;
		msg_ct ++;
		// only 0..recv_bytes is message received this time.
		rv = on_wss_msg(recv_buf, recv_bytes);
		if (rv != 0) {
			URN_WARNF("Error in processing wss msg %.*s", (int)recv_bytes, recv_buf);
			return rv;
		}
	}
	URN_INFOF("wss_connect finished at msg_ct %d", msg_ct);
	return 0;
}

int on_wss_msg(char *msg, size_t len) {
	int rv = 0;
	yyjson_doc *jdoc = NULL;
	yyjson_val *jroot = NULL;
	yyjson_val *jval = NULL;
	yyjson_val *jcore_data = NULL;
//	URN_DEBUGF("on_wss_msg %zu %.*s", len, (int)len, msg);

#ifdef URN_WSS_DEBUG
	URN_DEBUGF("on_wss_msg %zu %.*s", len, (int)len, msg);
#endif
	// Parsing key values from json
	jdoc = yyjson_read(msg, len, 0);
	jroot = yyjson_doc_get_root(jdoc);

	// request successful ack.
	URN_GO_FINAL_IF(yyjson_obj_get(jroot, "request") != NULL, "omit request message");

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "topic"), "No topic", EINVAL);
	const char *topic = yyjson_get_str(jval);

	URN_RET_ON_NULL(jcore_data = yyjson_obj_get(jroot, "data"), "No data", EINVAL);


	// Send to sub-func
	uintptr_t pairid = 0;
	urn_hmap_get(depth_chn_to_pairid, topic, &pairid);
	if (pairid != 0) {
		pairid -= 1; // restore to correct value
		char *depth_pair = pair_arr[pairid];
		URN_LOGF("depth_pair id %lu %s for topic %s", pairid, depth_pair, topic);

		// must have timestamp_e6 and type in depth message.
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "type"), "No type", EINVAL);
		const char *type = yyjson_get_str(jval);
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "timestamp_e6"), "No timestamp_e6", EINVAL);
		const char *ts_e6 = yyjson_get_str(jval);

		if (depth_pair == NULL) {
			URN_WARNF("NO depth_pair id %lu for topic %s", pairid, topic);
			URN_GO_FINAL_ON_RV(EINVAL, topic);
		} else if (strcmp(type, "snapshot") == 0) {
			URN_DEBUGF("\t odbk pair snapshot %s", depth_pair);
//			URN_GO_FINAL_ON_RV(on_odbk(depth_pair, type, jcore_data), "Err in odbk handling")
		} else if (strcmp(type, "delta") == 0) {
			URN_DEBUGF("\t odbk pair delta    %s", depth_pair);
//			URN_GO_FINAL_ON_RV(on_odbk_update(depth_pair, type, jcore_data), "Err in odbk handling")
		} else
			URN_GO_FINAL_ON_RV(EINVAL, type);
	}
	urn_hmap_get(trade_chn_to_pairid, topic, &pairid);
	if (pairid != 0) {
		pairid -= 1; // restore to correct value
		char *trade_pair = pair_arr[pairid];
		URN_LOGF("trade_pair id %lu %s for topic %s", pairid, trade_pair, topic);
		URN_DEBUGF("\t trade data %s", trade_pair);
	}

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}

int on_odbk(const char *pair, const char *type, yyjson_val *jdata) {
	yyjson_val *orders = NULL;
	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "order_book"), "No jdata/order_book", EINVAL);

	yyjson_val *pricev, *symbolv, *sidev, *sizev, *o;
	const char *price, *symbol, *side;
	char sizestr[32];
	urn_inum *p, *s;
	_Bool buy;
	double size;
	size_t idx, max;
	yyjson_arr_foreach(orders, idx, max, o) {
		// "price":"23900.00","symbol":"BTCUSDT",
		// "id":"239000000","side":"Buy","size":1.705
		URN_RET_ON_NULL(pricev = yyjson_obj_get(o, "price"), "No price", EINVAL);
		URN_RET_ON_NULL(symbolv= yyjson_obj_get(o, "symbol"), "No symbol", EINVAL);
		URN_RET_ON_NULL(sidev  = yyjson_obj_get(o, "side"), "No side", EINVAL);
		URN_RET_ON_NULL(sizev  = yyjson_obj_get(o, "size"), "No size", EINVAL);
		price = yyjson_get_str(pricev);
		symbol= yyjson_get_str(symbolv);
		side  = yyjson_get_str(sidev);
		size  = yyjson_get_real(sizev);
		sprintf(sizestr, "%lf", size);
		URN_LOGF("%s %s %s %s", price, symbol, side, sizestr);
		urn_inum_alloc(&p, price);
		urn_inum_alloc(&s, sizestr);
		if (strcmp(side, "Buy") == 0)
			buy = 1;
		else if (strcmp(side, "Sell") == 0)
			buy = 0;
		else
			URN_RET_ON_NULL(NULL, side, EINVAL);
		urn_porder *o = urn_porder_alloc(NULL, p, s, buy, NULL);
		URN_LOGF_C(GREEN, "\t %s %s %d", urn_inum_str(p), urn_inum_str(s), buy);
	}

	return 0;
}

int on_odbk_update(const char *pair, const char *type, yyjson_val *jdata) {
	return 0;
}

int parse_args_build_idx(int argc, char **argv) {
	const int max_pairn = argc;
	int rv = 0;
	if (argc < 2)
		return URN_FATAL("No target pair supplied!", 1);
	URN_INFO("Parsing ARGV start");

	pair_to_sym = urn_hmap_init(max_pairn*5); // to avoid resizing.
	sym_to_pair = urn_hmap_init(max_pairn*5);
	depth_chn_to_pair = urn_hmap_init(max_pairn*5);
	trade_chn_to_pair = urn_hmap_init(max_pairn*5);

	pair_arr = malloc(argc);
	bids_arr = malloc(argc);
	asks_arr = malloc(argc);
	bids_arr = malloc(argc);
	asks_arr = malloc(argc);
	depth_chn_to_pairid = urn_hmap_init(max_pairn*5);
	trade_chn_to_pairid = urn_hmap_init(max_pairn*5);

	int chn_ct = 0;
	const char *byb_chns[max_pairn*2]; // both depth and trade

	urn_pair pairs[argc-1];
	for (int i=1; i<argc; i++) {
		gchar *upcase_s = g_ascii_strup(argv[i], strlen(argv[i]));

		// Check dup args
		char *dupval;
		int dup = urn_hmap_getstr(pair_to_sym, upcase_s, &dupval);
		if (dup) {
			URN_WARNF("Duplicated arg[%d] %s <-> %s", i, upcase_s, dupval);
			continue;
		}
		URN_LOGF("%d -->    %s", i-1, upcase_s);

		// parsing urn_pair
		urn_pair pair = pairs[i-1];
		rv = urn_pair_parse(&pair, argv[i], strlen(argv[i]), NULL);
		if (rv != 0)
			return URN_FATAL("Error in parsing pairs", EINVAL);
		if (pair.expiry == NULL)
			return URN_FATAL("Pair must have expiry", EINVAL);
		if (strcmp(pair.expiry, "P") != 0)
			return URN_FATAL("Pair expiry must be P", EINVAL);
		// urn_pair_print(pair);

		char *byb_sym = NULL;
		if ((rv = to_bybit_sym(&pair, &byb_sym)) != 0) {
			URN_WARNF("Invalid pair for bybit arg[%d] %s", i, pair.name);
			continue;
		}
		URN_LOGF("bybu sym %s", byb_sym);

		// Prepare symbol <-> pair map.
		urn_hmap_setstr(pair_to_sym, upcase_s, byb_sym);
		urn_hmap_setstr(sym_to_pair, byb_sym, upcase_s);

		// Prepare channels.
		char *depth_chn = malloc(20+strlen(byb_sym));
		char *trade_chn = malloc(20+strlen(byb_sym));
		if (depth_chn == NULL || trade_chn == NULL)
			return URN_FATAL("Not enough memory for creating channel str", ENOMEM);
		sprintf(depth_chn, "orderBookL2_25.%s", byb_sym);
		sprintf(trade_chn, "trade.%s", byb_sym);
		byb_chns[chn_ct] = depth_chn;
		byb_chns[chn_ct+1] = trade_chn;
		urn_hmap_setstr(depth_chn_to_pair, depth_chn, upcase_s);
		urn_hmap_setstr(trade_chn_to_pair, trade_chn, upcase_s);

		uintptr_t pairid = chn_ct/2;
		pair_arr[pairid] = upcase_s;
		urn_hmap_set(depth_chn_to_pairid, depth_chn, pairid+1); // dont put 0=NULL into hmap
		urn_hmap_set(trade_chn_to_pairid, trade_chn, pairid+1); // dont put 0=NULL into hmap
		URN_LOGF("\targv %d chn %d %s", i, chn_ct, depth_chn);
		URN_LOGF("\targv %d chn %d %s", i, chn_ct+1, trade_chn);
		chn_ct += 2;
	}

	URN_INFO("parsing end, building index now");

	urn_hmap_print(pair_to_sym, "pair_to_sym");
	urn_hmap_print(sym_to_pair, "sym_to_pair");
	urn_hmap_print(depth_chn_to_pair, "depth_to_pair");
	urn_hmap_print(trade_chn_to_pair, "trade_to_pair");
	urn_hmap_printi(depth_chn_to_pairid, "depth_to_pairid");
	urn_hmap_printi(trade_chn_to_pairid, "trade_to_pairid");

	URN_LOGF("Generate req json with %d channels", chn_ct);
	yyjson_mut_doc *wss_req_j = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *jroot = yyjson_mut_obj(wss_req_j);
	if (jroot == NULL)
		return URN_FATAL("Error in creating json root", EINVAL);
	yyjson_mut_doc_set_root(wss_req_j, jroot);
	yyjson_mut_obj_add_str(wss_req_j, jroot, "op", "subscribe");
	for (int i=0; i<chn_ct; i++)
		URN_LOGF("\tchn %d %s", i, byb_chns[i]);
	yyjson_mut_val *target_chns = yyjson_mut_arr_with_strcpy(wss_req_j, byb_chns, chn_ct);
	if (target_chns == NULL)
		return URN_FATAL("Error in creating channel json array", EINVAL);
	yyjson_mut_obj_add_val(wss_req_j, jroot, "args", target_chns);

	wss_req_s = yyjson_mut_write(wss_req_j, YYJSON_WRITE_PRETTY, NULL);
	URN_LOGF("req str: %s", wss_req_s);
	free(wss_req_s);
	wss_req_s = yyjson_mut_write(wss_req_j, 0, NULL); // one-line json
	URN_INFOF("req str: %s", wss_req_s);
	yyjson_mut_doc_free(wss_req_j);

	URN_INFO("Parsing ARGV end");
	return 0;
}

int to_bybit_sym(urn_pair *pair, char **str) {
	*str = malloc(strlen(pair->name));
	if ((*str) == NULL) return ENOMEM;
	sprintf(*str, "%s%s", pair->asset, pair->currency);
	return 0;
}
