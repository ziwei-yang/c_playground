#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#define EPRINT(msg) fprintf (stderr, "%s:%d %s(): %s\n", __FILE__, __LINE__, __FUNCTION__,  msg);
#define DEBUG(msg) fprintf (stdout, "%s:%d %s(): %s\n", __FILE__, __LINE__, __FUNCTION__,  msg);

int main() {
	char fpath[] = "./ftx-com-chain.pem";
	char *contents;
	char **content_addr = &contents;
	gsize clen;
	GError *error;
	gboolean ret = g_file_get_contents(fpath, content_addr, &clen, &error);
	printf("ret  %d\n", ret);
	printf("clen %lu\n", clen);
	DEBUG(*content_addr);
	return 0;
}
