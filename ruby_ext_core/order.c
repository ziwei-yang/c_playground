#include "ruby.h"

#include "order.h"
#include "urn.h"

static VALUE URN_CORE_Order = Qnil;

// Pre-calculate all static rb_str_new_cstr()
static VALUE s_i = Qnil;
static VALUE s_client_oid;
static VALUE s_pair;
static VALUE s_asset;
static VALUE s_currency;
static VALUE s_status;
static VALUE s_T;
static VALUE s_market;
static VALUE s_p;
static VALUE s_s;
static VALUE s_remained;
static VALUE s_executed;
static VALUE s_avg_price;
static VALUE s_fee;
static VALUE s_dv;
static VALUE s_maker_size;
static VALUE s_p_real;
static VALUE s_v;
static VALUE s_executed_v;
static VALUE s_remained_v;
static VALUE s_t;
static VALUE s__buy;
static VALUE s__status_cached;
static VALUE s__alive;
static VALUE s__cancelled;
static VALUE s_maker_buy;
static VALUE s_maker_sell;
static VALUE s_taker_buy;
static VALUE s_taker_sell;
static VALUE s__data;

static void rb_init_order_extension() {
	s_i = rb_str_new_cstr("i");
	rb_global_variable(&s_i);
	s_client_oid = rb_str_new_cstr("client_oid");
	rb_global_variable(&s_client_oid);
	s_pair = rb_str_new_cstr("pair");
	rb_global_variable(&s_pair);
	s_asset = rb_str_new_cstr("asset");
	rb_global_variable(&s_asset);
	s_currency = rb_str_new_cstr("currency");
	rb_global_variable(&s_currency);
	s_status = rb_str_new_cstr("status");
	rb_global_variable(&s_status);
	s_T = rb_str_new_cstr("T");
	rb_global_variable(&s_T);
	s_market = rb_str_new_cstr("market");
	rb_global_variable(&s_market);
	s_p = rb_str_new_cstr("p");
	rb_global_variable(&s_p);
	s_s = rb_str_new_cstr("s");
	rb_global_variable(&s_s);
	s_remained = rb_str_new_cstr("remained");
	rb_global_variable(&s_remained);
	s_executed = rb_str_new_cstr("executed");
	rb_global_variable(&s_executed);
	s_avg_price = rb_str_new_cstr("avg_price");
	rb_global_variable(&s_avg_price);
	s_fee = rb_str_new_cstr("fee");
	rb_global_variable(&s_fee);
	s_dv = rb_str_new_cstr("dv");
	rb_global_variable(&s_dv);
	s_maker_size = rb_str_new_cstr("maker_size");
	rb_global_variable(&s_maker_size);
	s_p_real= rb_str_new_cstr("p_real");
	rb_global_variable(&s_p_real);
	s_v = rb_str_new_cstr("v");
	rb_global_variable(&s_v);
	s_executed_v = rb_str_new_cstr("executed_v");
	rb_global_variable(&s_executed_v);
	s_remained_v = rb_str_new_cstr("remained_v");
	rb_global_variable(&s_remained_v);
	s_t = rb_str_new_cstr("t");
	rb_global_variable(&s_t);
	s__buy = rb_str_new_cstr("_buy");
	rb_global_variable(&s__buy);
	s__status_cached = rb_str_new_cstr("_status_cached");
	rb_global_variable(&s__status_cached);
	s__alive = rb_str_new_cstr("_alive");
	rb_global_variable(&s__alive);
	s__cancelled = rb_str_new_cstr("_cancelled");
	rb_global_variable(&s__cancelled);
	s_maker_buy = rb_str_new_cstr("maker/buy");
	rb_global_variable(&s_maker_buy);
	s_maker_sell = rb_str_new_cstr("maker/sell");
	rb_global_variable(&s_maker_sell);
	s_taker_buy = rb_str_new_cstr("taker/buy");
	rb_global_variable(&s_taker_buy);
	s_taker_sell = rb_str_new_cstr("taker/sell");
	rb_global_variable(&s_taker_sell);
	s__data = rb_str_new_cstr("_data");
	rb_global_variable(&s__data);
}

