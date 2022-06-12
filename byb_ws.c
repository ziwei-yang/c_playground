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

int parse_args_build_idx(int argc, char **argv);
int to_bybit_sym(urn_pair *pair, char **str);
int wss_connect();

hashmap* pair_to_sym;
hashmap* sym_to_pair;
hashmap* trade_chn_to_pair;
hashmap* depth_chn_to_pair;

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
	free(wss_req_s);
	return 0;
}

int wss_connect() {
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

	while(1) {
		if ((rv = nngaio_recv_wait_res(stream, recv_aio, recv_iov, &recv_bytes, "<-- recv wait", "<-- recv poll")) != 0)
			return rv;
		// only 0..recv_bytes is message received this time.
		URN_INFOF("%zu %.*s", recv_bytes, (int)recv_bytes, recv_buf);
	}
	return 0;
}

int parse_args_build_idx(int argc, char **argv) {
	const int max_pairn = argc;
	int rv = 0;
	if (argc < 2)
		return URN_FATAL("No target pair supplied!", 1);
	URN_INFO("Parsing ARGV start");

	pair_to_sym = urn_hmap_init(max_pairn*2); // *2 to avoid resizing.
	sym_to_pair = urn_hmap_init(max_pairn*2);
	depth_chn_to_pair = urn_hmap_init(max_pairn*2);
	trade_chn_to_pair = urn_hmap_init(max_pairn*2);

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
		URN_LOGF("\targv %d chn %d %s", i, chn_ct, depth_chn);
		URN_LOGF("\targv %d chn %d %s", i, chn_ct+1, trade_chn);
		chn_ct += 2;
	}

	URN_INFO("parsing end, building index now");

	urn_hmap_print(pair_to_sym, "pair_to_sym");
	urn_hmap_print(sym_to_pair, "sym_to_pair");
	urn_hmap_print(depth_chn_to_pair, "depth_to_pair");
	urn_hmap_print(trade_chn_to_pair, "trade_to_pair");

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
