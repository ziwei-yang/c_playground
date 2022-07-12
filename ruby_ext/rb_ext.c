#include "ruby.h"

#include "../urn.h"
#include "../hmap.h"

VALUE             URN_MKTDATA = Qnil;
urn_odbk_mem     *shmptr_arr[urn_shm_exch_num];
long              last_odbk_data_id[urn_shm_exch_num][URN_ODBK_MAX_PAIR];
long              last_tick_data_id[urn_shm_exch_num][URN_ODBK_MAX_PAIR];
urn_odbk_clients *clients_shmptr_arr[urn_shm_exch_num];
hashmap          *pair_to_id[urn_shm_exch_num];
hashmap          *pair_sigusr1[urn_shm_exch_num]; // pairs listening SIGUSR1

//////////////////////////////////////////
// Internal Methods below.
//////////////////////////////////////////

static int _prepare_shmptr(int i) {
	if (i < 0 || i >= urn_shm_exch_num) return ERANGE;
	if (shmptr_arr[i] != NULL) return 0;
	URN_INFOF("Attach to urn_odbk_shm %d %s", i, urn_shm_exchanges[i]);

	int rv = urn_odbk_shm_init(false, (char *)(urn_shm_exchanges[i]), &(shmptr_arr[i]));
	return rv;
}

static int _reset_pairmap(int i) {
	if (i < 0 || i >= urn_shm_exch_num) return ERANGE;
	// reset pointer, free old ptr if needed
	hashmap *pair_map = pair_to_id[i];
	pair_to_id[i] = NULL;
	if (pair_map != NULL) {
		URN_LOGF("Reset %s pair_to_id[%d]", urn_shm_exchanges[i], i);
		urn_hmap_free_with_keys(pair_map);
	}

	int rv = _prepare_shmptr(i);
	if (rv != 0) {
		URN_WARNF("Attach shmptr %d %s failed", i, urn_shm_exchanges[i]);
		return rv;
	}
	urn_odbk_mem *shmp = shmptr_arr[i];

	// build hashmap
	URN_LOGF("Reset %s pair_to_id[%d] with shmp->pairs", urn_shm_exchanges[i], i);
	pair_map = urn_hmap_init(URN_ODBK_MAX_PAIR*5);
	for (int pairid=0; pairid<URN_ODBK_MAX_PAIR; pairid++) {
		if (shmp->pairs[pairid][0] == '\0') break;
		char *key = malloc(strlen(shmp->pairs[pairid])+1);
		strcpy(key, shmp->pairs[pairid]);
		urn_hmap_set(pair_map, key, (uintptr_t)pairid);
	}
	pair_to_id[i] = pair_map;

	if (clients_shmptr_arr[i] == NULL)
		return 0;
	// If did method_mktdata_reg_sigusr1(), re-register PID in related SHMEM/pairs again.
	urn_odbk_clients_clear(clients_shmptr_arr[i]);
	size_t kidx, klen;
	char *key;
	void *val;
	urn_hmap_foreach(pair_sigusr1[i], kidx, key, klen, val) {
		uintptr_t pairid = 0;
		urn_hmap_getptr(pair_map, key, &pairid);
		if (pairid < 1 || pairid > URN_ODBK_MAX_PAIR) {
			URN_WARNF("\tre-register SIGUSR1 for %s failed, not found", key);
			continue;
		}
		URN_LOGF("\tre-register SIGUSR1 for %s %lu", key, pairid);
		urn_odbk_clients_reg(clients_shmptr_arr[i], pairid);
	}
	klen = klen;
	val = val;
	return 0;
}

/* get pairid from pair_to_id, verify with shm->pairs[pairid]
 * reset pair_to_id if pairid does not match
 */
