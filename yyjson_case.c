#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "urn.h"
#include "wss.h"

#include "yyjson.h"
#include "yyjson.c"

int main(int argc, char **argv) {
	char fpath[] = "./example.json";
//	char fpath[] = "./pos_trader_SLP.json";
	char *contents;
	gsize clen;
	GError *error;

	URN_LOG("Loading file");
	gboolean ret = g_file_get_contents(fpath, &contents, &clen, &error);
	URN_LOGF("ret  %d len %lu\n", ret, clen);
//	URN_INFO(contents);
	
	yyjson_doc *doc;
	URN_LOG("parsing 100 times");
	for(int i=0; i<100; i++) {
		doc = yyjson_read(contents, clen, 0);
	}
	URN_LOG("parsing finished");
	yyjson_val *root = yyjson_doc_get_root(doc);

	yyjson_val *val = yyjson_obj_get(root, "e");
	printf("e: %s\n", yyjson_get_str(val));
	printf("elength:%d\n", (int)yyjson_get_len(val));

	val = yyjson_obj_get(root, "b");
	size_t b_idx, b_max;
	yyjson_val *rec;
	yyjson_arr_foreach(val, b_idx, b_max, rec) {
		yyjson_val *sub_val1 = yyjson_arr_get_first(rec);
		yyjson_val *sub_val2 = unsafe_yyjson_get_next(sub_val1);
		printf("buy %d/%d: %s %s\n", (int)b_idx, (int)b_max, sub_val1->uni.str, sub_val2->uni.str);
	}

	const char *byb_chns[20]; // both depth and trade
	byb_chns[0] = "chn0";
	byb_chns[1] = "chn1";
	size_t chn_ct = 2;

	yyjson_mut_doc *wss_req_j;
	wss_req_j = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *jroot = yyjson_mut_obj(wss_req_j);
	yyjson_mut_doc_set_root(wss_req_j, jroot);
	yyjson_mut_obj_add_str(wss_req_j, jroot, "op", "subscribe");

	yyjson_mut_val *target_chns = yyjson_mut_arr_with_strcpy(wss_req_j, byb_chns, chn_ct);
	yyjson_mut_obj_add_val(wss_req_j, jroot, "target", target_chns);

	char *json = yyjson_mut_write(wss_req_j, YYJSON_WRITE_PRETTY, NULL);
	printf("JSON:%s\n", json);
	free(json);

	URN_INFO("END");
}

