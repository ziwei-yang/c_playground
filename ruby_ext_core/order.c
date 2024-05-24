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
static VALUE s_maker_size;
static VALUE s_t;
static VALUE s__buy;
static VALUE s__status_cached;
static VALUE s__alive;
static VALUE s__cancelled;
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
	s_maker_size = rb_str_new_cstr("maker_size");
	rb_global_variable(&s_maker_size);
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
}

VALUE rb_order_new(VALUE self) {
	Order *o = ALLOC(Order);
	memset(o, 0, sizeof(Order));
	// Wrap struct into @_cdata
	rb_ivar_set(self, rb_intern("@_cdata"), Data_Wrap_Struct(URN_CORE_Order, NULL, xfree, o));
	// Al rest unstructured fields in @_data
	rb_ivar_set(self, rb_intern("@_data"), rb_hash_new());
	return self;
}

// Retrieve struct o from Order @_cdata
struct Order* order_cdata(VALUE order) {
	VALUE _cdata = rb_ivar_get(order, rb_intern("@_cdata"));
	struct Order *o = NULL;
	Data_Get_Struct(_cdata, Order, o);
	return o;
}

// Importing Order from Hash values.
void order_from_hash(VALUE hash, Order* o) {
	memset(o, 0, sizeof(Order));
	VALUE temp;
	// Set fields from hash if present
	if ((temp = rb_hash_aref(hash, s_i)) != Qnil) strncpy(o->i, RSTRING_PTR(temp), sizeof(o->i) - 1);
	if ((temp = rb_hash_aref(hash, s_client_oid)) != Qnil) strncpy(o->client_oid, RSTRING_PTR(temp), sizeof(o->client_oid) - 1);
	if ((temp = rb_hash_aref(hash, s_pair)) != Qnil) strncpy(o->pair, RSTRING_PTR(temp), sizeof(o->pair) - 1);
	if ((temp = rb_hash_aref(hash, s_asset)) != Qnil) strncpy(o->asset, RSTRING_PTR(temp), sizeof(o->asset) - 1);
	if ((temp = rb_hash_aref(hash, s_currency)) != Qnil) strncpy(o->currency, RSTRING_PTR(temp), sizeof(o->currency) - 1);
	if ((temp = rb_hash_aref(hash, s_status)) != Qnil) strncpy(o->status, RSTRING_PTR(temp), sizeof(o->status) - 1);
	if ((temp = rb_hash_aref(hash, s_T)) != Qnil) strncpy(o->T, RSTRING_PTR(temp), sizeof(o->T) - 1);
	if ((temp = rb_hash_aref(hash, s_market)) != Qnil) strncpy(o->market, RSTRING_PTR(temp), sizeof(o->market) - 1);

	if ((temp = rb_hash_aref(hash, s_p)) != Qnil) o->p = NUM2DBL(temp);
	if ((temp = rb_hash_aref(hash, s_s)) != Qnil) o->s = NUM2DBL(temp);
	if ((temp = rb_hash_aref(hash, s_remained)) != Qnil) o->remained = NUM2DBL(temp);
	if ((temp = rb_hash_aref(hash, s_executed)) != Qnil) o->executed = NUM2DBL(temp);
	if ((temp = rb_hash_aref(hash, s_avg_price)) != Qnil) o->avg_price = NUM2DBL(temp);
	if ((temp = rb_hash_aref(hash, s_fee)) != Qnil) o->fee = NUM2DBL(temp);
	if ((temp = rb_hash_aref(hash, s_maker_size)) != Qnil) o->maker_size = NUM2DBL(temp);
	if ((temp = rb_hash_aref(hash, s_t)) != Qnil) o->t = NUM2ULONG(temp);

	if ((temp = rb_hash_aref(hash, s__status_cached)) != Qnil) o->_status_cached = RTEST(temp);
	if ((temp = rb_hash_aref(hash, s__buy)) != Qnil) o->_buy = RTEST(temp);
	if ((temp = rb_hash_aref(hash, s__alive)) != Qnil) o->_alive = RTEST(temp);
	if ((temp = rb_hash_aref(hash, s__cancelled)) != Qnil) o->_cancelled = RTEST(temp);
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

	if (strcmp(s, "p") == 0) return rb_float_new(o->p);
	if (strcmp(s, "s") == 0) return rb_float_new(o->s);
	if (strcmp(s, "remained") == 0) return rb_float_new(o->remained);
	if (strcmp(s, "executed") == 0) return rb_float_new(o->executed);
	if (strcmp(s, "avg_price") == 0) return rb_float_new(o->avg_price);
	if (strcmp(s, "fee") == 0) return rb_float_new(o->fee);
	if (strcmp(s, "maker_size") == 0) return rb_float_new(o->maker_size);

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
		strcpy(o->i, RSTRING_PTR(v_val));
	} else if (strcmp(s, "client_oid") == 0) {
		strcpy(o->client_oid, RSTRING_PTR(v_val));
	} else if (strcmp(s, "pair") == 0) {
		strcpy(o->pair, RSTRING_PTR(v_val));
	} else if (strcmp(s, "asset") == 0) {
		strcpy(o->asset, RSTRING_PTR(v_val));
	} else if (strcmp(s, "currency") == 0) {
		strcpy(o->currency, RSTRING_PTR(v_val));
	} else if (strcmp(s, "status") == 0) {
		strcpy(o->status, RSTRING_PTR(v_val));
	} else if (strcmp(s, "T") == 0) {
		strcpy(o->T, RSTRING_PTR(v_val));
	} else if (strcmp(s, "market") == 0) {
		strcpy(o->market, RSTRING_PTR(v_val));
	} else if (strcmp(s, "p") == 0) {
		o->p = NUM2DBL(v_val);
	} else if (strcmp(s, "s") == 0) {
		o->s = NUM2DBL(v_val);
	} else if (strcmp(s, "remained") == 0) {
		o->remained = NUM2DBL(v_val);
	} else if (strcmp(s, "executed") == 0) {
		o->executed = NUM2DBL(v_val);
	} else if (strcmp(s, "avg_price") == 0) {
		o->avg_price = NUM2DBL(v_val);
	} else if (strcmp(s, "fee") == 0) {
		o->fee = NUM2DBL(v_val);
	} else if (strcmp(s, "maker_size") == 0) {
		o->maker_size = NUM2DBL(v_val);
	} else if (strcmp(s, "t") == 0) {
		o->t = NUM2ULONG(v_val);
	} else if (strcmp(s, "_buy") == 0) {
		o->_buy = RTEST(v_val);
	} else if (strcmp(s, "_status_cached") == 0) {
		o->_status_cached = RTEST(v_val);
	} else if (strcmp(s, "_alive") == 0) {
		o->_alive = RTEST(v_val);
	} else if (strcmp(s, "_cancelled") == 0) {
		o->_cancelled = RTEST(v_val);
	} else { // Write into @_data if the field is unknown
		rb_hash_aset(rb_iv_get(self, "@_data"), v_key, v_val);
	}

	return Qnil;
}

