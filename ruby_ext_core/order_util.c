#include "ruby.h"
#include <errno.h>

#include "order.h"
#include "urn.h"
#include "order_util.h"

void format_num(double num, int fraclen, int decilen, char* str); // in util.c
void urn_s_trim(const char* str, char* new_s); // in urn.h

// in order.c
void order_from_hash(VALUE hash, Order* o);

static Order* _attach_or_parse_ruby_order(VALUE v_order, Order *o) {
	if (rb_obj_is_kind_of(v_order, rb_path2class("URN_CORE::Order")))
		Data_Get_Struct(rb_ivar_get(v_order, rb_intern("@_cdata")), Order, o);
	else if (TYPE(v_order) == T_HASH)
		order_from_hash(v_order, o);
	else
		rb_raise(rb_eTypeError, "Expected URN_CORE::Order or Hash");
	return o;
}

// attach_or_parse_ruby_order, creates Order _temp_o in stack space.
#define attach_or_parse_ruby_order(v_order, opvar) \
        Order _temp_o; INIT_ORDER(&_temp_o); \
        Order* opvar = _attach_or_parse_ruby_order(v_order, &_temp_o);
// attach_or_parse_ruby_order, uses customized _temp_stackspace_o
#define attach_or_parse_ruby_order2(v_order, opvar, _temp_stackspace_o) \
        INIT_ORDER(&_temp_stackspace_o); \
        Order* opvar = _attach_or_parse_ruby_order(v_order, &_temp_stackspace_o);

// attach_or_parse_ruby_order, array version, creates Order _temp_orders[] in stack space.
#define attach_or_parse_ruby_order_array(v_order_array, o_array_var) \
	long _v_order_array_len = RARRAY_LEN(v_order_array); \
	Order _temp_orders[_v_order_array_len]; \
	memset(_temp_orders, 0, sizeof(Order) * _v_order_array_len); \
	Order* o_array_var[_v_order_array_len + 1]; \
	for (long i = 0; i < _v_order_array_len; i++) { \
		VALUE v_order = rb_ary_entry(v_orders, i); \
		INIT_ORDER(&(_temp_orders[i])); \
		o_array_var[i] = _attach_or_parse_ruby_order(v_order, &(_temp_orders[i])); \
	} \
	o_array_var[_v_order_array_len] = NULL;

