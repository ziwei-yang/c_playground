#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <time.h>
#include <sys/time.h>

#include "urn.c"
#include "wss.c"

#include "jsmn.h"

int main(int argc, char **argv) {
	char fpath[] = "./example.json";
	char *contents;
	gsize clen;
	GError *error;

	gboolean ret = g_file_get_contents(fpath, &contents, &clen, &error);
	printf("ret  %d\n", ret);
	printf("clen %lu\n", clen);
//	URN_INFO(contents);

	URN_LOGF("Parsing json, size %lu", clen);
	int max_json_t = 65536;
	jsmntok_t tokens[max_json_t];
	jsmn_parser parser;
	jsmn_init(&parser);
	int rv = jsmn_parse(&parser, contents, clen, tokens, max_json_t);
	if (rv < 0)
		return URN_FATAL("Error in parsing json", rv);

	URN_INFOF("%d tokens parsed from %s", rv, fpath);
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
	for (int i=0; i< rv; i++) {
		jsmntok_t token = tokens[i];
		int cplen = URN_MIN(token.size, 49);
		cplen = URN_MIN((token.end - token.start), 49);
		memcpy(preview, contents+token.start, cplen);
		preview[cplen] = '\0';
		URN_LOGF("%9s token type, %d -> %d, len(%d) %s %s %s", type_desc[token.type], token.start, token.end, token.size, CLR_GREEN, preview, CLR_RST);
	}
}
