#ifndef URANUS_TRADE_ORDER
#define URANUS_TRADE_ORDER

#include <string.h>
#include <glib.h>

typedef struct urn_pair urn_pair;
typedef struct urn_inum urn_inum;

//////////////////////////////////////////
// Market number represent in integer.
//////////////////////////////////////////
typedef struct urn_inum {
	long   intg;
	size_t frac_ext; // frac_ext = frac * 1e-URN_INUM_PRECISE
} urn_inum;
#define URN_INUM_PRECISE 14;
#define URN_INUM_FORMATS "%ld.%014zu"

// return the number of characters printed
int urn_inum_sprintf(urn_inum *i, char *s) {
	if (i->frac_ext == 0)
		return sprintf(s, "%ld", i->intg);
	return sprintf(s, URN_INUM_FORMATS, i->intg, i->frac_ext);
}

int urn_inum_parse(urn_inum *i, char *s) {
	// 123.00456 -> intg=123  frac_s='00456'
	char frac_s[99];
	int rv = sscanf(s, "%ld.%s", &(i->intg), frac_s);
	if (rv == 1) {
		i->frac_ext = 0;
		return 0;
	} else if (rv != 2)
		return EINVAL;

	// right pading '00456' 
	// -> rpad(URN_INUM_PRECISE) '00456000000000'
	// -> to unsigned long
	int padto = URN_INUM_PRECISE;
	char frac_padded[padto+1];
	int cp_ct = URN_MIN(padto, strlen(frac_s));
	for (int p = 0; p < padto; p++)
		frac_padded[p] = (p<cp_ct) ? frac_s[p] : '0';
	frac_padded[padto] = '\0';
	i->frac_ext = (size_t)(atol(frac_padded));

	// printf("%s = %ld.[%s] = " URN_INUM_FORMATS " ?\n", s, i->intg, frac_padded, i->intg, i->frac_ext);
	// printf("%s = " URN_INUM_FORMATS " ?\n", s, i->intg, i->frac_ext);
	return 0;
}
int urn_inum_cmp(urn_inum *i1, urn_inum *i2) {
	if (i1 == NULL && i2 == NULL) return 0;
	if (i1 != NULL && i2 == NULL) return INT_MAX;
	if (i1 == NULL && i2 != NULL) return INT_MIN;
	if (i1->intg > 0 && i2->intg > 0) {
		if (i1->intg != i2->intg)
			return URN_INTCAP(i1->intg - i2->intg);
		if (i1->frac_ext != i2->frac_ext)
			return URN_INTCAP(i1->frac_ext - i2->frac_ext);
		return 0;
	} else if (i1->intg > 0 && i2->intg < 0) {
		return URN_INTCAP(i1->intg); // divid by 2 incase of overflow.
	} else if (i1->intg < 0 && i2->intg > 0) {
		return URN_INTCAP(0-(i2->intg)); // divid by 2 incase of overflow.
	} else if ((i1->intg < 0) && (i2->intg < 0)) {
		if (i1->intg != i2->intg)
			return URN_INTCAP(i2->intg - i1->intg);
		if (i1->frac_ext != i2->frac_ext)
			return URN_INTCAP(i2->frac_ext - i1->frac_ext);
		return 0;
	}
	return 0;
}

//////////////////////////////////////////
// Public order from market data
//////////////////////////////////////////
typedef struct urn_porder {
	urn_pair *pair;
	urn_inum *p;
	urn_inum *s;
	struct timeval  *t;
	_Bool    *T; // 1: buy, 0: sell
} urn_porder;
int urn_order_cmp(urn_porder *o1, urn_porder *o2) {
	if (o1 == NULL && o2 == NULL) return 0;
	if (o1 != NULL && o2 == NULL) return INT_MAX;
	if (o1 == NULL && o2 != NULL) return INT_MIN;
	// TODO
	return 0;
}

//////////////////////////////////////////
// Pair
//////////////////////////////////////////

typedef struct urn_pair {
	gchar *name; // name str at first for printf directly.
	gchar *currency;
	gchar *asset;
	gchar *expiry;
} urn_pair;

void urn_pair_print(urn_pair pair) {
	URN_LOGF("currency %s", pair.currency);
	URN_LOGF("asset    %s", pair.asset);
	URN_LOGF("expiry   %s", pair.expiry);
	URN_LOGF("name     %s", pair.name);
}

// parse currency-asset@expiry, or currency-asset
int urn_pair_parse(urn_pair *pair, char *s, int slen, urn_func_opt *opt) {
	gchar **currency_and_left = NULL;
	gchar **asset_exp = NULL;
	// copy to new place for splitting.
	gchar *gs = g_ascii_strup(s, slen);

	// get currency
	currency_and_left= g_strsplit(gs, "-", 3);
	if (currency_and_left[1] == NULL) goto error;
	if (currency_and_left[2] != NULL) goto error;
	// get asset and expiry
	asset_exp = g_strsplit(currency_and_left[1], "@", 3);
	if (asset_exp[2] != NULL) goto error;

	pair->currency = currency_and_left[0];

	pair->asset = asset_exp[0];
	pair->expiry = asset_exp[1];
	pair->name = gs;

	g_free(currency_and_left[1]); // [0] taken, [2] NULL
	g_free(currency_and_left);
	g_free(asset_exp); // [0,1] taken, [2] NULL
	return 0;
error:
	g_strfreev(currency_and_left);
	g_strfreev(asset_exp);
	if (opt == NULL || !(opt->silent))
		URN_LOGF_C(RED, "failed to parse pair from %s", s);
	return EINVAL;
}

#endif
