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
	if (rb_obj_is_kind_of(v_order, rb_path2class("URN_CORE::Order"))) {
		VALUE _cdata = rb_ivar_get(v_order, rb_intern("@_cdata"));
		Data_Get_Struct(_cdata, Order, o);
	} else if (TYPE(v_order) == T_HASH) {
		order_from_hash(v_order, o);
	} else {
		rb_raise(rb_eTypeError, "Expected URN_CORE::Order or Hash");
	}
	return o;
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
	extract_ruby_order(v_order, o);

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
		strncpy(short_status, " √ ", 4);
	} else if (strcmp(trimmed_status, "new") == 0) {
		strncpy(short_status, "NEW", 4);
	} else if (strcmp(trimmed_status, "cancelling") == 0 || strcmp(trimmed_status, "canceling") == 0) {
		strncpy(short_status, "---", 4);
	} else {
		strncpy(short_status, trimmed_status, 3);
		short_status[3] = '\0'; // Ensure null-terminated string
	}
}
void format_trade(Order* o, char* str) {
	char market_account[9];
	char type[2];
	char price[32];
	char executed_s[32];
	char status[4];
	char id[7];
	char trade_time[64];
	char maker_size[4] = "??";
	char formatted_str[256];  // Buffer for the formatted string

	// Market and account
	snprintf(market_account, 9, "%s%s", o->market, o->account);
	market_account[8] = '\0';

	// Type
	if (strlen(o->T) > 0)
		snprintf(type, 2, "%c", toupper(o->T[0]));
	else
		snprintf(type, 2, "?");

	// Price
	format_num(o->p, 10, 5, price);

	// Executed and size
	format_num(o->executed, 5, 5, executed_s);
	char size[32];
	format_num(o->s, 5, 5, size);
	snprintf(executed_s + strlen(executed_s), 32 - strlen(executed_s), "/%s", size);

	// Status
	short_order_status(o->status, status);

	// ID
	if (strlen(o->i) > 0)
		snprintf(id, 7, "%s", o->i);
	else
		snprintf(id, 7, "-");
	id[6] = '\0';

	// Trade time
	format_trade_time(o->t, trade_time);

	// Maker size
	if (o->maker_size != 0 && o->s != 0) {
		if (o->maker_size == o->s) {
			snprintf(maker_size, 3, "  ");
		} else {
			snprintf(maker_size, 3, "Tk");
		}
	} else
		snprintf(maker_size, 3, "??");

	// Construct the formatted string
	snprintf(formatted_str, 256, "%-8s %s %s %s %s %-6s %s %s",
			market_account, type, price, executed_s,
			status, id, trade_time, maker_size);

	// Wrap the formatted string with the appropriate color
	if (strlen(o->T) > 0 && (strcasecmp(o->T, "sell") == 0 || strcasecmp(o->T, "ask") == 0))
		snprintf(str, 256, "%s%s%s", CLR_RED, formatted_str, CLR_RST);
	else
		snprintf(str, 256, "%s%s%s", CLR_GREEN, formatted_str, CLR_RST);
}
VALUE rb_format_trade(int argc, VALUE* argv, VALUE self) {
	VALUE v_order;
	VALUE v_opt;
	rb_scan_args(argc, argv, "11", &v_order, &v_opt);  // 1 required and 1 optional argument

	extract_ruby_order(v_order, o);

	char result[256];
	format_trade(o, result);
	return rb_str_new_cstr(result);
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

	if (strcasecmp(o->status, "filled") == 0)
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
	extract_ruby_order(v_order, o);
	return order_cancelled(o) ? Qtrue : Qfalse;
}
VALUE rb_order_alive(VALUE self, VALUE v_order) {
	extract_ruby_order(v_order, o);
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
    extract_ruby_order(v_order, o);
    return order_canceling(o) ? Qtrue : Qfalse;
}
VALUE rb_order_pending(VALUE self, VALUE v_order) {
    extract_ruby_order(v_order, o);
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
    extract_ruby_order(v_order, o);
    order_set_dead(o);
    return v_order;
}

void order_util_bind(VALUE urn_core_module) {
	VALUE cOrderUtil = rb_define_module_under(urn_core_module, "OrderUtil");
	rb_define_method(cOrderUtil, "is_future?", rb_is_future, 1);
	rb_define_method(cOrderUtil, "pair_assets", rb_pair_assets, 1);
	rb_define_method(cOrderUtil, "parse_contract", rb_parse_contract, 1);
	rb_define_method(cOrderUtil, "format_order", rb_format_order, 1);
	rb_define_method(cOrderUtil, "format_trade_time", rb_format_trade_time, 1);
	rb_define_method(cOrderUtil, "format_trade", rb_format_trade, -1);
	rb_define_method(cOrderUtil, "order_cancelled?", rb_order_cancelled, 1);
	rb_define_method(cOrderUtil, "order_alive?", rb_order_alive, 1);
	rb_define_method(cOrderUtil, "order_canceling", rb_order_canceling, 1);
	rb_define_method(cOrderUtil, "order_pending", rb_order_pending, 1);
	rb_define_method(cOrderUtil, "order_set_dead", rb_order_set_dead, 1);

}