static int _get_pairid(int exch_i, char *pair, uintptr_t *pairidp) {
	int rv = 0;
	if (exch_i < 0 || exch_i >= urn_shm_exch_num) return ERANGE;
	const char *exch = urn_shm_exchanges[exch_i];

	if (pair == NULL) {
		URN_WARNF("_get_pairmap %d %s pair is NULL", exch_i, exch);
		return ERANGE;
	}

	hashmap *pair_map = pair_to_id[exch_i];
	if (pair_map == NULL) {
		rv = _reset_pairmap(exch_i);
		pair_map = pair_to_id[exch_i];
		if ((rv != 0) || (pair_map == NULL)) {
			URN_WARNF("_reset_pairmap %s failed", exch);
			return EINVAL;
		}
	}
	urn_hmap_getptr(pair_map, pair, pairidp);
	if ((*pairidp) >= URN_ODBK_MAX_PAIR) {
		URN_WARNF("_get_pairmap %s %s out of range: %lu", exch, pair, *pairidp);
		return ERANGE;
	}

	urn_odbk_mem *shmp = shmptr_arr[exch_i];
	bool must_rebuild = false;
	// Found in pair_to_id, but still need to verify pair name.
	if ((*pairidp) == 0) { // NULL means not found.
		URN_WARNF("_get_pairmap %s %s not found: %lu, search again",
			exch, pair, *pairidp);
	} else if (strcmp(shmp->pairs[*pairidp], pair) == 0)
		return 0; // verified
	else {
		URN_LOGF("_get_pairmap %s %s=%lu found but does not match SHMEM %s, search again",
			exch, pair, *pairidp, shmp->pairs[*pairidp]);
		must_rebuild = true;
	}

	// Linear search in shmp, if found then reset pair_to_id
	bool found = false;
	for (int i=0; i<URN_ODBK_MAX_PAIR; i++) {
		if (shmp->pairs[i][0] == '\0') break;
		if (strcmp(shmp->pairs[i], pair) == 0) {
			*pairidp = (uintptr_t) i;
			found = true;
			break;
		}
	}
	URN_LOGF("SHMEM %s %s=%lu %s found", exch, pair, *pairidp, (found ? "":"not"));
	if (!found && !must_rebuild) return ENOENT;
	return _reset_pairmap(exch_i);
}

static urn_odbk_mem * _get_valid_urn_odbk_mem(VALUE v_idx) {
	if (RB_TYPE_P(v_idx, T_FIXNUM) != 1)
		return NULL;
	int idx = NUM2INT(v_idx);
	if (idx < 0 || idx >= urn_shm_exch_num) return NULL;

	// Prepare share memory ptr, only do this once.
	int rv = 0;
	if (shmptr_arr[idx] == NULL) {
		if ((rv = _prepare_shmptr(idx)) != 0) {
			URN_WARNF("Error in _prepare_shmptr %d %s",
				idx, urn_shm_exchanges[idx]);
			return NULL;
		}
	}

	return shmptr_arr[idx];
}

static int _prepare_clients_shmptr(int i) {
	if (i < 0 || i >= urn_shm_exch_num) return ERANGE;
	if (clients_shmptr_arr[i] != NULL) return 0;
	URN_INFOF("Attach to urn_odbk_clients shm %d %s", i, urn_shm_exchanges[i]);

	int rv = urn_odbk_clients_init((char *)(urn_shm_exchanges[i]), &(clients_shmptr_arr[i]));
	_reset_pairmap(i);
	return rv;
}

void sigusr1_handler(int sig) { return; }

/* macos has no sigtimedwait(); use sigwait() + alarm() to simulate */
void macos_sigalrm_handler(int sig) { kill(getpid(), SIGUSR1); }

//////////////////////////////////////////
// Ruby Methods below.
//////////////////////////////////////////

VALUE method_mktdata_shm_index(VALUE self, VALUE str) {
	if (RB_TYPE_P(str, T_STRING) != 1)
		return INT2NUM(-1);
	return INT2NUM(urn_odbk_shm_i(RSTRING_PTR(str)));
}

