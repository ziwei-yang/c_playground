#ifndef URANUS_TRADE_ORDER
#define URANUS_TRADE_ORDER

#include <string.h>
#include <glib.h>

typedef struct urn_inum {
	long   intg;
	size_t frac_ext; // frac_ext = frac * 1e-URN_INUM_PRECISE
	_Bool  pstv; // when intg is zero, use this for sign.
	// description size padding to 64
	char   s[64-sizeof(long)-sizeof(size_t)-sizeof(_Bool)];
} urn_inum;

typedef struct urn_pair {
	gchar *name; // name str at first for printf directly.
	gchar *currency;
	gchar *asset;
	gchar *expiry;
} urn_pair;

typedef struct urn_porder {
	urn_pair *pair;
	urn_inum *p;
	urn_inum *s;
	struct timeval  *t;
	_Bool    T; // 1: buy, 0: sell
	char    *desc; // self managed description cache.
} urn_porder;

//////////////////////////////////////////
// Market number represent in integer.
//////////////////////////////////////////
#define URN_INUM_PRECISE 12
#define URN_INUM_FRACEXP 100000000000000ul

// print inum into s (or internal description pointer)
char *urn_inum_str(urn_inum *i) {
	if (i == NULL) return NULL;
	char *s = i->s;
	if (s[0] != '\0') // cached already.
		return s;

	if (i->frac_ext == 0) {
		sprintf(s, "%ld%*s", i->intg, URN_INUM_PRECISE+1, "");
		return s;
	}

	int chars = 0;
	if (i->pstv)
		chars = sprintf(s, "%ld.%012zu", i->intg, i->frac_ext);
	else if (i->intg != 0)
		chars = sprintf(s, "%ld.%012zu", i->intg, i->frac_ext);
	else // intg=0 and negative.
		chars = sprintf(s, "-%ld.%012zu", i->intg, i->frac_ext);
	// Replace 0s in fraction at end with space.
	while (chars > 0) {
		if (s[chars-1] == '0') {
			s[chars-1] = ' ';
			chars--;
		} else
			break;
	}
	return s;
}

int urn_inum_sprintf(urn_inum *i, char *s) {
	if (i == NULL) return 0;
	if (i->frac_ext == 0) // integer only.
		return sprintf(s, "%ld", i->intg);
	else if (i->pstv)
		return sprintf(s, "%ld.%zu", i->intg, i->frac_ext);
	else if (i->intg != 0)
		return sprintf(s, "%ld.%zu", i->intg, i->frac_ext);
	else // intg=0 and negative.
		return sprintf(s, "-%ld.%zu", i->intg, i->frac_ext);
}

// Every inum should be initialized here.
int urn_inum_parse(urn_inum *i, const char *s) {
	(i->s)[0] = '\0'; // clear desc
	int rv = 0;
	// -0.00456 -> intg=0 intg_s='-0'  frac_s='00456'
	char intg_s[99];
	sscanf(s, "%s.", intg_s);
	i->pstv = (intg_s[0] == '-') ? 0 : 1;

	char frac_s[99];
	rv = sscanf(s, "%ld.%s", &(i->intg), frac_s);
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

	return 0;
}
int urn_inum_alloc(urn_inum **i, const char *s) {
	URN_RET_ON_NULL(*i = malloc(sizeof(urn_inum)), "Not enough memory", ENOMEM);
	return urn_inum_parse(*i, s);
}

int urn_inum_cmp(urn_inum *i1, urn_inum *i2) {
	if (i1 == NULL && i2 == NULL) return 0;
	if (i1 != NULL && i2 == NULL) return INT_MAX;
	if (i1 == NULL && i2 != NULL) return INT_MIN;
//	URN_DEBUGF("Sorting inum %20s and %20s", urn_inum_str(i1), urn_inum_str(i2));
	if (i1->pstv && i2->pstv) {
		if (i1->intg != i2->intg)
			return (i1->intg > i2->intg) ? 1 : -1;
		if (i1->frac_ext != i2->frac_ext)
			return (i1->frac_ext > i2->frac_ext) ? 1 : -1;
		return 0;
	} else if (i1->pstv && !(i2->pstv)) {
		return 1;
	} else if (!(i1->pstv) && i2->pstv) {
		return -1;
	} else if (!(i1->pstv) && !(i2->pstv)) {
		if (i1->intg != i2->intg)
			return (i1->intg > i2->intg) ? 1 : -1;
		// when intg is same, fraction bigger means number smaller.
		if (i1->frac_ext != i2->frac_ext)
			return (i1->frac_ext < i2->frac_ext) ? 1 : -1;
		return 0;
	}
	return 0;
}

