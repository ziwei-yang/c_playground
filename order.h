#ifndef URANUS_TRADE_ORDER
#define URANUS_TRADE_ORDER

#include <string.h>
#include <glib.h>

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

/**
int urn_pair_str(char **sp, urn_pair *pair) {
	int l = pair->currency.l + 1 + pair->asset.l;
	if (pair->has_expiry)
		l = l + 1 + pair->expiry.l;
	*sp = malloc(l+1);
	if ((*sp) == NULL) return ENOMEM;
	
	int offset = 0;
	memcpy(*sp, pair->currency.s, pair->currency.l);
	offset += (pair->currency.l);
	*((*sp) + offset) = '-';
	offset += 1;
	memcpy((*sp)+offset, pair->asset.s, pair->asset.l);
	offset += (pair->asset.l);
	if (pair->has_expiry) {
		*((*sp) + offset) = '@';
		offset += 1;
		memcpy((*sp)+offset, pair->expiry.s, pair->expiry.l);
		offset += (pair->expiry.l);
	}
	*((*sp) + offset) = '\0';
	return 0;
}

// parse currency-asset@expiry, or currency-asset
int urn_pair_parse(urn_pair *pair, char *s, int slen, urn_func_opt *opt) {
	size_t currency_from=0, currency_to=0;
	size_t asset_from=0, asset_to=0;
	size_t expiry_from=0;
	char c;
	int rv = 0;
	for (size_t i=0; i<slen; i++) {
		c = *(s+i);
		if (currency_to == 0) { // scanning currency
			if (c != '-') continue;
			currency_to = i;
			asset_from = i+1;
		} else if (asset_to == 0) { // scanning asset
			if (c != '@') continue;
			asset_to = i;
			expiry_from = i+1;
		}
	}
	if (asset_from == 0) { // no '-' found
		if (opt == NULL || !(opt->silent))
			URN_LOGF_C(RED, "failed to parse pair from %s", s);
		return 1;
	}

	pair->currency.s = s+currency_from;
	pair->currency.l = currency_to-currency_from;

	pair->has_expiry = 0;
	if (expiry_from != 0) { // has '@' found.
		pair->has_expiry = 1;
		pair->expiry.s = s+expiry_from;
		pair->expiry.l = slen-expiry_from;
	} else
		asset_to = slen;

	pair->asset.s = s+asset_from;
	pair->asset.l = asset_to-asset_from;
	return 0;
}
**/

#endif