VALUE method_mktdata_exch(VALUE self, VALUE idx) {
	if (RB_TYPE_P(idx, T_FIXNUM) != 1)
		return Qnil;
	int i = NUM2INT(idx);
	if (i < 0 || i >= urn_shm_exch_num) return Qnil;
	return rb_str_new_cstr(urn_shm_exchanges[i]);
}

VALUE method_mktdata_pairs(VALUE self, VALUE v_idx) {
	urn_odbk_mem *shmp = _get_valid_urn_odbk_mem(v_idx);
	if (shmp == NULL) return Qnil;

	VALUE pairs = rb_ary_new_capa(URN_ODBK_MAX_PAIR);
	for (int i=0; i<URN_ODBK_MAX_PAIR; i++) {
		if (shmp->pairs[i][0] == '\0') break;
		rb_ary_push(pairs, rb_str_new_cstr(shmp->pairs[i]));
	}
	return pairs;
}

urn_odbk * _get_valid_urn_odbk(VALUE v_idx, VALUE v_pair, uintptr_t *pairidp) {
	urn_odbk_mem *shmp = _get_valid_urn_odbk_mem(v_idx);
	if (shmp == NULL) return NULL;

	if (RB_TYPE_P(v_pair, T_STRING) != 1)
		return NULL;

	int rv = _get_pairid(NUM2INT(v_idx), RSTRING_PTR(v_pair), pairidp);
	if (rv != 0) return NULL;
	uintptr_t pairid = *pairidp;

	if (pairid < 1 || pairid >= URN_ODBK_MAX_PAIR) return NULL;

	urn_odbk *odbk = NULL;
	if (shmp->odbks[pairid][0].complete) {
		odbk = &(shmp->odbks[pairid][0]);
	} else if (shmp->odbks[pairid][1].complete) {
		odbk = &(shmp->odbks[pairid][1]);
	}
	return odbk;
}

urn_ticks * _get_valid_urn_ticks(VALUE v_idx, VALUE v_pair, uintptr_t *pairidp) {
	urn_odbk_mem *shmp = _get_valid_urn_odbk_mem(v_idx);
	if (shmp == NULL) return NULL;

	if (RB_TYPE_P(v_pair, T_STRING) != 1)
		return NULL;

	int rv = _get_pairid(NUM2INT(v_idx), RSTRING_PTR(v_pair), pairidp);
	if (rv != 0) return NULL;
	uintptr_t pairid = *pairidp;

	if (pairid < 1 || pairid >= URN_ODBK_MAX_PAIR) return NULL;

	return &(shmp->ticks[pairid]);
}

