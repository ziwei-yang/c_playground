#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// uranus options
#undef URN_WSS_DEBUG // wss I/O log
#define URN_MAIN_DEBUG // debug log
#undef URN_MAIN_DEBUG // off debug log

// local options
#define MAX_DEPTH 5 // max depth in each side.
#define MAX_PAIRS 300
#define TSMS_LEN 13 // ms timestamp str len
#define PUB_REDIS
//#undef PUB_REDIS // off redis publish

#include "mkt_wss.h"

int   on_odbk(int pairid, const char *type, yyjson_val *jdata);
int   on_odbk_update(int pairid, const char *type, yyjson_val *jdata);

void mkt_data_set_exchange(char *s) {
	sprintf(s, "BybitU"); // TODO getenv
}

void mkt_data_set_wss_url(char *s) {
	// "wss://stream.bybit.com/realtime"; // Bybit Coin M
	sprintf(s, "wss://stream.bybit.com/realtime_public"); // Bybit USDT M
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

	// request successful ack.
	URN_GO_FINAL_IF(yyjson_obj_get(jroot, "request") != NULL, "omit request message");

	URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "topic"), "No topic", EINVAL);
	const char *topic = yyjson_get_str(jval);

	URN_RET_ON_NULL(jcore_data = yyjson_obj_get(jroot, "data"), "No data", EINVAL);

	// Most cases: depth channel
	uintptr_t pairid = 0;
	urn_hmap_get(depth_chn_to_pairid, topic, &pairid);
	if (pairid != 0) {
		char *depth_pair = pair_arr[pairid];
		URN_DEBUGF("depth_pair id %lu %s for topic %s", pairid, depth_pair, topic);

		// must have timestamp_e6 and type in depth message.
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "type"), "No type", EINVAL);
		const char *type = yyjson_get_str(jval);
		URN_RET_ON_NULL(jval = yyjson_obj_get(jroot, "timestamp_e6"), "No timestamp_e6", EINVAL);
		const char *ts_e6 = yyjson_get_str(jval);
		newodbk_arr[pairid] ++;
		odbk_t_arr[pairid] = strtol(ts_e6, NULL, 10);
		wss_stat_mkt_ts = odbk_t_arr[pairid];

		if (depth_pair == NULL) {
			URN_WARNF("NO depth_pair id %lu for topic %s", pairid, topic);
			URN_GO_FINAL_ON_RV(EINVAL, topic);
		} else if (strcmp(type, "snapshot") == 0) {
			URN_DEBUGF("\t odbk pair snapshot %lu %s", pairid, depth_pair);
			URN_GO_FINAL_ON_RV(on_odbk(pairid, type, jcore_data), "Err in odbk handling")
		} else if (strcmp(type, "delta") == 0) {
			URN_DEBUGF("\t odbk pair delta    %lu %s", pairid, depth_pair);
			URN_GO_FINAL_ON_RV(on_odbk_update(pairid, type, jcore_data), "Err in odbk handling")
		} else
			URN_GO_FINAL_ON_RV(EINVAL, type);
		goto final;
	}

	// If not depth chn, might be trade chn.
	urn_hmap_get(trade_chn_to_pairid, topic, &pairid);
	if (pairid != 0) {
		char *trade_pair = pair_arr[pairid];
		URN_DEBUGF("trade_pair id %lu %s for topic %s", pairid, trade_pair, topic);
		URN_DEBUGF("\t trade data %s", trade_pair);
		goto final;
	}

	// Unknown topic
	URN_GO_FINAL_ON_RV(EINVAL, topic);

final:
	if (jdoc != NULL) yyjson_doc_free(jdoc);
	return rv;
}

