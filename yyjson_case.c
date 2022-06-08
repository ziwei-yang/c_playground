#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <time.h>
#include <sys/time.h>

#include "urn.c"
#include "wss.c"

#include "yyjson.h"
#include "yyjson.c"

int main(int argc, char **argv) {
//	char fpath[] = "./example.json";
	char fpath[] = "./pos_trader_SLP.json";
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
}