VALUE _make_odbk_data_if_newer(VALUE v_idx, VALUE v_pair, VALUE v_max_row, bool comp_ver) {
	uintptr_t pairid = 0;
	urn_odbk *odbk = _get_valid_urn_odbk(v_idx, v_pair, &pairid);
	if (odbk == NULL) return Qnil;

	int exch_idx = NUM2INT(v_idx);
	// writer changes timestamp first.
	// For readers, check ts_e6 before and after reading all data.
	// if ts_e6 changed, data should be dirty.
	unsigned long data_ts_e6 = odbk->w_ts_e6;
	unsigned long known_data_id = 0l;
	if (comp_ver) {
		known_data_id = last_odbk_data_id[exch_idx][pairid];
		if (known_data_id >= data_ts_e6)
			return Qnil;
	}

	int max_row = URN_ODBK_DEPTH;
	if (RB_TYPE_P(v_idx, T_FIXNUM) == 1)
		max_row = NUM2INT(v_max_row);
	max_row = URN_MIN(URN_ODBK_DEPTH, (int)max_row);
	if (max_row <= 0) return Qnil;

	long mkt_ts_e6;
	double bids_l[max_row];
	double bidp_l[max_row];
	double asks_l[max_row];
	double askp_l[max_row];
	int bid_ct, ask_ct;

read_shmem:
	// data timestamp(writer changes it firstly) should be same at last.
	data_ts_e6 = odbk->w_ts_e6;
	mkt_ts_e6 = odbk->mkt_ts_e6;
	bid_ct = 0; ask_ct = 0;
	for (int i = 0; i < max_row; i++) {
		if (urn_inum_iszero(&(odbk->bidp[i])) &&
				urn_inum_iszero(&(odbk->askp[i]))) {
			break;
		}
		if (!urn_inum_iszero(&(odbk->bidp[i]))) {
			bids_l[bid_ct] = urn_inum_to_db(&(odbk->bids[i]));
			bidp_l[bid_ct] = urn_inum_to_db(&(odbk->bidp[i]));
			bid_ct++;
		}
		if (!urn_inum_iszero(&(odbk->askp[i]))) {
			askp_l[ask_ct] = urn_inum_to_db(&(odbk->askp[i]));
			asks_l[ask_ct] = urn_inum_to_db(&(odbk->asks[i]));
			ask_ct++;
		}
	}
	if (data_ts_e6 != odbk->w_ts_e6) {
		URN_LOGF_C(YELLOW, "%s %s data_ts_e6 changed, data dirty, retry.",
				urn_shm_exchanges[exch_idx],
				shmptr_arr[exch_idx]->pairs[pairid]);
		// choose odbk to read again.
		uintptr_t new_pairid;
		odbk = _get_valid_urn_odbk(v_idx, v_pair, &new_pairid);
		// If pairid changed, abort and return empty data.
		if (new_pairid != pairid) return Qnil;
		if (odbk == NULL) return Qnil;
		data_ts_e6 = odbk->w_ts_e6;
		if (known_data_id >= data_ts_e6) return Qnil;
		goto read_shmem;
	}

	// make ruby array [bids, asks, writer_t, mkt_t]
	VALUE r_odbk = rb_ary_new_capa(4);
	VALUE r_bids = rb_ary_new_capa(max_row);
	VALUE r_asks = rb_ary_new_capa(max_row);
	for (int i=0; i<bid_ct; i++) {
		VALUE r_bid = rb_ary_new_capa(2);
		rb_ary_push(r_bid, DBL2NUM(bidp_l[i]));
		rb_ary_push(r_bid, DBL2NUM(bids_l[i]));
		rb_ary_push(r_bids, r_bid);
	}
	for (int i=0; i<ask_ct; i++) {
		VALUE r_ask = rb_ary_new_capa(2);
		rb_ary_push(r_ask, DBL2NUM(askp_l[i]));
		rb_ary_push(r_ask, DBL2NUM(asks_l[i]));
		rb_ary_push(r_asks, r_ask);
	}
	rb_ary_push(r_odbk, r_bids);
	rb_ary_push(r_odbk, r_asks);
	rb_ary_push(r_odbk, LONG2NUM(data_ts_e6/1000));
	rb_ary_push(r_odbk, LONG2NUM(mkt_ts_e6/1000));
	// save data time to local cache, next time compare this first.
	last_odbk_data_id[exch_idx][pairid] = data_ts_e6;
	return r_odbk;
}