// op_type: 0 order_book - snapshot
// op_type: 1 insert     - delta
// op_type: 2 update     - delta
// op_type: 3 delete     - delta
int parse_odbk_porder(int pairid, const char *type, yyjson_val *v, int op_type) {
	urn_inum *p, *s=NULL; // must free by hand if insertion failed.
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
	if (op_type != 3) // deletion has no size
		URN_RET_ON_NULL(sizev, "No size", EINVAL);

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
			sprintf(sizestr2, "%f", sizef);
			urn_inum_alloc(&s, sizestr2);
		} else if (sizei != 0) {
			sprintf(sizestr2, "%d", sizei);
			urn_inum_alloc(&s, sizestr2);
		}
		URN_DEBUGF("OP %d %s %s %s %s sizes %s %s %f %d", op_type, price, symbol, side, yyjson_get_type_desc(sizev), sizestr, sizestr2, sizef, sizei);
	} else
		URN_DEBUGF("OP %d %s %s %s no size", op_type, price, symbol, side);

	if (strcmp(side, "Buy") == 0)
		buy = 1;
	else if (strcmp(side, "Sell") == 0)
		buy = 0;
	else
		URN_RET_ON_NULL(NULL, side, EINVAL);

	urn_inum_alloc(&p, price);

	// Insertion, deletion and overwriting.
	if (op_type == 2 || op_type == 3) { // update or delete
		GList *node = buy ? bids : asks;
		bool found = false;
		int check_idx = -1; // -1 means unchecked
		// Find node with target price
		while(true) {
			if (node->next == NULL && node->data == NULL)
				break;
			if (node->next != NULL && node->data == NULL)
				URN_RET_IF(true, "GList->next is not NULL but ->data is NULL", EINVAL);

			check_idx++;
			int cmpv = urn_inum_cmp(p, ((urn_porder *)(node->data))->p);
			found = (cmpv == 0);
			URN_DEBUGF("cmp %s %s = %d",
				urn_inum_str(p),
				urn_inum_str(((urn_porder *)(node->data))->p),
				cmpv
			);
			if ((buy) && cmpv < 0) break; // bids: p smaller than tail.
			if ((!buy) && cmpv > 0) break; // asks: p higher than tail.
			if (found) break;

			if (node->next == NULL) break;
			node = node->next;
		}
		if (!found) {
			URN_DEBUGF("node not found at check_idx %d", check_idx);
			if (check_idx == 0) {
				URN_DEBUG("price checked but out of valid range, goto final");
				if (p != NULL) free(p);
				if (s != NULL) free(s);
				goto final;
			}
			if (op_type == 2) op_type = 1; // update -> insert
			goto insertion;
		}
		// found node with same price.
		if (op_type == 2) { // update p and s only
			urn_porder *o = (urn_porder *)(node->data);
			urn_porder_free(o);
			o = urn_porder_alloc(NULL, p, s, buy, NULL);
			node->data = o;
			URN_DEBUGF_C(BLUE, "update the node %p s %s p %s", node, urn_inum_str(o->s), urn_inum_str(o->p));
		} else if (op_type == 3) { // delete this node.
			urn_porder *o = (urn_porder *)(node->data);
			// free node->data first.
			if (node->data != NULL) {
				URN_DEBUGF_C(RED, "delete the node %p s %s p %s", node, urn_inum_str(o->s), urn_inum_str(o->p));
				urn_porder_free(o);
			} else
				URN_DEBUGF_C(RED, "Delete the node %p NULL data", node);
			node->data = NULL;

			if (node->prev == NULL && node->next == NULL) {
				// node is the only one element, delete data only.
				URN_DEBUGF_C(RED, "clear data only for node %p", node);
			} else if (node->prev == NULL || node == bids_arr[pairid] || node == asks_arr[pairid]) {
				// node is at head, remains after
				URN_DEBUGF_C(RED, "free node %p as head", node);
				if (buy)  bids = node->next;
				if (!buy) asks = node->next;
				g_list_free_1(node);
			} else if (node->next == NULL) { // node is at end
				URN_DEBUGF_C(RED, "free node %p as tail", node);
				node->prev->next = NULL;
				g_list_free_1(node);
			} else { // node is in middle, connect prev<->next
				URN_DEBUGF_C(RED, "free node %p as middle", node);
				node->prev->next = node->next;
				node->next->prev = node->prev;
				g_list_free_1(node);
			}
			if (buy)  bidct_arr[pairid]--;
			if (!buy) askct_arr[pairid]--;
		}
		goto final;
	}
insertion:
	if (op_type == 0 || op_type == 1) { // snapshot or insert
		urn_porder *o = urn_porder_alloc(NULL, p, s, buy, NULL);
		if (buy) { // bids from low to high, reversed
			if (bids->data == NULL) {
				bids->data = o;
				bidct_arr[pairid]++;
				goto final;
			}
			bids = g_list_insert_sorted(bids, o, urn_porder_px_cmp);
			URN_DEBUGF_C(GREEN, "\t %s %s insert bids %p->%p o %p", urn_inum_str(p), urn_inum_str(s), bids, bids->data, o);
			if (bidct_arr[pairid]+1 > MAX_DEPTH)
				bids = remove_top_porder(bids);
			else
				bidct_arr[pairid]++;
		} else { // asks from high to low, reversed
			if (asks->data == NULL) {
				asks->data = o;
				askct_arr[pairid]++;
				goto final;
			}
			asks = g_list_insert_sorted(asks, o, urn_porder_px_cmprv);
			URN_DEBUGF_C(GREEN, "\t %s %s insert asks %p->%p o %p", urn_inum_str(p), urn_inum_str(s), asks, asks->data, o);
			if (askct_arr[pairid]+1 > MAX_DEPTH)
				asks = remove_top_porder(asks);
			else
				askct_arr[pairid]++;
		}
	}
final:
	bids_arr[pairid] = bids;
	bids->prev = NULL;
	asks_arr[pairid] = asks;
	asks->prev = NULL;
	print_odbk(pairid);
	return 0;
}

int on_odbk(int pairid, const char *type, yyjson_val *jdata) {
	URN_DEBUGF("on_odbk %d %s", pairid, type);
	char* pair = pair_arr[pairid];
	int rv = 0;

	// clear bids and asks for new snapshot.
	g_list_free_full(bids_arr[pairid], urn_porder_free);
	g_list_free_full(asks_arr[pairid], urn_porder_free);
	bids_arr[pairid] = g_list_alloc();
	bids_arr[pairid]->prev = NULL;
	bids_arr[pairid]->next = NULL;
	asks_arr[pairid] = g_list_alloc();
	asks_arr[pairid]->prev = NULL;
	asks_arr[pairid]->next = NULL;
	askct_arr[pairid] = 0;
	bidct_arr[pairid] = 0;

	// Parse snapshot data
	yyjson_val *orders = NULL;
	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "order_book"), "No jdata/order_book", EINVAL);
	yyjson_val *v;
	size_t idx, max;
	yyjson_arr_foreach(orders, idx, max, v) {
		URN_RET_ON_RV(parse_odbk_porder(pairid, type, v, 0), "Error in parse_odbk_porder()");
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
		URN_RET_ON_RV(parse_odbk_porder(pairid, type, v, 3), "Error in parse_odbk_porder() to delete");
	}

	// update
	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "update"), "No jdata/update", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		URN_RET_ON_RV(parse_odbk_porder(pairid, type, v, 2), "Error in parse_odbk_porder() to update");
	}

	// insert
	URN_RET_ON_NULL(orders = yyjson_obj_get(jdata, "insert"), "No jdata/insert", EINVAL);
	yyjson_arr_foreach(orders, idx, max, v) {
		URN_RET_ON_RV(parse_odbk_porder(pairid, type, v, 1), "Error in parse_odbk_porder() to insert");
	}

	return 0;
}