// Copy order back to v_order @_cdata, if v_order is URN_CORE::Order
// Copy order back to v_order fields, if v_order is Hash
static void write_to_ruby_order(VALUE v_order, Order *o) {
	if (rb_obj_is_kind_of(v_order, rb_path2class("URN_CORE::Order"))) {
		VALUE _cdata = rb_ivar_get(v_order, rb_intern("@_cdata"));
		Order *order_data;
		Data_Get_Struct(_cdata, Order, order_data);
		if (order_data == o)
			return; // Do nothing if o is already the data inside URN_CORE::Order instance
		*order_data = *o; // Copy the entire structure
	} else if (TYPE(v_order) == T_HASH) {
		VALUE hash = rb_hash_new();
		order_to_hash(hash, o);
		rb_funcall(v_order, rb_intern("merge!"), 1, hash);
	} else {
		rb_raise(rb_eTypeError, "Expected URN_CORE::Order or Hash");
	}
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 * 	def is_future?(pair)
 */
bool is_future(const char* pair) {
	return strstr(pair, "@") != NULL;
}
VALUE rb_is_future(VALUE self, VALUE v_pair) {
	Check_Type(v_pair, T_STRING);
	const char* pair_str = StringValueCStr(v_pair);
	return (is_future(pair_str) ? Qtrue : Qfalse);
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 * 	def pair_assets(pair)
 */
int pair_assets(const char* pair, char* asset1, char* asset2) {
	if (!pair || !asset1 || !asset2) return EINVAL;

	// Copy the pair to a local buffer to avoid modifying the original string
	char pair_copy[strlen(pair) + 1];
	strcpy(pair_copy, pair);

	// Split the pair by '-'
	char* dash_pos = strchr(pair_copy, '-');
	if (dash_pos == NULL) return EINVAL;

	size_t asset1_len = dash_pos - pair_copy;
	strncpy(asset1, pair_copy, asset1_len);
	asset1[asset1_len] = '\0'; // Null-terminate
	strcpy(asset2, dash_pos + 1); // Copy asset2

	// Ensure there is only one '-'
	if (strchr(asset2, '-') != NULL) return EINVAL;
	return 0;
}
VALUE rb_pair_assets(VALUE self, VALUE v_pair) {
	Check_Type(v_pair, T_STRING);
	const char* pair_str = StringValueCStr(v_pair);

	char asset1[128];
	char asset2[128];

	int result = pair_assets(pair_str, asset1, asset2);
	if (result != 0)
		rb_raise(rb_eArgError, "Invalid pair %s", pair_str);

	VALUE ruby_array = rb_ary_new();

	if (is_future(pair_str)) {
		// Future pair: return [asset1, pair]
		rb_ary_push(ruby_array, rb_str_new_cstr(asset1));
		rb_ary_push(ruby_array, v_pair);
	} else {
		// Spot pair: return [asset1, asset2]
		rb_ary_push(ruby_array, rb_str_new_cstr(asset1));
		rb_ary_push(ruby_array, rb_str_new_cstr(asset2));
	}

	return ruby_array;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 * 	def parse_contract(contract)
 */
int parse_contract(const char* contract, char* asset1, char* asset2, char* expiry) {
	if (!contract || !asset1 || !asset2 || !expiry)
		return EINVAL;

	// Copy the contract_str to a local buffer to avoid modifying the original string
	char contract_copy[strlen(contract) + 1];
	strcpy(contract_copy, contract);

	// Split the contract by '@'
	char* at_pos = strchr(contract_copy, '@');
	if (at_pos != NULL) {
		*at_pos = '\0'; // Temporarily terminate the string at '@'
		strcpy(expiry, at_pos + 1); // Copy expiry
	} else
		expiry[0] = '\0'; // Set expiry to an empty string

	// Split the remaining part by '-'
	char* dash_pos = strchr(contract_copy, '-');
	if (dash_pos == NULL)
		return EINVAL;

	size_t asset1_len = dash_pos - contract_copy;
	strncpy(asset1, contract_copy, asset1_len);
	asset1[asset1_len] = '\0'; // Null-terminate
	strcpy(asset2, dash_pos + 1); // Copy asset2

	// Ensure there is only one '-'
	if (strchr(asset2, '-') != NULL)
		return EINVAL;

	return 0;
}
VALUE rb_parse_contract(VALUE self, VALUE v_contract) {
	Check_Type(v_contract, T_STRING);
	const char* contract_str = StringValueCStr(v_contract);

	char asset1[128];
	char asset2[128];
	char expiry[128];

	int result = parse_contract(contract_str, asset1, asset2, expiry);
	if (result != 0)
		rb_raise(rb_eArgError, "Invalid contract %s", contract_str);

	VALUE ruby_array = rb_ary_new();
	rb_ary_push(ruby_array, rb_str_new_cstr(asset1));
	rb_ary_push(ruby_array, rb_str_new_cstr(asset2));
	if (strlen(expiry) > 0)
		rb_ary_push(ruby_array, rb_str_new_cstr(expiry));
	else
		rb_ary_push(ruby_array, Qnil);

	return ruby_array;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 * 	def format_order(o)
 */
void format_order(char* buffer, size_t size, const struct Order* o) {
	char p_str[32];
	char s_str[32];
	format_num(o->p, 10, 4, p_str);
	format_num(o->s, 8, 8, s_str);
	snprintf(buffer, size, "%-5s%s%s", o->T, p_str, s_str);
}
VALUE rb_format_order(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);

	char buffer[256];
	format_order(buffer, sizeof(buffer), o);
	return rb_str_new_cstr(buffer);
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def format_trade_time(t)
 */
void format_trade_time(unsigned long t, char *s) {
	if (t == 0) {
		snprintf(s, 64, "no-time");
		return;
	}

	// Add 8 hours in milliseconds
	t += 8 * 3600 * 1000;

	// Convert milliseconds to seconds
	time_t seconds = t / 1000;

	// Convert to tm struct
	struct tm *tm_info = gmtime(&seconds);

	// Format the time
	strftime(s, 64, "%H:%M:%S %y-%m-%d", tm_info);
}
VALUE rb_format_trade_time(VALUE self, VALUE v_t) {
	if (NIL_P(v_t)) {
		return rb_str_new_cstr("no-time");
	}

	unsigned long t = NUM2ULONG(v_t);
	char formatted_time[64];
	format_trade_time(t, formatted_time);

	return rb_str_new_cstr(formatted_time);
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def short_order_status(status)
 *	def format_trade(t, opt={})
 */
void short_order_status(const char* status, char* short_status) {
	char trimmed_status[64]; // 64 is enough for order status
	urn_s_trim(status, trimmed_status);

	if (strcmp(trimmed_status, "rejected") == 0) {
		strncpy(short_status, "XXX", 4);
	} else if (strcmp(trimmed_status, "canceled") == 0) {
		strncpy(short_status, " - ", 4);
	} else if (strcmp(trimmed_status, "finished") == 0 || strcmp(trimmed_status, "filled") == 0) {
		strncpy(short_status, " √ ", 6); // ticker is a 3-byte UTF8 char
	} else if (strcmp(trimmed_status, "new") == 0) {
		strncpy(short_status, "NEW", 4);
	} else if (strcmp(trimmed_status, "cancelling") == 0 || strcmp(trimmed_status, "canceling") == 0) {
		strncpy(short_status, ".x.", 4);
	} else {
		strncpy(short_status, trimmed_status, 3);
		short_status[3] = '\0'; // Ensure null-terminated string
	}
}
void format_trade(Order* o, char* str) {
	if (o == NULL) {
		snprintf(str, 256, "Null trade");
		return;
	}

	char market_account[16] = {0};
	snprintf(market_account, sizeof(market_account), "%-8s", o->market);

	char t_type[16] = {0};
	snprintf(t_type, sizeof(t_type), "%-4s", o->T);
	t_type[0] = toupper(t_type[0]);

	char p_str[32] = {0};
	format_num(o->p >= 0 ? o->p : 0, 10, 5, p_str);

	char executed_str[32] = {0};
	format_num(o->executed >= 0 ? o->executed : 0, 5, 5, executed_str);

	char s_str[32] = {0};
	format_num(o->s >= 0 ? o->s : 0, 5, 5, s_str);

	char status[16] = {0};
	short_order_status(o->status, status);

	char id_str[64] = {0};
	snprintf(id_str, sizeof(id_str), "%-6s", strlen(o->i) >= 6 ? o->i + strlen(o->i) - 6 : o->i);

	char id_ljust[64] = {0};
	snprintf(id_ljust, sizeof(id_ljust), "%-6s", id_str);

	char time_str[64] = {0};
	format_trade_time(o->t, time_str);

	char maker_size_str[32] = "??";
	if (o->maker_size >= 0 && o->s >= 0) {
		if (o->maker_size == o->s) {
			strcpy(maker_size_str, "  ");
		} else {
			strcpy(maker_size_str, "Tk");
		}
	}

	char result[512] = {0};

	if (o->v >= 0) {
		char executed_v_str[32] = {0};
		format_num(o->executed_v >= 0 ? o->executed_v : 0, 1, 7, executed_v_str);

		char v_str[32] = {0};
		format_num(o->v >= 0 ? o->v : 0, 1, 7, v_str);

		char vol_info[64] = {0};
		format_num(o->executed >= 0 ? o->executed : 0, 4, 2, executed_str);
		snprintf(vol_info, sizeof(executed_str), "S:%s", executed_str);

		char expiry_info[16] = {0};
		char* at_pos = strchr(o->pair, '@');
		if (at_pos != NULL) {
			if (strcmp(at_pos + 1, "P") == 0) {
				strcpy(expiry_info, "@P");
			} else {
				snprintf(expiry_info, sizeof(expiry_info), "@%.4s", at_pos + 1 + strlen(at_pos + 1) - 4);
			}
		}

		snprintf(vol_info + strlen(vol_info), sizeof(vol_info) - strlen(vol_info), "%s", expiry_info);
		snprintf(result, sizeof(result), "%-8s %c %-10s %-9s/%-9s %s %-5s %s %s %s",
				market_account, t_type[0], p_str, executed_v_str,
				v_str, status, id_ljust, time_str, maker_size_str, vol_info);
	} else {
		snprintf(result, sizeof(result), "%-8s %c %-10s %-11s/%-10s %s %-5s %s %s",
				market_account, t_type[0], p_str, executed_str,
				s_str, status, id_ljust, time_str, maker_size_str);
	}

	// Handle color based on order type
	if (strcasecmp(o->T, "sell") == 0 || strcasecmp(o->T, "ask") == 0) {
		snprintf(str, 256, "%s%s%s", CLR_RED, result, CLR_RST);
	} else {
		snprintf(str, 256, "%s%s%s", CLR_GREEN, result, CLR_RST);
	}
}
static ID id_force_encoding;
static VALUE encoding_utf8;
VALUE rb_format_trade(int argc, VALUE* argv, VALUE self) {
	VALUE v_order;
	VALUE v_opt;
	rb_scan_args(argc, argv, "11", &v_order, &v_opt);  // 1 required and 1 optional argument
	if (NIL_P(v_order)) return rb_str_new_cstr("Null trade");

	attach_or_parse_ruby_order(v_order, o);

	char result[256];
	format_trade(o, result);

	// force_encoding(Encoding::UTF_8)
	VALUE rb_result = rb_str_new_cstr(result);
	rb_funcall(rb_result, id_force_encoding, 1, encoding_utf8);
	return rb_result;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_cancelled?(o)
 *	def order_alive?(o)
 */
static int order_calculate_status(Order* o) {
	// If _status_cached is already set, return
	if (o->_status_cached) return 0;
	o->_status_cached = false;

	if (o->status[0] == '\0' || o->T[0] == '\0')
		return EINVAL;

	if (strcasecmp(o->status, "canceled") == 0)
		o->_cancelled = true;
	else
		o->_cancelled = false;

	if (o->remained == 0)
		o->_alive = false;
	else if (o->status[0] == '\0')
		o->_alive = false;
	else if (strcasecmp(o->status, "filled") == 0)
		o->_alive = false;
	else if (strcasecmp(o->status, "canceled") == 0)
		o->_alive = false;
	else if (strcasecmp(o->status, "expired") == 0)
		o->_alive = false;
	else if (strcasecmp(o->status, "rejected") == 0)
		o->_alive = false;
	else
		o->_alive = true;

	if (strcasecmp(o->T, "buy") == 0)
		o->_buy = true;
	else if (strcasecmp(o->T, "sell") == 0)
		o->_buy = false;
	else
		return EINVAL;

	o->_status_cached = true;
	return 0;
}
bool order_cancelled(Order* o) {
	if (!o->_status_cached) order_calculate_status(o);
	return o->_cancelled;
}
bool order_alive(Order* o) {
	if (!o->_status_cached) order_calculate_status(o);
	return o->_alive;
}
VALUE rb_order_cancelled(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);
	return order_cancelled(o) ? Qtrue : Qfalse;
}
VALUE rb_order_alive(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);
	return order_alive(o) ? Qtrue : Qfalse;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_canceling?(o)
 *	def order_pending?(o)
 */
inline bool order_canceling(Order* o) { return strcmp(o->status, "canceling") == 0; }
inline bool order_pending(Order* o) { return strcmp(o->status, "pending") == 0; }
VALUE rb_order_canceling(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);
	return order_canceling(o) ? Qtrue : Qfalse;
}
VALUE rb_order_pending(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);
	return order_pending(o) ? Qtrue : Qfalse;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_set_dead(o)
 */
void order_set_dead(Order *o) {
	if (o->_status_cached && !o->_alive) return;
	if (o->remained == 0.0)
		strncpy(o->status, "filled", sizeof(o->status) - 1);
	else
		strncpy(o->status, "canceled", sizeof(o->status) - 1);
	o->_status_cached = false;
	order_calculate_status(o);
}
VALUE rb_order_set_dead(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);
	order_set_dead(o);
	write_to_ruby_order(v_order, o);
	return v_order;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_status_evaluate(o)
 */
void order_status_evaluate(Order *o) {
	if (o->v < 0) {
		o->s = urn_round(o->s, 10);
		o->executed = urn_round(o->executed, 10);
		o->remained = urn_round(o->remained, 10);
		o->p = urn_round(o->p, 10);
		if (strcmp(o->status, "pending") == 0) {
			// Pending order might have no maker_size
			if (o->maker_size >= 0)
				o->maker_size = urn_round(o->maker_size, 10);
		} else
			o->maker_size = urn_round(o->maker_size, 10);
	}

	o->_status_cached = false;
	order_calculate_status(o);
}
VALUE rb_order_status_evaluate(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);
	order_status_evaluate(o);
	write_to_ruby_order(v_order, o);
	return v_order;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_age(t)
 */
// order age in ms
long order_age(Order* o) {
	if (o == NULL) return -1;

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long current_time_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	return current_time_ms - o->t;
}
VALUE rb_order_age(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);
	long age = order_age(o);
	return LONG2NUM(age);
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_full_filled(t, opt)
 */
bool order_full_filled(Order* t, double omit_size) {
	if (t == NULL) return false;
	if (t->remained <= omit_size) return true;
	if (t->s - t->executed <= omit_size) return true;
	return false;
}
static VALUE sym_omit_size;
VALUE rb_order_full_filled(int argc, VALUE* argv, VALUE self) {
	VALUE v_order, v_opt;
	rb_scan_args(argc, argv, "11", &v_order, &v_opt);  // 1 required and 1 optional argument
	attach_or_parse_ruby_order(v_order, o);

	double omit_size = 0;
	if (!NIL_P(v_opt) && rb_hash_lookup(v_opt, sym_omit_size) != Qnil)
		omit_size = NUM2DBL(rb_hash_lookup(v_opt, sym_omit_size));
	bool is_filled = order_full_filled(o, omit_size);
	return is_filled ? Qtrue : Qfalse;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_same?(o1, o2)
 */
bool order_same(Order* o1, Order* o2) {
	if (o1 == NULL || o2 == NULL) return false;
	if (o1 == o2) return true;
	if (strcmp(o1->market, o2->market) != 0) return false;
	if (strcmp(o1->market, "Gemini") == 0 && strcmp(o2->market, "Gemini") == 0 && (strlen(o1->pair) == 0 || strlen(o2->pair) == 0)) {
		// Allow o1 / o2 without pair info, Gemini allow querying order without pair
	} else {
		if (strcmp(o1->pair, o2->pair) != 0) return false;
	}

	if (strlen(o1->T) != 0 && strlen(o2->T) != 0)
		if (strcmp(o1->T, o2->T) != 0)
			return false;

	// Some exchanges do not assign order id for instant filled orders.
	// We could identify them only by price-size-timestamp
	if (strlen(o1->i) > 0 && strlen(o2->i) > 0) {
		return strcmp(o1->i, o2->i) == 0;
	} else if (strlen(o1->client_oid) != 0 && strlen(o2->client_oid) != 0) {
		if (o1->s >= 0 && o2->s >= 0) {
			return (o1->s == o2->s) && (o1->p == o2->p);
		} else if (o1->v >= 0 && o2->v >= 0) {
			return (o1->v == o2->v) && (o1->p == o2->p);
		}
	}

	return (o1->s == o2->s) && (o1->p == o2->p) && (o1->t) == (o2->t);
}
VALUE rb_order_same(VALUE self, VALUE v_o1, VALUE v_o2) {
	if (NIL_P(v_o1) || NIL_P(v_o2)) return Qfalse;

	Order temp_o1, temp_o2;
	attach_or_parse_ruby_order2(v_o1, o1, temp_o1);
	attach_or_parse_ruby_order2(v_o2, o2, temp_o2);

	return order_same(o1, o2) ? Qtrue : Qfalse;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_changed?(o1, o2)
 */
bool order_changed(Order* o1, Order* o2) {
	if (!order_same(o1, o2)) {
		URN_WARN("Order are not same one");
		ORDER_LOG(o1);
		ORDER_LOG(o2);
		return false;
	}
	if (o1 == o2) return false; // Both pointers are the same, no change
	if (strcmp(o1->status, o2->status) != 0) return true; // Status changed
	if (o1->executed != o2->executed) return true; // Executed amount changed
	if (o1->remained != o2->remained) return true; // Remained amount changed
	return false;
}
VALUE rb_order_changed(VALUE self, VALUE v_o1, VALUE v_o2) {
	Order temp_o1, temp_o2;
	attach_or_parse_ruby_order2(v_o1, o1, temp_o1);
	attach_or_parse_ruby_order2(v_o2, o2, temp_o2);
	return order_changed(o1, o2) ? Qtrue : Qfalse;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_should_update?(o1, o2)
 */
bool order_should_update(Order* o1, Order* o2) {
	if (!order_same(o1, o2)) {
		URN_WARN("Order are not same one");
		ORDER_LOG(o1);
		ORDER_LOG(o2);
		return false;
	}

	bool o1_alive = order_alive(o1);
	bool o2_alive = order_alive(o2);

	if (o1_alive && o2_alive) {
		// Better replace if status: 'pending' -> 'new'
		if (order_pending(o1) && o1->executed <= o2->executed)
			return true;
		return o1->executed < o2->executed;
	} else if (o1_alive && !o2_alive) {
		return true;
	} else if (!o1_alive && o2_alive) {
		return false;
	} else if (!o1_alive && !o2_alive) {
		return o1->executed < o2->executed;
	}
	URN_WARN("Should not come here");
	return false;
}
VALUE rb_order_should_update(VALUE self, VALUE v_o1, VALUE v_o2) {
	Order temp_o1, temp_o2;
	attach_or_parse_ruby_order2(v_o1, o1, temp_o1);
	attach_or_parse_ruby_order2(v_o2, o2, temp_o2);
	return order_should_update(o1, o2) ? Qtrue : Qfalse;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_stat(orders, opt={})
 */
int order_stat(Order* orders, int precise, double* result) {
	if (orders == NULL || result == NULL) {
		result[0] = 0.0;
		result[1] = 0.0;
		result[2] = 0.0;
		return 0;
	}

	double orders_size_sum = 0.0;
	double orders_executed_sum = 0.0;
	for (Order* o = orders; o != NULL && o->i[0] != '\0'; o++) {
		if (o->s < 0 || o->executed < 0) {
			URN_WARN("Error order, no size or executed\n");
			ORDER_LOG(o);
			return EINVAL;
		}
		if (order_alive(o))
			orders_size_sum += o->s;
		else
			orders_size_sum += o->executed;
		orders_executed_sum += o->executed;
	}

	double orders_remained_sum = orders_size_sum - orders_executed_sum;

	// Round the results
	double factor = pow(10, precise);
	result[0] = round(orders_size_sum * factor) / factor;
	result[1] = round(orders_executed_sum * factor) / factor;
	result[2] = round(orders_remained_sum * factor) / factor;
	return 0;
}
VALUE rb_order_stat(int argc, VALUE* argv, VALUE self) {
	VALUE v_orders, v_opt;
	rb_scan_args(argc, argv, "11", &v_orders, &v_opt); // 1 required and 1 optional argument

	// Check if orders is an array
	Check_Type(v_orders, T_ARRAY);

	int precise = 1;
	if (!NIL_P(v_opt)) {
		VALUE v_precise = rb_hash_aref(v_opt, ID2SYM(rb_intern("precise")));
		if (!NIL_P(v_precise)) {
			precise = NUM2INT(v_precise);
		}
	}

	attach_or_parse_ruby_order_array(v_orders, orders);

	// Perform the calculation
	double result[3];
	order_stat(orders[0], precise, result);

	// Return the result as an array
	VALUE rb_result = rb_ary_new();
	rb_ary_push(rb_result, rb_float_new(result[0]));
	rb_ary_push(rb_result, rb_float_new(result[1]));
	rb_ary_push(rb_result, rb_float_new(result[2]));
	return rb_result;
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_real_vol(o)
 */
double order_real_vol(Order* o) {
	if (o->executed <= 0) return 0.0;

	double maker_size = fmin(o->executed, o->maker_size >= 0.0 ? o->maker_size : 0.0);
	double taker_size = o->executed - maker_size;
	const char* type = o->T;

	double maker_fee;
	double taker_fee;

	if (o->_buy) {
		maker_fee = o->fee_maker_buy;
		taker_fee = o->fee_taker_buy;
	} else if (o->_sell) {
		maker_fee = o->fee_maker_sell;
		taker_fee = o->fee_taker_sell;
	} else {
		URN_WARNF("Unknown order type: %s", type);
		ORDER_LOG(o);
		return 0;
	}

	if (maker_fee < 0 || taker_fee < 0) {
		URN_WARN("Missing fee data for order");
		ORDER_LOG(o);
		return 0;
	}

	double fee = o->p * maker_size * maker_fee + o->p * taker_size * taker_fee;
	double vol = o->p * o->executed;

	if (o->_buy) { return vol + fee; }
	else if (o->_sell) { return vol - fee; }
	else { return 0; }
}
VALUE rb_order_real_vol(VALUE self, VALUE v_order) {
	attach_or_parse_ruby_order(v_order, o);
	double real_vol = order_real_vol(o);
	return rb_float_new(real_vol);
}

/* Replacement of:
 * bootstrap.rb/OrderUtil
 *	def order_same_mkt_pair?(orders)
 */
bool order_same_mkt_pair(Order* orders[]) {
	if (orders == NULL) return true;
	if (orders[0] == NULL) return true;
	if (orders[1] == NULL) return true;
	const char* market = orders[0]->market;
	const char* pair = orders[0]->pair;
	// Iterate over the orders to check if they all have the same market and pair
	for (int i = 1; orders[i] != NULL; i++) {
		if (strcmp(market, orders[i]->market) != 0 || strcmp(pair, orders[i]->pair) != 0) {
			return false;
		}
	}
	return true;
}
VALUE rb_order_same_mkt_pair(VALUE self, VALUE v_orders) {
	Check_Type(v_orders, T_ARRAY);
	attach_or_parse_ruby_order_array(v_orders, orders);
	return order_same_mkt_pair(orders) ? Qtrue : Qfalse;
}

/////////////// RUBY interface below ///////////////
void order_util_bind(VALUE urn_core_module) {
	// Init static resources
	sym_omit_size = ID2SYM(rb_intern("omit_size"));
	id_force_encoding = rb_intern("force_encoding");
	encoding_utf8 = rb_const_get(rb_const_get(rb_cObject, rb_intern("Encoding")), rb_intern("UTF_8"));

	VALUE cOrderUtil = rb_define_module_under(urn_core_module, "OrderUtil");
	rb_define_method(cOrderUtil, "is_future?", rb_is_future, 1);
	rb_define_method(cOrderUtil, "pair_assets", rb_pair_assets, 1);
	rb_define_method(cOrderUtil, "parse_contract", rb_parse_contract, 1);
	rb_define_method(cOrderUtil, "format_order", rb_format_order, 1);
	rb_define_method(cOrderUtil, "format_trade_time", rb_format_trade_time, 1);
	rb_define_method(cOrderUtil, "format_trade", rb_format_trade, -1);
	rb_define_method(cOrderUtil, "order_cancelled?", rb_order_cancelled, 1);
	rb_define_method(cOrderUtil, "order_alive?", rb_order_alive, 1);
	rb_define_method(cOrderUtil, "order_canceling?", rb_order_canceling, 1);
	rb_define_method(cOrderUtil, "order_pending", rb_order_pending, 1);
//	rb_define_method(cOrderUtil, "order_set_dead", rb_order_set_dead, 1);
//	rb_define_method(cOrderUtil, "order_status_evaluate", rb_order_status_evaluate, 1);
	rb_define_method(cOrderUtil, "order_age", rb_order_age, 1);
	rb_define_method(cOrderUtil, "order_full_filled?", rb_order_full_filled, -1);
	rb_define_method(cOrderUtil, "order_same?", rb_order_same, 2);
	rb_define_method(cOrderUtil, "order_changed?", rb_order_changed, 2);
	rb_define_method(cOrderUtil, "order_should_update?", rb_order_should_update, 2);
	rb_define_method(cOrderUtil, "order_stat", rb_order_stat, -1);
	rb_define_method(cOrderUtil, "order_real_vol", rb_order_real_vol, 1);
	rb_define_method(cOrderUtil, "order_same_mkt_pair?", rb_order_same_mkt_pair, 1);
}