VALUE rb_order_new(VALUE self) {
	Order *o = ALLOC(Order);
	INIT_ORDER(o);
	// Wrap struct into @_cdata
	rb_ivar_set(self, rb_intern("@_cdata"), Data_Wrap_Struct(URN_CORE_Order, NULL, xfree, o));
	// Al rest unstructured fields in @_data
	rb_ivar_set(self, rb_intern("@_data"), rb_hash_new());
	return self;
}

// Retrieve struct o from Order @_cdata
Order* order_cdata(VALUE order) {
	VALUE _cdata = rb_ivar_get(order, rb_intern("@_cdata"));
	struct Order *o = NULL;
	Data_Get_Struct(_cdata, Order, o);
	return o;
}

#define PARSE_STRING_FROM_VALUE(hash, key, field) \
    if ((temp = rb_hash_aref(hash, key)) != Qnil) { \
        if (RB_TYPE_P(temp, T_STRING)) { \
            strncpy(field, RSTRING_PTR(temp), sizeof(field) - 1); \
	} else { \
            strncpy(field, RSTRING_PTR(rb_funcall(temp, rb_intern("to_s"), 0)), sizeof(field) - 1); \
	} \
    } else { \
        field[0] = '\0'; \
    }

#define PARSE_DOUBLE_FROM_VALUE(hash, key, field) \
    if ((temp = rb_hash_aref(hash, key)) != Qnil) { \
        if (RB_TYPE_P(temp, T_FLOAT) || RB_TYPE_P(temp, T_FIXNUM) || RB_TYPE_P(temp, T_BIGNUM)) { \
            field = NUM2DBL(temp); \
        } else if (RB_TYPE_P(temp, T_STRING)) { \
            field = NUM2DBL(rb_funcall(temp, rb_intern("to_f"), 0)); \
        } else { \
            field = NUM2DBL(rb_funcall(temp, rb_intern("to_f"), 0)); \
            URN_WARNF("value of key [%s] has type [%s] and value [%s], parsed as double %lf", \
                      RSTRING_PTR(key), \
		      rb_obj_classname(temp), \
		      RSTRING_PTR(rb_funcall(temp, rb_intern("inspect"), 0)), \
		      field); \
        } \
    } else { \
        field = -99999.0; \
    }


