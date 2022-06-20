#ifndef URANUS_TRADE_ORDER
#define URANUS_TRADE_ORDER

#include <string.h>
#include <stdbool.h>

//////////////////////////////////////////
// Extended methods, with glib's help
//////////////////////////////////////////

#include <glib.h>

//////////////////////////////////////////
// Order Pair
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

void urn_pair_free(urn_pair *p) {
	if (p->name != NULL) g_free(p->name);
	if (p->currency != NULL) g_free(p->currency);
	if (p->asset != NULL) g_free(p->asset);
	if (p->expiry != NULL) g_free(p->expiry);
	free(p);
}

// parse currency-asset@expiry, or currency-asset
int urn_pair_alloc(urn_pair *pair, char *s, int slen, urn_func_opt *opt) {
	gchar **currency_and_left = NULL;
	gchar **asset_exp = NULL;
	// copy to new place for splitting.
	gchar *gs = g_ascii_strup(s, slen);

	// get currency
	currency_and_left = g_strsplit(gs, "-", 3);
	if (currency_and_left[1] == NULL) goto error;
	if (currency_and_left[2] != NULL) goto error;

	// get asset and expiry
	asset_exp = g_strsplit(currency_and_left[1], "@", 3);
	// asset_exp might not have [1 expiry], check [1] first.
	if (asset_exp[1] != NULL && asset_exp[2] != NULL) goto error;

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
// Public order in orderbook
//////////////////////////////////////////

typedef struct urn_porder {
	urn_pair *pair;
	urn_inum *p;
	urn_inum *s;
	struct timeval  *t;
	_Bool    T; // 1: buy, 0: sell
	char    *desc; // self managed description cache.
} urn_porder;

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
// orderbook data operation
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
	int node_ct = 0;
	int ct = 0;
	*(s+ct) = '[';
	ct ++;
	GList *node = tail;
	do {
		if (node == l && node->data == NULL)
			break; // only head could have NULL
		node_ct ++;
		ct += sprintf_odbko_json(s+ct, node->data);
		*(s+ct) = ','; ct++;
		if (node == l) break;
		node = node->prev;
	} while(true);
	if (node_ct == 0) {
		// Append ']'
		*(s+ct) = ']';
		ct++;
	} else {
		// change last ',' char to ']'
		*(s+ct-1) = ']';
	}
	return ct;
}

//////////////////////////////////////////
// orderbook data read/write in share memory
//////////////////////////////////////////

/* asks and bids are sorted desc, from bottom to top */
int urn_odbk_shm_write(
		urn_odbk_mem *shmp,
		int pairid,
		GList *asks, int ask_ct,
		GList *bids, int bid_ct,
		long mkt_ts_e6,
		long w_ts_e6,
		char *desc
		)
{
	URN_RET_IF((pairid >= urn_odbk_mem_cap), "pairid too big", ERANGE);
	// choose a dirty side to write in.
	urn_odbk *odbk = NULL;
	int mark_i_complete = -1;
	int mark_i_dirty = -1;
	if (shmp->odbks[pairid][0].complete == false) {
		odbk = &(shmp->odbks[pairid][0]);
		mark_i_complete = 0;
		mark_i_dirty = 1;
	} else if (shmp->odbks[pairid][1].complete == false) {
		odbk = &(shmp->odbks[pairid][1]);
		mark_i_complete = 1;
		mark_i_dirty = 0;
	}

	// Write depth no more than URN_ODBK_DEPTH
	urn_porder *tmp_o = NULL;
	if (asks != NULL) {
		// skip too deep data
		while (ask_ct > URN_ODBK_DEPTH) {
			asks = asks->next;
			ask_ct --;
			URN_RET_IF((asks == NULL), "asks is NULL", EINVAL);
		}
		// write NULL for ask_ct+1 order
		if (ask_ct < URN_ODBK_DEPTH) {
			memset(&(odbk->askp[ask_ct]), 0, sizeof(urn_inum));
			memset(&(odbk->asks[ask_ct]), 0, sizeof(urn_inum));
		}
		// write data from [ask_ct-1] to [0]
		while (ask_ct > 0) {
			URN_RET_IF((asks == NULL), "asks is NULL", EINVAL);
			tmp_o = asks->data;
			URN_RET_IF((tmp_o == NULL), "asks o is NULL", EINVAL);
			memcpy(&(odbk->askp[ask_ct-1]), tmp_o->p, sizeof(urn_inum));
			memcpy(&(odbk->asks[ask_ct-1]), tmp_o->s, sizeof(urn_inum));
			asks = asks->next;
			ask_ct --;
		}
	}
	if (bids != NULL) {
		// skip too deep data
		while (bid_ct > URN_ODBK_DEPTH) {
			bids = bids->next;
			bid_ct --;
			URN_RET_IF((bids == NULL), "bids is NULL", EINVAL);
		}
		// write NULL for bid_ct+1 order
		if (bid_ct < URN_ODBK_DEPTH) {
			memset(&(odbk->bidp[bid_ct]), 0, sizeof(urn_inum));
			memset(&(odbk->bids[bid_ct]), 0, sizeof(urn_inum));
		}
		// write data from [bid_ct-1] to [0]
		while (bid_ct > 0) {
			URN_RET_IF((bids == NULL), "bids is NULL", EINVAL);
			tmp_o = bids->data;
			URN_RET_IF((tmp_o == NULL), "bids o is NULL", EINVAL);
			memcpy(&(odbk->bidp[bid_ct-1]), tmp_o->p, sizeof(urn_inum));
			memcpy(&(odbk->bids[bid_ct-1]), tmp_o->s, sizeof(urn_inum));
			bids = bids->next;
			bid_ct --;
		}
	}

	odbk->mkt_ts_e6 = mkt_ts_e6;
	odbk->w_ts_e6 = w_ts_e6;
	if (desc != NULL)
		strcpy(odbk->desc, desc);

	// mark this side complete and another side dirty (place to write next)
	shmp->odbks[pairid][mark_i_complete].complete = true;
	shmp->odbks[pairid][mark_i_dirty].complete = false;
	return 0;
}

#endif
