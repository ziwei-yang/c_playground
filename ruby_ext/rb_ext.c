#include "ruby.h"

#include "../urn.h"

VALUE URN_MKTDATA = Qnil;
urn_odbk_mem *shmptr_arr[urn_shm_exch_num];
long last_odbk_data_id[urn_shm_exch_num][urn_odbk_mem_cap];

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

int _prepare_shmptr(int i) {
	if (i < 0 || i >= urn_shm_exch_num) return ERANGE;
	if (shmptr_arr[i] != NULL) return 0;
	URN_INFOF("Attach to urn_odbk_shm %d %s", i, urn_shm_exchanges[i]);

	key_t shmkey;
	int shmid;
	int rv = urn_odbk_shm_init(false, (char *)(urn_shm_exchanges[i]), &shmkey, &shmid, &(shmptr_arr[i]));
	return rv;
}

urn_odbk_mem * _get_valid_urn_odbk_mem(VALUE v_idx) {
	if (RB_TYPE_P(v_idx, T_FIXNUM) != 1)
		return NULL;
	int idx = NUM2INT(v_idx);
	if (idx < 0 || idx >= urn_shm_exch_num) return NULL;

	// Prepare share memory ptr, only do this once.
	int rv = 0;
	if (shmptr_arr[idx] == NULL) {
		if ((rv = _prepare_shmptr(idx)) != 0) {
			URN_WARNF("Error in _prepare_shmptr %d", idx);
			return NULL;
		}
	}

	return shmptr_arr[idx];
}

VALUE method_mktdata_pairs(VALUE self, VALUE v_idx) {
	urn_odbk_mem *shmp = _get_valid_urn_odbk_mem(v_idx);
	if (shmp == NULL) return Qnil;

	VALUE pairs = rb_ary_new_capa(urn_odbk_mem_cap);
	for (int i=0; i<urn_odbk_mem_cap; i++) {
		if (shmp->pairs[i][0] == '\0') break;
		rb_ary_push(pairs, rb_str_new_cstr(shmp->pairs[i]));
	}
	return pairs;
}

urn_odbk * _get_valid_urn_odbk(VALUE v_idx, VALUE v_pairid) {
	urn_odbk_mem *shmp = _get_valid_urn_odbk_mem(v_idx);
	if (shmp == NULL) return NULL;

	if (RB_TYPE_P(v_pairid, T_FIXNUM) != 1)
		return NULL;
	int pairid = NUM2INT(v_pairid);
	if (pairid < 1 || pairid >= urn_odbk_mem_cap) return NULL;

	urn_odbk *odbk = NULL;
	if (shmp->odbks[pairid][0].complete) {
		odbk = &(shmp->odbks[pairid][0]);
	} else if (shmp->odbks[pairid][1].complete) {
		odbk = &(shmp->odbks[pairid][1]);
	}
	return odbk;
}

VALUE _make_odbk_data_if_newer(VALUE v_idx, VALUE v_pairid, VALUE v_max_row, bool comp_ver) {
	urn_odbk *odbk = _get_valid_urn_odbk(v_idx, v_pairid);
	if (odbk == NULL) return Qnil;

	int exch_idx = NUM2INT(v_idx);
	int pairid = NUM2INT(v_pairid);
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

	int max_row = 99;
	if (RB_TYPE_P(v_idx, T_FIXNUM) != 1)
		max_row = NUM2INT(v_max_row);
	max_row = URN_MIN(99, (int)max_row);
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
		odbk = _get_valid_urn_odbk(v_idx, v_pairid);
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

// Always return shmem odbk without comparing known_data_id
VALUE method_mktdata_odbk(VALUE self, VALUE v_idx, VALUE v_pairid, VALUE v_max_row) {
	return _make_odbk_data_if_newer(v_idx, v_pairid, v_max_row, false);
}

VALUE method_mktdata_new_odbk(VALUE self, VALUE v_idx, VALUE v_pairid, VALUE v_max_row) {
	return _make_odbk_data_if_newer(v_idx, v_pairid, v_max_row, true);
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

	for (int i=0; i<urn_shm_exch_num; i++) {
		shmptr_arr[i] = NULL;
		for (int j=0; j<urn_odbk_mem_cap; j++)
			last_odbk_data_id[i][j] = 0l;
	}
}