// Importing Order from Hash values.
void order_from_hash(VALUE hash, Order* o) {
	INIT_ORDER(o);
	VALUE temp;
	// Set fields from hash if present
	PARSE_STRING_FROM_VALUE(hash, s_i, o->i);
	PARSE_STRING_FROM_VALUE(hash, s_client_oid, o->client_oid);
	PARSE_STRING_FROM_VALUE(hash, s_pair, o->pair);
	PARSE_STRING_FROM_VALUE(hash, s_asset, o->asset);
	PARSE_STRING_FROM_VALUE(hash, s_currency, o->currency);
	PARSE_STRING_FROM_VALUE(hash, s_status, o->status);
	PARSE_STRING_FROM_VALUE(hash, s_market, o->market);

	if ((temp = rb_hash_aref(hash, s_T)) != Qnil) {
		strncpy(o->T, RSTRING_PTR(temp), sizeof(o->T) - 1);
		if (strcasecmp(o->T, "BUY") == 0) {
			o->_buy = true; o->_sell = false;
		} else if (strcasecmp(o->T, "SELL") == 0) {
			o->_buy = false; o->_sell = true;
		} else
			URN_WARNF("Unknown order T %s", o->T);
	}

	PARSE_DOUBLE_FROM_VALUE(hash, s_p, o->p);
	PARSE_DOUBLE_FROM_VALUE(hash, s_s, o->s);
	PARSE_DOUBLE_FROM_VALUE(hash, s_remained, o->remained);
	PARSE_DOUBLE_FROM_VALUE(hash, s_executed, o->executed);
	PARSE_DOUBLE_FROM_VALUE(hash, s_avg_price, o->avg_price);
	PARSE_DOUBLE_FROM_VALUE(hash, s_fee, o->fee);
	PARSE_DOUBLE_FROM_VALUE(hash, s_maker_size, o->maker_size);
	PARSE_DOUBLE_FROM_VALUE(hash, s_p_real, o->p_real);
	PARSE_DOUBLE_FROM_VALUE(hash, s_v, o->v);
	PARSE_DOUBLE_FROM_VALUE(hash, s_executed_v, o->executed_v);
	PARSE_DOUBLE_FROM_VALUE(hash, s_remained_v, o->remained_v);

	o->t = 0;
	if ((temp = rb_hash_aref(hash, s_t)) != Qnil) {
		if (RB_TYPE_P(temp, T_FIXNUM) || RB_TYPE_P(temp, T_BIGNUM)) {
			o->t = NUM2ULONG(temp);
			o->t_ns = o->t * 1000;
		} else if (RB_TYPE_P(temp, T_STRING) || RB_TYPE_P(temp, T_FLOAT)) { \
			o->t = NUM2ULONG(rb_funcall(temp, rb_intern("to_i"), 0));
			o->t_ns = (unsigned long)(NUM2DBL(rb_funcall(temp, rb_intern("to_f"), 0)) * 1000);
		} else {
			URN_WARNF("value of key [%s] has type [%s] and value [%s], parsed as ulong",
					RSTRING_PTR(s_t), rb_obj_classname(temp),
					RSTRING_PTR(rb_funcall(temp, rb_intern("inspect"), 0)));
			o->t = NUM2ULONG(rb_funcall(temp, rb_intern("to_i"), 0));
			o->t_ns = (unsigned long)(NUM2DBL(rb_funcall(temp, rb_intern("to_f"), 0)) * 1000);
		}
	}

	if ((temp = rb_hash_aref(hash, s__status_cached)) != Qnil) o->_status_cached = RTEST(temp);
	if ((temp = rb_hash_aref(hash, s__buy)) != Qnil) o->_buy = RTEST(temp);
	if ((temp = rb_hash_aref(hash, s__alive)) != Qnil) o->_alive = RTEST(temp);
	if ((temp = rb_hash_aref(hash, s__cancelled)) != Qnil) o->_cancelled = RTEST(temp);

	// Set default values for fee fields
	o->fee_maker_buy = -99999.0;
	o->fee_maker_sell = -99999.0;
	o->fee_taker_buy = -99999.0;
	o->fee_taker_sell = -99999.0;
	o->dv_maker_buy = -99999.0;
	o->dv_maker_sell = -99999.0;
	o->dv_taker_buy = -99999.0;
	o->dv_taker_sell = -99999.0;

	// Initialize fee dv from nested hash structure
	VALUE data = rb_hash_aref(hash, s__data);
	if (data != Qnil) {
		VALUE fee = rb_hash_aref(data, s_fee);
		if (fee != Qnil) {
			if ((temp = rb_hash_aref(fee, s_maker_buy)) != Qnil) o->fee_maker_buy = NUM2DBL(temp);
			if ((temp = rb_hash_aref(fee, s_maker_sell)) != Qnil) o->fee_maker_sell = NUM2DBL(temp);
			if ((temp = rb_hash_aref(fee, s_taker_buy)) != Qnil) o->fee_taker_buy = NUM2DBL(temp);
			if ((temp = rb_hash_aref(fee, s_taker_sell)) != Qnil) o->fee_taker_sell = NUM2DBL(temp);
		}
		VALUE dv = rb_hash_aref(data, s_dv);
		if (dv != Qnil) {
			if ((temp = rb_hash_aref(dv, s_maker_buy)) != Qnil) o->dv_maker_buy = NUM2DBL(temp);
			if ((temp = rb_hash_aref(dv, s_maker_sell)) != Qnil) o->dv_maker_sell = NUM2DBL(temp);
			if ((temp = rb_hash_aref(dv, s_taker_buy)) != Qnil) o->dv_taker_buy = NUM2DBL(temp);
			if ((temp = rb_hash_aref(dv, s_taker_sell)) != Qnil) o->dv_taker_sell = NUM2DBL(temp);
		}
	}

	return;
}
VALUE rb_new_order_from_hash(VALUE klass, VALUE hash) {
	VALUE obj = rb_obj_alloc(klass);
	rb_funcall(obj, rb_intern("initialize"), 0);

	Order *o = order_cdata(obj);
	order_from_hash(hash, o);

	// rest fields will be stored in @_data
	VALUE data_hash = rb_iv_get(obj, "@_data");
	VALUE keys = rb_funcall(hash, rb_intern("keys"), 0);
	for (int i = 0; i < RARRAY_LEN(keys); ++i) {
		VALUE key = rb_ary_entry(keys, i);
		const char *key_str = RSTRING_PTR(key);

		if (strcmp(key_str, "i") != 0 && strcmp(key_str, "client_oid") != 0 &&
				strcmp(key_str, "pair") != 0 && strcmp(key_str, "asset") != 0 &&
				strcmp(key_str, "currency") != 0 && strcmp(key_str, "status") != 0 &&
				strcmp(key_str, "T") != 0 && strcmp(key_str, "market") != 0 &&
				strcmp(key_str, "p") != 0 && strcmp(key_str, "s") != 0 &&
				strcmp(key_str, "remained") != 0 && strcmp(key_str, "executed") != 0 &&
				strcmp(key_str, "avg_price") != 0 && strcmp(key_str, "fee") != 0 &&
				strcmp(key_str, "maker_size") != 0 && strcmp(key_str, "t") != 0 &&
				strcmp(key_str, "p_real") != 0 && strcmp(key_str, "v") != 0 &&
				strcmp(key_str, "executed_v") != 0 && strcmp(key_str, "remained_v") != 0 &&
				strcmp(key_str, "_buy") != 0 && strcmp(key_str, "_status_cached") != 0 &&
				strcmp(key_str, "_alive") != 0 && strcmp(key_str, "_cancelled") != 0) {
			rb_hash_aset(data_hash, key, rb_hash_aref(hash, key));
		}
	}

	return obj;
}