VALUE _make_tick_data_if_newer(VALUE v_idx, VALUE v_pair, VALUE v_max_row, bool comp_ver) {
	uintptr_t pairid = 0;
	urn_ticks *ticks = _get_valid_urn_ticks(v_idx, v_pair, &pairid);
	if (ticks == NULL) return Qnil;

	int exch_idx = NUM2INT(v_idx);
	unsigned long data_ts_e6 = ticks->latest_t_e6;
	unsigned long known_data_id = 0l;
	if (data_ts_e6 <= 0)
		return Qnil;
	if (comp_ver) {
		known_data_id = last_tick_data_id[exch_idx][pairid];
		if (known_data_id >= data_ts_e6)
			return Qnil;
	}

	int max_row = URN_TICK_LENTH;
	if (RB_TYPE_P(v_idx, T_FIXNUM) == 1)
		max_row = NUM2INT(v_max_row);
	max_row = URN_MIN(URN_TICK_LENTH, (int)max_row);
	if (max_row <= 0) return Qnil;

	double sizes[max_row];
	double prices[max_row];
	bool   sides[max_row];
	unsigned long ts_e6s[max_row];
	urn_inum p, s;

	for (int i = 0; i < max_row; i++) {
		int rv = urn_tick_get(ticks, i, &(sides[i]), &p, &s, &(ts_e6s[i]));
		if (rv == ERANGE) {
			max_row = i;
			break;
		}
		if (comp_ver && (ts_e6s[i] <= known_data_id)) {
			// break if data is not new
			max_row = i;
			break;
		}
		prices[i] = urn_inum_to_db(&p);
		sizes[i] = urn_inum_to_db(&s);
	}

	// make ruby array [[p, s, 1/0:(buy/sell), ts_e3]]
	VALUE r_ticks = rb_ary_new_capa(max_row);
	for (int i=0; i<max_row; i++) {
		VALUE r_tk = rb_ary_new_capa(4);
		rb_ary_push(r_tk, DBL2NUM(prices[i]));
		rb_ary_push(r_tk, DBL2NUM(sizes[i]));
		rb_ary_push(r_tk, INT2NUM((int)(sides[i])));
		rb_ary_push(r_tk, LONG2NUM(ts_e6s[i]/1000));
		rb_ary_push(r_ticks, r_tk);
	}
	// save data time to local cache, next time compare this first.
	last_tick_data_id[exch_idx][pairid] = data_ts_e6;
	return r_ticks;
}

// Always return shmem odbk without comparing known_data_id
VALUE method_mktdata_odbk(VALUE self, VALUE v_idx, VALUE v_pair, VALUE v_max_row) {
	return _make_odbk_data_if_newer(v_idx, v_pair, v_max_row, false);
}
VALUE method_mktdata_new_odbk(VALUE self, VALUE v_idx, VALUE v_pair, VALUE v_max_row) {
	return _make_odbk_data_if_newer(v_idx, v_pair, v_max_row, true);
}

// Always return shmem ticks without comparing known_data_id
VALUE method_mktdata_ticks(VALUE self, VALUE v_idx, VALUE v_pair, VALUE v_max_row) {
	return _make_tick_data_if_newer(v_idx, v_pair, v_max_row, false);
}
VALUE method_mktdata_new_ticks(VALUE self, VALUE v_idx, VALUE v_pair, VALUE v_max_row) {
	return _make_tick_data_if_newer(v_idx, v_pair, v_max_row, true);
}

VALUE method_mktdata_reg_sigusr1(VALUE self, VALUE v_idx, VALUE v_pair) {
	if (RB_TYPE_P(v_idx, T_FIXNUM) != 1)
		return Qnil;
	int idx = NUM2INT(v_idx);
	if (idx < 0 || idx >= urn_shm_exch_num) return Qnil;

	// Prepare share memory ptr, only do this once.
	int rv = 0;
	if (clients_shmptr_arr[idx] == NULL) {
		if ((rv = _prepare_clients_shmptr(idx)) != 0) {
			URN_WARNF("Error in _prepare_clients_shmptr %d", idx);
			return Qnil;
		}
	}

	uintptr_t pairid = 0;
	rv = _get_pairid(idx, RSTRING_PTR(v_pair), &pairid);
	if (rv != 0) {
		URN_WARNF("Error in _get_pairid %d %s", idx, RSTRING_PTR(v_pair));
		return Qnil;
	}
	if (pairid < 1 || pairid >= URN_ODBK_MAX_PAIR) return Qnil;

	// record pair to pair_sigusr1 hashmap.
	urn_odbk_mem *shmp = shmptr_arr[idx];
	char *key = malloc(strlen(shmp->pairs[pairid])+1);
	strcpy(key, shmp->pairs[pairid]);
	urn_hmap_set(pair_sigusr1[idx], key, (uintptr_t)pairid);

	signal(SIGUSR1, sigusr1_handler);
#if __APPLE__
	signal(SIGALRM, macos_sigalrm_handler);
#endif
	rv = urn_odbk_clients_reg(clients_shmptr_arr[idx], pairid);
	return INT2NUM(rv);
}

