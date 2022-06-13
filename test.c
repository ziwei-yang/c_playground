#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "urn.h"
#include "order.h"
#include "wss.h"
#include "hmap.h"

int test_inum () {
	int rv = 0;
	int ct = 3;
	char *s_a[]   = {
		"123.000123", "123.0000123400001234",
		"123"
	};
	char *ans_a[] = {
		"123.00012300000000", "123.00001234000012",
		"123"
	};

	for (int i=0; i< ct; i++) {
		char *s = s_a[i];
		char *ans = ans_a[i];
		URN_LOGF("parsing     %s", s);
		urn_inum i1;
		URN_RET_ON_RV(urn_inum_parse(&i1, s), "error in parsing s");
		char *is = malloc(30);
		urn_inum_sprintf(&i1, is);
		URN_LOGF("parsed INUM %s", is);
		URN_RET_ON_RV(strcmp(is, ans), "error in comparing parsed s");
	}

	return rv;
}

int main() {
	URN_RET_ON_RV(test_inum(), "Error in test_inum()");
	return 0;
}