VALUE rb_order_hashget(VALUE self, VALUE v_key) {
	Order *o = order_cdata(self);
	char *s = RSTRING_PTR(v_key);

	if (strcmp(s, "i") == 0) return rb_str_new_cstr(o->i);
	if (strcmp(s, "client_oid") == 0) return rb_str_new_cstr(o->client_oid);
	if (strcmp(s, "pair") == 0) return rb_str_new_cstr(o->pair);
	if (strcmp(s, "asset") == 0) return rb_str_new_cstr(o->asset);
	if (strcmp(s, "currency") == 0) return rb_str_new_cstr(o->currency);
	if (strcmp(s, "status") == 0) return rb_str_new_cstr(o->status);
	if (strcmp(s, "T") == 0) return rb_str_new_cstr(o->T);
	if (strcmp(s, "market") == 0) return rb_str_new_cstr(o->market);

	if (strcmp(s, "p") == 0) return o->p < 0 ? Qnil : rb_float_new(o->p);
	if (strcmp(s, "s") == 0) return o->s < 0 ? Qnil : rb_float_new(o->s);
	if (strcmp(s, "remained") == 0) return o->remained < 0 ? Qnil : rb_float_new(o->remained);
	if (strcmp(s, "executed") == 0) return o->executed < 0 ? Qnil : rb_float_new(o->executed);
	if (strcmp(s, "avg_price") == 0) return o->avg_price < 0 ? Qnil : rb_float_new(o->avg_price);
	if (strcmp(s, "fee") == 0) return o->fee < 0 ? Qnil : rb_float_new(o->fee);
	if (strcmp(s, "maker_size") == 0) return o->maker_size < 0 ? Qnil : rb_float_new(o->maker_size);
	if (strcmp(s, "p_real") == 0) return o->p_real < 0 ? Qnil : rb_float_new(o->p_real);
	if (strcmp(s, "v") == 0) return o->v < 0 ? Qnil : rb_float_new(o->v);
	if (strcmp(s, "executed_v") == 0) return o->executed_v < 0 ? Qnil : rb_float_new(o->executed_v);
	if (strcmp(s, "remained_v") == 0) return o->remained_v < 0 ? Qnil : rb_float_new(o->remained_v);

	if (strcmp(s, "t") == 0) return ULONG2NUM(o->t);

	if (strcmp(s, "_buy") == 0) return o->_buy ? Qtrue : Qfalse;
	if (strcmp(s, "_status_cached") == 0) return o->_status_cached ? Qtrue : Qfalse;
	if (strcmp(s, "_alive") == 0) return o->_alive ? Qtrue : Qfalse;
	if (strcmp(s, "_cancelled") == 0) return o->_cancelled ? Qtrue : Qfalse;

	// Retrieve from @_data if the key is not recognized
	VALUE _data = rb_iv_get(self, "@_data");
	return rb_hash_aref(_data, v_key);
}

