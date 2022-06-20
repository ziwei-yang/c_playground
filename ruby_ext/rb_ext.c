#include "ruby.h"

#include "../urn.h"

VALUE URN_MKTDATA = Qnil;
urn_odbk_mem *shmptr_arr[urn_shm_exch_num];

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
	int rv = urn_odbk_shm_init(false, urn_shm_exchanges[i], &shmkey, &shmid, &(shmptr_arr[i]));
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

VALUE method_mktdata_odbk(VALUE self, VALUE v_idx, VALUE v_pairid, VALUE v_max_row) {
	urn_odbk *odbk = _get_valid_urn_odbk(v_idx, v_pairid);
	if (odbk == NULL) return Qnil;

	if (RB_TYPE_P(v_max_row, T_FIXNUM) != 1)
		return Qnil;
	int max_row = NUM2INT(v_max_row);
	if (max_row <= 0) return Qnil;
	max_row = URN_MIN(max_row, URN_ODBK_DEPTH);

	// [bids, asks, t, t]
	VALUE r_odbk = rb_ary_new_capa(4);
	VALUE r_bids = rb_ary_new_capa(max_row);
	VALUE r_asks = rb_ary_new_capa(max_row);
	for (int i = 0; i < max_row; i++) {
		if (urn_inum_iszero(&(odbk->bidp[i])) &&
				urn_inum_iszero(&(odbk->askp[i]))) {
			break;
		}
		odbk->bidp[i].s[0] = '\0';
		odbk->bids[i].s[0] = '\0';
		odbk->askp[i].s[0] = '\0';
		odbk->asks[i].s[0] = '\0';
		if (!urn_inum_iszero(&(odbk->bidp[i]))) {
			VALUE r_bid = rb_ary_new_capa(2);
			rb_ary_push(r_bid, DBL2NUM(urn_inum_to_db(&(odbk->bidp[i]))));
			rb_ary_push(r_bid, DBL2NUM(urn_inum_to_db(&(odbk->bids[i]))));
			rb_ary_push(r_bids, r_bid);
		}
		if(!urn_inum_iszero(&(odbk->askp[i]))) {
			VALUE r_ask = rb_ary_new_capa(2);
			rb_ary_push(r_ask, DBL2NUM(urn_inum_to_db(&(odbk->askp[i]))));
			rb_ary_push(r_ask, DBL2NUM(urn_inum_to_db(&(odbk->asks[i]))));
			rb_ary_push(r_asks, r_ask);
		}
	}
	rb_ary_push(r_odbk, r_bids);
	rb_ary_push(r_odbk, r_asks);
	rb_ary_push(r_odbk, LONG2NUM(odbk->w_ts_e6/1000));
	rb_ary_push(r_odbk, LONG2NUM(odbk->mkt_ts_e6/1000));

	return r_odbk;
}

// The initialization method for this module
// Prototype for the initialization method - Ruby calls this, not you
void Init_urn_mktdata() {
	URN_MKTDATA = rb_define_module("URN_MKTDATA");
	rb_define_method(URN_MKTDATA, "mktdata_shm_index", method_mktdata_shm_index, 1);
	rb_define_method(URN_MKTDATA, "mktdata_exch_by_shm_index", method_mktdata_exch, 1);
	rb_define_method(URN_MKTDATA, "mktdata_pairs", method_mktdata_pairs, 1);
	rb_define_method(URN_MKTDATA, "mktdata_odbk", method_mktdata_odbk, 3);

	for (int i=0; i<urn_shm_exch_num; i++)
		shmptr_arr[i] = NULL;
}
