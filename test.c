#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <stdbool.h>

#include "urn.h"
#include "order.h"
#include "wss.h"
#include "hmap.h"

int test_inum () {
	int rv = 0;

	// parsing
	int ct = 5;
	char *s_a[]   = {
		"  123.0001230000  ",
		"123.0000123400001234",
		"0.00001234000000000000  ",
		"  00000000123",
		"-0.00001234000000000000",
		"-0.0000123400001234"
	};
	char *ans_a[] = {
		"123.000123      ",
		"123.00001234    ",
		  "0.00001234    ",
		"123             ",
		 "-0.00001234    ",
		 "-0.00001234    "
	};

	for (int i=0; i< ct; i++) {
		char *s = s_a[i];
		char *ans = ans_a[i];
		URN_LOGF("parsing     s [%s]", s);
		urn_inum i1;
		URN_RET_ON_RV(urn_inum_parse(&i1, s), "error in parsing s");
		char *is = urn_inum_str(&i1);
		URN_LOGF("parsed INUM s [%s]", is);
		URN_LOGF("expected ans  [%s]", ans);
		URN_RET_ON_RV(strcmp(is, ans), "error in comparing s and parsed is");
	}

	// comparision
	urn_inum i1;
	URN_RET_ON_RV(urn_inum_parse(&i1, "1234.5678"), "error in parsing i1");
	urn_inum i2;
	URN_RET_ON_RV(urn_inum_parse(&i2, "1234.56799"), "error in parsing i2");

	URN_RET_UNLESS(urn_inum_cmp(&i1, &i2) < 0, "i1 should < i2", EINVAL);
	URN_RET_UNLESS(urn_inum_cmp(&i2, &i1) > 0, "i2 should > i1", EINVAL);

	URN_RET_ON_RV(urn_inum_parse(&i2, "-1234.56799"), "error in parsing negative i2");

	URN_RET_UNLESS(urn_inum_cmp(&i1, &i2) > 0, "i1 should > i2", EINVAL);
	URN_RET_UNLESS(urn_inum_cmp(&i2, &i1) < 0, "i2 should < i1", EINVAL);

	URN_LOGF("sizeof long   %zu", sizeof(long));
	URN_LOGF("sizeof size_t %zu", sizeof(size_t));
	URN_LOGF("sizeof _Bool  %zu", sizeof(_Bool));
	size_t sizeof_desc = sizeof(i1)-sizeof(i1.intg)-sizeof(i1.frac_ext)-sizeof(i1.pstv);
	URN_LOGF("sizeof urn_inum.s %zu", sizeof_desc);
	URN_LOGF("sizeof urn_inum   %zu", sizeof(i1));
	URN_RET_UNLESS(sizeof_desc >= 32, "sizeof(inum.s) should >= 32", EINVAL);
	URN_RET_UNLESS(sizeof(i1)==64, "sizeof(inum) should == 64", EINVAL);
	URN_LOG_C(GREEN, "urn_inum test passed");

	// TODO
	// test urn_inum_add()
	return rv;
}

int test_odbk() {
	URN_LOGF("sizeof urn_odbk _Bool %zu", sizeof(_Bool));
	URN_LOGF("sizeof urn_odbk unsigned long %zu", sizeof(unsigned long));
	URN_LOGF("sizeof urn_odbk %zu", sizeof(urn_odbk));
	unsigned long expected_s = 64*(4*URN_ODBK_DEPTH+1);
	URN_RET_UNLESS(sizeof(urn_odbk)==expected_s, "sizeof(inum) should == 64*(4*DEPTH+1)", EINVAL);
	URN_LOG_C(GREEN, "urn_odbk test passed");
	return 0;
}

int test_ticks() {
	URN_LOGF("sizeof urn_ticks %zu", sizeof(urn_ticks));
	urn_ticks ticks;
	memset(&ticks, 0, sizeof(urn_ticks));
	for (int i=0; i<URN_TICK_LENTH*4; i++) {
		urn_inum p, s;
		p.intg = i+1;
		s.intg = i+10;
		urn_tick_append(&ticks, (i%2==0), &p, &s, i*1000000);
		// look back, verify ticks
		for (int j=0; j<=URN_TICK_LENTH; j++) {
			bool buy;
			unsigned long ts_e6=9999;
			int rv = urn_tick_get(&ticks, j, &buy, &p, &s, &ts_e6);
			if (j >= URN_TICK_LENTH && rv != ERANGE) {
				URN_FATAL("urn_tick_get() should return ERANGE", EINVAL);
			} else if (j > i && rv != ERANGE) {
				URN_FATAL("urn_tick_get() should return ERANGE", EINVAL);
			}
			if (j >= URN_TICK_LENTH) continue;
			if (j >= i) continue;
			if (rv != 0) {
				URN_WARNF("urn_tick_get() write %d, back %d, rv %d", i, j, rv);
				URN_FATAL("urn_tick_get() should return 0", EINVAL);
			}
			if (p.intg != (i-j+1)) {
				URN_WARNF("urn_tick_get() p %lu, expect %d", p.intg, i-j+1);
				URN_FATAL("urn_tick_get() p error", EINVAL);
			}
			if (s.intg != (i-j+10)) {
				URN_WARNF("urn_tick_get() s %lu, expect %d", s.intg, i-j+10);
				URN_FATAL("urn_tick_get() s error", EINVAL);
			}
			if (ts_e6 != (i-j+1000000)) {
				URN_WARNF("urn_tick_get() ts_e6 %lu, expect %d", ts_e6, (i-j)*1000000);
				URN_FATAL("urn_tick_get() ts_e6 error", EINVAL);
			}
		}
	}
	URN_LOG_C(GREEN, "urn_ticks test passed");
	return 0;
}

int main() {
	int rv = 0;
	URN_RET_ON_RV(test_inum(), "Error in test_inum()");
	URN_RET_ON_RV(test_odbk(), "Error in test_odbk()");
	URN_RET_ON_RV(test_ticks(), "Error in test_ticks()");
	return rv;
}