VALUE rb_order_hashset(VALUE self, VALUE v_key, VALUE v_val) {
	Order *o = order_cdata(self);
	char *s = RSTRING_PTR(v_key);

	if (strcmp(s, "i") == 0) {
		if (v_val == Qnil) {
			memset(o->i, 0, sizeof(o->i));
		} else {
			strncpy(o->i, RSTRING_PTR(v_val), sizeof(o->i) - 1);
		}
	} else if (strcmp(s, "client_oid") == 0) {
		if (v_val == Qnil) {
			memset(o->client_oid, 0, sizeof(o->client_oid));
		} else {
			strncpy(o->client_oid, RSTRING_PTR(v_val), sizeof(o->client_oid) - 1);
		}
	} else if (strcmp(s, "pair") == 0) {
		if (v_val == Qnil) {
			memset(o->pair, 0, sizeof(o->pair));
		} else {
			strncpy(o->pair, RSTRING_PTR(v_val), sizeof(o->pair) - 1);
		}
	} else if (strcmp(s, "asset") == 0) {
		if (v_val == Qnil) {
			memset(o->asset, 0, sizeof(o->asset));
		} else {
			strncpy(o->asset, RSTRING_PTR(v_val), sizeof(o->asset) - 1);
		}
	} else if (strcmp(s, "currency") == 0) {
		if (v_val == Qnil) {
			memset(o->currency, 0, sizeof(o->currency));
		} else {
			strncpy(o->currency, RSTRING_PTR(v_val), sizeof(o->currency) - 1);
		}
	} else if (strcmp(s, "status") == 0) {
		if (v_val == Qnil) {
			memset(o->status, 0, sizeof(o->status));
		} else {
			strncpy(o->status, RSTRING_PTR(v_val), sizeof(o->status) - 1);
		}
	} else if (strcmp(s, "T") == 0) {
		if (v_val == Qnil) {
			memset(o->T, 0, sizeof(o->T));
			o->_buy = false; o->_sell = false;
		} else {
			strncpy(o->T, RSTRING_PTR(v_val), sizeof(o->T) - 1);
			if (strcasecmp(o->T, "BUY") == 0) {
				o->_buy = true; o->_sell = false;
			} else if (strcasecmp(o->T, "SELL") == 0) {
				o->_buy = false; o->_sell = true;
			} else
				URN_WARNF("Unknown order T %s", o->T);
		}
	} else if (strcmp(s, "market") == 0) {
		if (v_val == Qnil) {
			memset(o->market, 0, sizeof(o->market));
		} else {
			strncpy(o->market, RSTRING_PTR(v_val), sizeof(o->market) - 1);
		}
	} else if (strcmp(s, "p") == 0) {
		o->p = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "s") == 0) {
		o->s = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "remained") == 0) {
		o->remained = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "executed") == 0) {
		o->executed = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "avg_price") == 0) {
		o->avg_price = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "fee") == 0) {
		o->fee = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "maker_size") == 0) {
		o->maker_size = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "p_real") == 0) {
		o->p_real = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "v") == 0) {
		o->v = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "executed_v") == 0) {
		o->executed_v = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "remained_v") == 0) {
		o->remained_v = (v_val == Qnil) ? -99999 : NUM2DBL(v_val);
	} else if (strcmp(s, "t") == 0) {
		o->t = (v_val == Qnil) ? 0 : NUM2ULONG(v_val);
	} else if (strcmp(s, "_buy") == 0) {
		o->_buy = (v_val == Qnil) ? false : RTEST(v_val);
	} else if (strcmp(s, "_status_cached") == 0) {
		o->_status_cached = (v_val == Qnil) ? false : RTEST(v_val);
	} else if (strcmp(s, "_alive") == 0) {
		o->_alive = (v_val == Qnil) ? false : RTEST(v_val);
	} else if (strcmp(s, "_cancelled") == 0) {
		o->_cancelled = (v_val == Qnil) ? false : RTEST(v_val);
	} else {
		// If the field is not recognized, store it in @_data
		VALUE _data = rb_iv_get(self, "@_data");
		rb_hash_aset(_data, v_key, v_val);
		// Check if the value contains fee data and update fee fields accordingly
		if (TYPE(v_val) == T_HASH) {
			VALUE fee = rb_hash_aref(v_val, s_fee);
			if (fee != Qnil) {
				VALUE fee_maker_buy = rb_hash_aref(fee, s_maker_buy);
				if (fee_maker_buy != Qnil) o->fee_maker_buy = NUM2DBL(fee_maker_buy);

				VALUE fee_maker_sell = rb_hash_aref(fee, s_maker_sell);
				if (fee_maker_sell != Qnil) o->fee_maker_sell = NUM2DBL(fee_maker_sell);

				VALUE fee_taker_buy = rb_hash_aref(fee, s_taker_buy);
				if (fee_taker_buy != Qnil) o->fee_taker_buy = NUM2DBL(fee_taker_buy);

				VALUE fee_taker_sell = rb_hash_aref(fee, s_taker_sell);
				if (fee_taker_sell != Qnil) o->fee_taker_sell = NUM2DBL(fee_taker_sell);
			}
			VALUE dv = rb_hash_aref(v_val, s_dv);
                        if (dv != Qnil) {
                                VALUE dv_maker_buy = rb_hash_aref(dv, s_maker_buy);
                                if (dv_maker_buy != Qnil) o->dv_maker_buy = NUM2DBL(dv_maker_buy);

                                VALUE dv_maker_sell = rb_hash_aref(dv, s_maker_sell);
                                if (dv_maker_sell != Qnil) o->dv_maker_sell = NUM2DBL(dv_maker_sell);

                                VALUE dv_taker_buy = rb_hash_aref(dv, s_taker_buy);
                                if (dv_taker_buy != Qnil) o->dv_taker_buy = NUM2DBL(dv_taker_buy);

                                VALUE dv_taker_sell = rb_hash_aref(dv, s_taker_sell);
                                if (dv_taker_sell != Qnil) o->dv_taker_sell = NUM2DBL(dv_taker_sell);
                        }
		}
	}
	return v_val;
}