sigset_t sigusr1_set;
VALUE method_sigusr1_timedwait(VALUE self, VALUE v_waits) {
	struct timespec timeout;
	if (RB_TYPE_P(v_waits, T_FIXNUM) == 1) {
		timeout.tv_sec = NUM2INT(v_waits);
		timeout.tv_nsec = 0;
	} else if (RB_TYPE_P(v_waits, T_FLOAT) == 1) {
		double f = NUM2DBL(v_waits);
		timeout.tv_sec = (long)f;
		timeout.tv_nsec = (long)((f - (long)f) * 1000000000l);
	} else if (RB_TYPE_P(v_waits, T_NIL) == 1) {
		// wait without timer
		int sig = 0;
		while(true) {
			sigwait(&sigusr1_set, &sig);
			if (sig == SIGUSR1) break;
		}
		return INT2NUM(sig);
	} else
		return Qnil;
	// URN_INFOF("sigtimedwait %lds, %ldns", timeout.tv_sec, timeout.tv_nsec);
#if __APPLE__
	// macos has no sigtimedwait(); timeout makes no different.
	// setup a alarm to send self a SIGUSR1
	unsigned secs = (unsigned)(timeout.tv_sec);
	alarm(URN_MAX(secs, 1));
	int sig = 0;
	while(true) {
		sigwait(&sigusr1_set, &sig);
		if (sig == SIGUSR1) break;
	}
#else
	int sig = sigtimedwait(&sigusr1_set, NULL, &timeout);
#endif
	return INT2NUM(sig);
}

// The initialization method for this module
// Prototype for the initialization method - Ruby calls this, not you
void Init_urn_mktdata() {
	URN_MKTDATA = rb_define_module("URN_MKTDATA");
	rb_define_method(URN_MKTDATA, "mktdata_shm_index", method_mktdata_shm_index, 1);
	rb_define_method(URN_MKTDATA, "mktdata_exch_by_shm_index", method_mktdata_exch, 1);
	rb_define_method(URN_MKTDATA, "mktdata_pairs", method_mktdata_pairs, 1);
	rb_define_method(URN_MKTDATA, "mktdata_odbk", method_mktdata_odbk, 3);
	rb_define_method(URN_MKTDATA, "mktdata_new_odbk", method_mktdata_new_odbk, 3);
	rb_define_method(URN_MKTDATA, "mktdata_ticks", method_mktdata_ticks, 3);
	rb_define_method(URN_MKTDATA, "mktdata_new_ticks", method_mktdata_new_ticks, 3);
	rb_define_method(URN_MKTDATA, "mktdata_reg_sigusr1", method_mktdata_reg_sigusr1, 2);
	rb_define_method(URN_MKTDATA, "mktdata_sigusr1_timedwait", method_sigusr1_timedwait, 1);

	/* module essential init */
	for (int i=0; i<urn_shm_exch_num; i++) {
		shmptr_arr[i] = NULL;
		clients_shmptr_arr[i] = NULL;
		pair_to_id[i] = NULL;
		pair_sigusr1[i] = urn_hmap_init(URN_ODBK_MAX_PAIR*5);
		for (int j=0; j<URN_ODBK_MAX_PAIR; j++) {
			last_odbk_data_id[i][j] = 0l;
			last_tick_data_id[i][j] = 0l;
		}
	}
	sigemptyset(&sigusr1_set);
	sigaddset(&sigusr1_set, SIGUSR1);
}
