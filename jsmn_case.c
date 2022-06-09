#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "urn.c"
#include "wss.c"

#include "jsmn.h"

int main(int argc, char **argv) {
//	char fpath[] = "./example.json";
	char fpath[] = "./pos_trader_SLP.json";
	char *contents;
	gsize clen;
	GError *error;

	char *type_desc[9] = {
		"0",
		"{}",
		"[]",
		"3",
		"\"\"",
		"5",
		"6",
		"7",
		"primitive"
	};
	char preview[50];
	preview[49] = '\0';

	URN_LOG("Loading file");
	gboolean ret = g_file_get_contents(fpath, &contents, &clen, &error);
	URN_LOGF("ret  %d len %lu\n", ret, clen);
//	URN_INFO(contents);

	URN_LOGF("Testing json, size %lu", clen);
	jsmn_parser parser;
	jsmn_init(&parser);
	int rv = 0;
	rv = jsmn_parse(&parser, contents, clen, NULL, 0);
	if (rv < 0)
		return URN_FATAL("Error in parsing json", rv);


	int max_json_t = rv; // rv is the number of tokens actually used by the parser.
	jsmntok_t tokens[max_json_t];

	URN_LOGF("Parsing json 100 times, need token array of size %d", rv);
	for(int i=0; i<100; i++) {
		jsmn_init(&parser);
		rv = jsmn_parse(&parser, contents, clen, tokens, max_json_t);
		if (rv < 0)
			return URN_FATAL("Error in parsing json", rv);
	}

	URN_INFOF("%d tokens parsed from %s", rv, fpath);
	exit(0);
	for (int i=0; i< rv; i++) {
		jsmntok_t token = tokens[i];
		int cplen = URN_MIN(token.size, 49);
		cplen = URN_MIN((token.end - token.start), 49);
		memcpy(preview, contents+token.start, cplen);
		preview[cplen] = '\0';
		URN_LOGF("%9s token type, %d -> %d, len(%d) %s %s %s", type_desc[token.type], token.start, token.end, token.size, CLR_GREEN, preview, CLR_RST);
	}
}