void order_to_hash(VALUE hash, Order* o) {
	if (strlen(o->i) > 0) rb_hash_aset(hash, rb_str_new_cstr("i"), rb_str_new_cstr(o->i));
	if (strlen(o->client_oid) > 0) rb_hash_aset(hash, rb_str_new_cstr("client_oid"), rb_str_new_cstr(o->client_oid));
	if (strlen(o->pair) > 0) rb_hash_aset(hash, rb_str_new_cstr("pair"), rb_str_new_cstr(o->pair));
	if (strlen(o->asset) > 0) rb_hash_aset(hash, rb_str_new_cstr("asset"), rb_str_new_cstr(o->asset));
	if (strlen(o->currency) > 0) rb_hash_aset(hash, rb_str_new_cstr("currency"), rb_str_new_cstr(o->currency));
	if (strlen(o->status) > 0) rb_hash_aset(hash, rb_str_new_cstr("status"), rb_str_new_cstr(o->status));
	if (strlen(o->T) > 0) rb_hash_aset(hash, rb_str_new_cstr("T"), rb_str_new_cstr(o->T));
	if (strlen(o->market) > 0) rb_hash_aset(hash, rb_str_new_cstr("market"), rb_str_new_cstr(o->market));

	if (o->p >= 0) rb_hash_aset(hash, rb_str_new_cstr("p"), rb_float_new(o->p));
	if (o->s >= 0) rb_hash_aset(hash, rb_str_new_cstr("s"), rb_float_new(o->s));
	if (o->remained >= 0) rb_hash_aset(hash, rb_str_new_cstr("remained"), rb_float_new(o->remained));
	if (o->executed >= 0) rb_hash_aset(hash, rb_str_new_cstr("executed"), rb_float_new(o->executed));
	if (o->avg_price >= 0) rb_hash_aset(hash, rb_str_new_cstr("avg_price"), rb_float_new(o->avg_price));
	if (o->fee >= 0) rb_hash_aset(hash, rb_str_new_cstr("fee"), rb_float_new(o->fee));
	if (o->maker_size >= 0) rb_hash_aset(hash, rb_str_new_cstr("maker_size"), rb_float_new(o->maker_size));
	if (o->p_real >= 0) rb_hash_aset(hash, rb_str_new_cstr("p_real"), rb_float_new(o->p_real));
	if (o->v >= 0) rb_hash_aset(hash, rb_str_new_cstr("v"), rb_float_new(o->v));
	if (o->executed_v >= 0) rb_hash_aset(hash, rb_str_new_cstr("executed_v"), rb_float_new(o->executed_v));
	if (o->remained_v >= 0) rb_hash_aset(hash, rb_str_new_cstr("remained_v"), rb_float_new(o->remained_v));

	rb_hash_aset(hash, s_t, ULONG2NUM(o->t));

	// Ruby version does not need _status_cached
	// rb_hash_aset(hash, s__status_cached, o->_status_cached ? Qtrue : Qfalse);
	if (o->_status_cached) {
		// Ruby version does not need _buy
		// rb_hash_aset(hash, s__buy, o->_buy ? Qtrue : Qfalse);
		rb_hash_aset(hash, s__alive, o->_alive ? Qtrue : Qfalse);
		rb_hash_aset(hash, s__cancelled, o->_cancelled ? Qtrue : Qfalse);
	}

	// Generate hash['_data']['fee'] with nested keys if fee values are present
	if (o->fee_maker_buy >= 0 || o->fee_taker_buy >= 0 || o->fee_maker_sell >= 0 || o->fee_taker_sell >= 0) {
		VALUE data = rb_hash_new();
		VALUE fee = rb_hash_new();
		if (o->fee_maker_buy >= 0) rb_hash_aset(fee, s_maker_buy, rb_float_new(o->fee_maker_buy));
		if (o->fee_taker_buy >= 0) rb_hash_aset(fee, s_taker_buy, rb_float_new(o->fee_taker_buy));
		if (o->fee_maker_sell >= 0) rb_hash_aset(fee, s_maker_sell, rb_float_new(o->fee_maker_sell));
		if (o->fee_taker_sell >= 0) rb_hash_aset(fee, s_taker_sell, rb_float_new(o->fee_taker_sell));
		rb_hash_aset(data, s_fee, fee);
		VALUE dv = rb_hash_new();
		if (o->dv_maker_buy >= 0) rb_hash_aset(dv, s_maker_buy, rb_float_new(o->dv_maker_buy));
		if (o->dv_taker_buy >= 0) rb_hash_aset(dv, s_taker_buy, rb_float_new(o->dv_taker_buy));
		if (o->dv_maker_sell >= 0) rb_hash_aset(dv, s_maker_sell, rb_float_new(o->dv_maker_sell));
		if (o->dv_taker_sell >= 0) rb_hash_aset(dv, s_taker_sell, rb_float_new(o->dv_taker_sell));
		rb_hash_aset(data, s_dv, dv);
		rb_hash_aset(hash, s__data, data);
	}

	return;
}
VALUE rb_order_to_hash(VALUE self) {
	VALUE _cdata = rb_ivar_get(self, rb_intern("@_cdata"));
	Order *o = NULL;
	Data_Get_Struct(_cdata, Order, o);

	VALUE hash = rb_hash_new();
	order_to_hash(hash, o);

	// Include fields in @_data
	VALUE data_hash = rb_iv_get(self, "@_data");
	VALUE keys = rb_funcall(data_hash, rb_intern("keys"), 0);
	int len = RARRAY_LEN(keys);
	for (int i = 0; i < len; ++i) {
		VALUE key = rb_ary_entry(keys, i);
		VALUE value = rb_hash_aref(data_hash, key);
		rb_hash_aset(hash, key, value);
	}

	return hash;
}