//////////////////////////////////////////
// Public order from market data
//////////////////////////////////////////
urn_porder *urn_porder_alloc(urn_pair *pair, urn_inum *p, urn_inum *s, _Bool buy, struct timeval *t) {
	urn_porder *o = malloc(sizeof(urn_porder));
	if (o == NULL) return o;
	o->pair = pair;
	o->p = p;
	o->s = s;
	o->t = t;
	o->T = buy;
	o->desc = NULL;
	return o;
}
void urn_porder_free(void *ptr) {
//	URN_DEBUGF("urn_pair_free %p", ptr);
	urn_porder *o = (urn_porder *)ptr;
	if (o == NULL) return;
	if (o->pair != NULL) free(o->pair);
	if (o->p != NULL) free(o->p);
	if (o->s != NULL) free(o->s);
	if (o->t != NULL) free(o->t);
	if (o->desc != NULL) free(o->desc);
	free(o);
}
char *urn_porder_str(urn_porder *o) {
	if (o->desc != NULL) return o->desc;
	o->desc = malloc(128);
	sprintf(o->desc, "%s %20s %20s",
			o->pair == NULL ? NULL : o->pair->name,
			urn_inum_str(o->p),
			urn_inum_str(o->s));
	return o->desc;
}
// Comparator for price only.
int urn_porder_px_cmp(const void *o1, const void *o2) {
//	URN_DEBUGF("Sorting %p and %p", o1, o2);
	if (o1 == NULL && o2 == NULL) return 0;
	if (o1 != NULL && o2 == NULL) return INT_MAX;
	if (o1 == NULL && o2 != NULL) return INT_MIN;
	return urn_inum_cmp(((urn_porder *)o1)->p, ((urn_porder *)o2)->p);
}
int urn_porder_px_cmprv(const void *o1, const void *o2) {
	if (o1 == NULL && o2 == NULL) return 0;
	if (o1 != NULL && o2 == NULL) return INT_MAX;
	if (o1 == NULL && o2 != NULL) return INT_MIN;
	return 0-urn_porder_px_cmp(o1, o2);
}
// should be faster than yyjson dump
static int sprintf_odbko_json(char *s, urn_porder *o) {
	if (o == NULL) URN_FATAL("sprintf_odbko_json() with NULL o", EINVAL);
	int ct = 0;
	ct += sprintf(s+ct, "{\"p\":");
	ct += urn_inum_sprintf(o->p, s+ct);
	ct += sprintf(s+ct, ",\"s\":");
	ct += urn_inum_sprintf(o->s, s+ct);
	*(s+ct) = '}'; ct++;
	return ct;
}

//////////////////////////////////////////
// Pair
//////////////////////////////////////////
void urn_pair_print(urn_pair pair) {
	URN_LOGF("currency %s", pair.currency);
	URN_LOGF("asset    %s", pair.asset);
	URN_LOGF("expiry   %s", pair.expiry);
	URN_LOGF("name     %s", pair.name);
}

void urn_pair_free(urn_pair *p) {
	if (p->name != NULL) g_free(p->name);
	if (p->currency != NULL) g_free(p->currency);
	if (p->asset != NULL) g_free(p->asset);
	if (p->expiry != NULL) g_free(p->expiry);
	free(p);
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

//////////////////////////////////////////
// orderbook data in GList
//////////////////////////////////////////

// odbk orders are saved from bottom to top
// should be faster than yyjson dump
static int sprintf_odbk_json(char *s, GList *l) {
	if (l == NULL) URN_FATAL("sprintf_odbk_json() with NULL l", EINVAL);
	// find the end of GList
	GList *tail = l;
	while(tail->next != NULL)
		tail = tail->next;
	// build from tail to head
	int ct = 0;
	*(s+ct) = '[';
	ct ++;
	GList *node = tail;
	do {
		if (node == l && node->data == NULL)
			break; // only head could have NULL
		ct += sprintf_odbko_json(s+ct, node->data);
		*(s+ct) = ','; ct++;
		if (node == l) break;
		node = node->prev;
	} while(true);
	// change last char to ']'
	*(s+ct-1) = ']';
	return ct;
}

#endif
