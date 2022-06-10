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

	char *s = "AB-CD@EF";
	GString *gs = g_string_new_len(s, strlen(s));
	printf("GString s %s\n", gs->str);
	char **segs = g_strsplit(gs->str, "-", 3);
	printf("split seg[0] %s\n", segs[0]);
	printf("split seg[1] %s\n", segs[1]);
	printf("split seg[2] %s\n", segs[2]);

	GArray *garray = g_array_sized_new(1, 1, 10, 5);
	g_array_append_val(garray, "X");
	g_array_append_val(garray, "B");
	g_array_append_val(garray, "D");
	g_array_append_val(garray, "A");
	return 0;
}