VALUE rb_order_to_json(int argc, VALUE *argv, VALUE self) {
	return rb_funcall2(rb_order_to_hash(self), rb_intern("to_json"), argc, argv);
}

VALUE rb_order_keys(int argc, VALUE *argv, VALUE self) {
    return rb_funcall2(rb_order_to_hash(self), rb_intern("keys"), argc, argv);
}

VALUE rb_order_values(int argc, VALUE *argv, VALUE self) {
    return rb_funcall2(rb_order_to_hash(self), rb_intern("values"), argc, argv);
}

VALUE rb_order_each(int argc, VALUE *argv, VALUE self) {
    return rb_funcall2(rb_order_to_hash(self), rb_intern("each"), argc, argv);
}

VALUE rb_order_to_s(int argc, VALUE *argv, VALUE self) {
    return rb_funcall2(rb_order_to_hash(self), rb_intern("to_s"), argc, argv);
}

void order_bind(VALUE urn_core_module) {
	rb_init_order_extension();
	URN_CORE_Order = rb_define_class_under(urn_core_module, "Order", rb_cObject);
	rb_define_method(URN_CORE_Order, "initialize", rb_order_new, 0);
	rb_define_method(URN_CORE_Order, "[]", rb_order_hashget, 1); // Slower
	rb_define_method(URN_CORE_Order, "[]=", rb_order_hashset, 2); // Slower
	rb_define_method(URN_CORE_Order, "to_h", rb_order_to_hash, 0);
	rb_define_method(URN_CORE_Order, "to_hash", rb_order_to_hash, 0);
	rb_define_method(URN_CORE_Order, "to_json", rb_order_to_json, -1);
	rb_define_method(URN_CORE_Order, "keys", rb_order_keys, -1);
	rb_define_method(URN_CORE_Order, "values", rb_order_values, -1);
	rb_define_method(URN_CORE_Order, "each", rb_order_each, -1);
	rb_define_method(URN_CORE_Order, "to_s", rb_order_to_s, -1);
	// define method Order.from_hash()
	rb_define_singleton_method(URN_CORE_Order, "from_hash", rb_new_order_from_hash, 1);
}