VALUE rb_order_to_hash(VALUE self) {
	VALUE _cdata = rb_ivar_get(self, rb_intern("@_cdata"));
	Order *order = NULL;
	Data_Get_Struct(_cdata, Order, order);

	VALUE hash = rb_hash_new();

	rb_hash_aset(hash, s_i, rb_str_new_cstr(order->i));
	rb_hash_aset(hash, s_client_oid, rb_str_new_cstr(order->client_oid));
	rb_hash_aset(hash, s_pair, rb_str_new_cstr(order->pair));
	rb_hash_aset(hash, s_asset, rb_str_new_cstr(order->asset));
	rb_hash_aset(hash, s_currency, rb_str_new_cstr(order->currency));
	rb_hash_aset(hash, s_status, rb_str_new_cstr(order->status));
	rb_hash_aset(hash, s_T, rb_str_new_cstr(order->T));
	rb_hash_aset(hash, s_market, rb_str_new_cstr(order->market));

	rb_hash_aset(hash, s_p, rb_float_new(order->p));
	rb_hash_aset(hash, s_s, rb_float_new(order->s));
	rb_hash_aset(hash, s_remained, rb_float_new(order->remained));
	rb_hash_aset(hash, s_executed, rb_float_new(order->executed));
	rb_hash_aset(hash, s_avg_price, rb_float_new(order->avg_price));
	rb_hash_aset(hash, s_fee, rb_float_new(order->fee));
	rb_hash_aset(hash, s_maker_size, rb_float_new(order->maker_size));
	rb_hash_aset(hash, s_t, ULONG2NUM(order->t));

	rb_hash_aset(hash, s__status_cached, order->_status_cached ? Qtrue : Qfalse);
	if (order->_status_cached) {
		rb_hash_aset(hash, s__buy, order->_buy ? Qtrue : Qfalse);
		rb_hash_aset(hash, s__alive, order->_alive ? Qtrue : Qfalse);
		rb_hash_aset(hash, s__cancelled, order->_cancelled ? Qtrue : Qfalse);
	}

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

void order_bind(VALUE urn_core_module) {
	rb_init_order_extension();
	URN_CORE_Order = rb_define_class_under(urn_core_module, "Order", rb_cObject);
	rb_define_method(URN_CORE_Order, "initialize", rb_order_new, 0);
	rb_define_method(URN_CORE_Order, "[]", rb_order_hashget, 1); // Slower
	rb_define_method(URN_CORE_Order, "[]=", rb_order_hashset, 2); // Slower
	rb_define_method(URN_CORE_Order, "to_h", rb_order_to_hash, 0);
	rb_define_method(URN_CORE_Order, "to_hash", rb_order_to_hash, 0);
	rb_define_method(URN_CORE_Order, "to_json", rb_order_to_json, -1);
	// define method Order.from_hash()
	rb_define_singleton_method(URN_CORE_Order, "from_hash", rb_new_order_from_hash, 1);
}
