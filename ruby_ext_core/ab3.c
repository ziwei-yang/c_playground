#include "ruby.h"
#include "math.h"
#include <stdarg.h>

#include "../urn.h"

double urn_round(double f, int precision);

/* pre-calculate String -> SymID */
bool ab3_inited = false;
#define max_odbk_depth 9
ID id_valid_markets;
ID id_size;
ID id_dig;
ID id_order_alive_qm;
ID id_market_client;
ID id_max_order_size;
ID id_equalize_order_size;
ID id_downgrade_order_size_for_all_market;
ID id_price_step;
ID id_price_ratio_range;
ID id_object_id;
ID id_balance_ttl_cash_asset;
ID id_future_side_for_close;
VALUE sym_orderbook;
VALUE sym_own_main_orders;
VALUE sym_own_spike_main_orders;
VALUE sym_own_live_orders;
VALUE sym_own_legacy_orders;
VALUE sym_p_real;
VALUE sym_price;
VALUE sym_type;
VALUE sym_market;
VALUE sym_child_type;
VALUE sym_child_price_threshold;
VALUE sym_cata;
VALUE sym_aggressive;
VALUE sym_suggest_size;
VALUE sym_explain;
VALUE sym_main_orders;
VALUE sym_ideal_profit;
VALUE sym_yes, sym_no, sym_active, sym_retain, sym_size;
ID id_my_market_data_agt;
ID id_my_markets;
ID id_my_keep_exist_spike_order;
ID id_my_market_snapshot;
ID id_my_arbitrage_min;
ID id_my_pair;
ID id_my_single_vol_ratio;
ID id_my_vol_max;
ID id_my_vol_min;
ID id_my_vol_omit;
ID id_my_last_order_arbitrage_min;
ID id_my_run_mode;
ID id_my_spike_catcher_enabled;
ID id_my_avg_last;
ID id_my_need_balance_refresh;
ID id_my_main_order_min_num_map;
ID id_my_low_balance_market;
ID id_my_deviation_map;
ID id_my_debug;
void ab3_init() {
	if (ab3_inited) return;
	id_valid_markets = rb_intern("valid_markets");
	id_size = rb_intern("size");
	id_dig = rb_intern("dig");
	id_order_alive_qm = rb_intern("order_alive?");
	id_market_client = rb_intern("market_client");
	id_max_order_size = rb_intern("max_order_size");
	id_equalize_order_size = rb_intern("equalize_order_size");
	id_downgrade_order_size_for_all_market = rb_intern("downgrade_order_size_for_all_market");
	id_price_step = rb_intern("price_step");
	id_price_ratio_range = rb_intern("price_ratio_range");
	id_object_id = rb_intern("object_id");
	id_balance_ttl_cash_asset = rb_intern("balance_ttl_cash_asset");
	id_future_side_for_close = rb_intern("future_side_for_close");

	sym_orderbook = ID2SYM(rb_intern("orderbook"));
	sym_own_main_orders = ID2SYM(rb_intern("own_main_orders"));
	sym_own_spike_main_orders = ID2SYM(rb_intern("own_spike_main_orders"));
	sym_own_live_orders = ID2SYM(rb_intern("own_live_orders"));
	sym_own_legacy_orders = ID2SYM(rb_intern("own_legacy_orders"));
	sym_p_real = ID2SYM(rb_intern("p_real"));
	sym_price = ID2SYM(rb_intern("price"));
	sym_type = ID2SYM(rb_intern("type"));
	sym_market = ID2SYM(rb_intern("market"));
	sym_child_type = ID2SYM(rb_intern("child_type"));
	sym_child_price_threshold = ID2SYM(rb_intern("child_price_threshold"));
	sym_cata = ID2SYM(rb_intern("cata"));
	sym_aggressive = ID2SYM(rb_intern("aggressive"));
	sym_suggest_size = ID2SYM(rb_intern("suggest_size"));
	sym_explain = ID2SYM(rb_intern("explain"));
	sym_main_orders = ID2SYM(rb_intern("main_orders"));
	sym_ideal_profit = ID2SYM(rb_intern("ideal_profit"));
	sym_yes = ID2SYM(rb_intern("yes"));
	sym_no = ID2SYM(rb_intern("no"));
	sym_active = ID2SYM(rb_intern("active"));
	sym_retain = ID2SYM(rb_intern("retain"));
	sym_size = ID2SYM(rb_intern("size"));

	id_my_market_data_agt = rb_intern("@market_data_agt");
	id_my_markets = rb_intern("@markets");
	id_my_keep_exist_spike_order = rb_intern("@_keep_exist_spike_order");
	id_my_market_snapshot = rb_intern("@market_snapshot");
	id_my_arbitrage_min = rb_intern("@arbitrage_min");
	id_my_pair = rb_intern("@pair");
	id_my_single_vol_ratio = rb_intern("@single_vol_ratio");
	id_my_vol_max = rb_intern("@vol_max");
	id_my_vol_min = rb_intern("@vol_min");
	id_my_vol_omit = rb_intern("@vol_omit");
	id_my_last_order_arbitrage_min = rb_intern("@last_order_arbitrage_min");
	id_my_run_mode = rb_intern("@run_mode");
	id_my_spike_catcher_enabled = rb_intern("@spike_catcher_enabled");
	id_my_avg_last = rb_intern("@avg_last");
	id_my_need_balance_refresh = rb_intern("@need_balance_refresh");
	id_my_main_order_min_num_map = rb_intern("@main_order_min_num_map");
	id_my_low_balance_market = rb_intern("@low_balance_market");
	id_my_deviation_map = rb_intern("@deviation_map");
	id_my_debug = rb_intern("@debug");

	ab3_inited = true;
	return;
}

#define rbary_sz(v_ary) \
	NUM2INT(rb_funcall((v_ary), id_size, 0))
#define rbary_el(v_ary, idx) \
	rb_ary_entry((v_ary), (long)(idx))
#define rbary_each(v_ary, idx, max, v_el) \
	for(idx=0, v_el=rbary_el(v_ary,idx), max=rbary_sz(v_ary); \
			idx<max;\
			idx++, v_el=rbary_el(v_ary,idx))
#define rbhsh_el(v_ary, chars) \
	rb_hash_aref((v_ary), rb_str_new_cstr(chars))
#define rbnum_to_dbl(v) \
	((double)((TYPE(v) == T_FIXNUM) ? NUM2LONG(v) : NUM2DBL(v)))
#define rbnum_to_long(v) \
	((long)((TYPE(v) == T_FIXNUM) ? NUM2LONG(v) : NUM2DBL(v)))

#define BUY 1
#define SEL 0
#define MAX_AB3_MKTS 9
#define MAX_AGRSV_TYPES 4 // yes no active retain
#define MAX_SPIKES 7
#define TRIGGER_OPTS 8

struct order {
	char m[16];
	char status[16];
	char desc[128];
	char i[64];
	double p;
	double s;
	double executed;
	double remained;
	bool vol_based;
	long v;
	long executed_v;
	long remained_v;
	bool buy;
	long ts_e3;
	struct order *next;
	bool valid;
};

static int index_of(int max, char (*strings)[MAX_AB3_MKTS][64], char *s) {
	if (s == NULL) return -1;
	for (int i=0; i<max; i++) {
		if ((*strings)[i] == NULL)
			continue;
		if (strcmp((*strings)[i], s) == 0)
			return i;
	}
	return -1;
}
static bool is_order_alive(struct order *o) {
	return true;
}

static double diff(double d1, double d2) {
	// return 9999999 if [f1, f2].min <= 0
	// (f1 - f2) / [f1, f2].min
	double min = URN_MIN(d1, d2);
	if (min <= 0) return 9999999;
	return (d1-d2)/min;
}

struct two_i {
	int a;
	int b;
};
int two_i_comparitor(const void *two_x, const void *two_y) {
	if (((struct two_i *)two_x)->a > ((struct two_i *)two_y)->a) return 1;
	if (((struct two_i *)two_x)->a < ((struct two_i *)two_y)->a) return -1;
	return 0;
}

/*
 * Local cache for trader
 */
long cache_trader_obj_id = -1;
VALUE my_market_data_agt;
VALUE my_markets;
VALUE my_keep_exist_spike_order;
VALUE my_market_snapshot;
double c_vol_max_const;
VALUE my_pair;
char c_my_pair[32];
bool c_my_spike_catcher_enabled;
int my_markets_sz;
double c_my_last_t_suggest_new_spike_prices[MAX_AB3_MKTS] = {0};
char c_mkts[MAX_AB3_MKTS][64] = {'\0'};
VALUE cv_mkts[MAX_AB3_MKTS] = {Qnil};
VALUE cv_mkt_clients[MAX_AB3_MKTS] = {Qnil};
bool c_run_mode_a=false, c_run_mode_b=false, c_run_mode_c=false;
bool c_run_mode_d=false, c_run_mode_e=false;
double c_arbitrage_min;
double c_last_order_arbitrage_min;
bool c_valid_mkts[MAX_AB3_MKTS] = {false};
double c_price_ratio_ranges[MAX_AB3_MKTS][2] = {-1};
double c_p_steps[MAX_AB3_MKTS] = {-1};
VALUE const_urn_mkt_bull = Qnil;
VALUE const_urn_mkt_bear = Qnil;
int c_trader_tasks_by_mkt[MAX_AB3_MKTS] = {-1};
double c_vol_zero;
int c_price_precise;
int c_size_precise;
bool c_is_fut_mkts[MAX_AB3_MKTS] = {false};
double c_typebc_cap_rate;
double c_single_vol_ratio;
VALUE c_aggr_syms[MAX_AGRSV_TYPES] = {Qnil};
bool c_my_debug = false;
double devi_map_mk_buy[MAX_AB3_MKTS] = {1};
double devi_map_tk_buy[MAX_AB3_MKTS] = {1};
double devi_map_mk_sel[MAX_AB3_MKTS] = {1};
double devi_map_tk_sel[MAX_AB3_MKTS] = {1};
double c_min_vols[MAX_AB3_MKTS] = {-1};
double c_min_qtys[MAX_AB3_MKTS] = {-1};
double c_qty_steps[MAX_AB3_MKTS] = {-1};
VALUE str_sell, str_buy, str_T, str_p, str_s, str_pair, str_market;
VALUE str_p_take, str_i, str_status, str_executed, str_remained;
VALUE str_v, str_executed_v, str_remained_v, str_t;
char c_spike_catas[2][MAX_SPIKES][8] = {'\0'};

/*
 * Call this after AB3 @deviation_map changed.
 */
static void cache_trader_reset(VALUE self) {
	cache_trader_obj_id = -1;
}
static void cache_trader_attrs(VALUE self) {
	long trader_obj_id = NUM2LONG(rb_funcall(self, id_object_id, 0));
	if (cache_trader_obj_id < 0)
		cache_trader_obj_id = trader_obj_id;
	else if (cache_trader_obj_id == trader_obj_id)
		return;
	URN_LOGF("cache_trader_attrs() reset for trader_obj_id %ld", cache_trader_obj_id);
	c_aggr_syms[0] = sym_yes;
	c_aggr_syms[1] = sym_no;
	c_aggr_syms[2] = sym_active;
	c_aggr_syms[3] = sym_retain;
	long idx, max; // for rbary_each()
	VALUE v_el, v_tmp; // for tmp usage.

	my_market_data_agt = rb_ivar_get(self, id_my_market_data_agt);
	my_markets = rb_ivar_get(self, id_my_markets);
	my_keep_exist_spike_order = rb_ivar_get(self, id_my_keep_exist_spike_order);
	my_market_snapshot = rb_ivar_get(self, id_my_market_snapshot);
	c_vol_max_const = NUM2DBL(rb_ivar_get(self, rb_intern("@vol_max_const")));
	my_pair = rb_ivar_get(self, id_my_pair);
	strcpy(c_my_pair, RSTRING_PTR(my_pair));
	VALUE my_run_mode = rb_ivar_get(self, id_my_run_mode);
	rbary_each(my_run_mode, idx, max, v_el) {
		char * mode = RSTRING_PTR(v_el);
		if (strcmp(mode, "A") == 0) c_run_mode_a = true;
		if (strcmp(mode, "B") == 0) c_run_mode_b = true;
		if (strcmp(mode, "C") == 0) c_run_mode_c = true;
	}
	c_run_mode_d=c_run_mode_b, c_run_mode_e=c_run_mode_c;
	c_my_spike_catcher_enabled = (TYPE(rb_ivar_get(self, id_my_spike_catcher_enabled)) == T_TRUE) ? true : false;

	my_markets_sz = rbary_sz(my_markets);
	if (my_markets_sz > MAX_AB3_MKTS)
		rb_raise(rb_eTypeError, "MAX_AB3_MKTS too small, rise it now");
	for (int i=0; i < my_markets_sz; i++) {
		cv_mkts[i] = rbary_el(my_markets, i);
		cv_mkt_clients[i] = rb_funcall(self, id_market_client, 1, cv_mkts[i]);
		strcpy(c_mkts[i], RSTRING_PTR(cv_mkts[i]));
	}
	for (int i=my_markets_sz; i < MAX_AB3_MKTS; i++) {
		cv_mkts[i] = Qnil;
		cv_mkt_clients[i] = Qnil;
		c_mkts[i][0] = '\0';
	}

	c_arbitrage_min = NUM2DBL(rb_ivar_get(self, id_my_arbitrage_min));
	c_last_order_arbitrage_min = NUM2DBL(rb_ivar_get(self, id_my_last_order_arbitrage_min));

	VALUE my_trader_tasks_by_mkt = rb_ivar_get(self, rb_intern("@trader_tasks_by_mkt"));
	VALUE my_last_t_suggest_new_spike_price = rb_ivar_get(self, rb_intern("@last_t_suggest_new_spike_price"));
	for (int i=0; i < my_markets_sz; i++) {
		c_min_vols[i] = NUM2DBL(rb_funcall(cv_mkt_clients[i], rb_intern("min_vol"), 1, my_pair));
		c_min_qtys[i] = NUM2DBL(rb_funcall(cv_mkt_clients[i], rb_intern("min_quantity"), 1, my_pair));
		c_qty_steps[i] = NUM2DBL(rb_funcall(cv_mkt_clients[i], rb_intern("quantity_step"), 1, my_pair));

		VALUE v_price_ratio_range = rb_funcall(cv_mkt_clients[i], id_price_ratio_range, 1, my_pair);
		c_price_ratio_ranges[i][0] = NUM2DBL(rbary_el(v_price_ratio_range, 0));
		c_price_ratio_ranges[i][1] = NUM2DBL(rbary_el(v_price_ratio_range, 1));
		if (c_price_ratio_ranges[i][0] >= c_price_ratio_ranges[i][1]) {
			URN_WARNF("c_price_ratio_ranges %8s %lf %lf", c_mkts[i],
				c_price_ratio_ranges[i][0], c_price_ratio_ranges[i][1]);
			rb_raise(rb_eTypeError, "price_ratio_range should be [min, max]");
		}
		c_p_steps[i] = NUM2DBL(rb_funcall(cv_mkt_clients[i],
			id_price_step, 1, my_pair));

		VALUE _trader_tasks_by_m = rb_hash_aref(my_trader_tasks_by_mkt, cv_mkts[i]);
		if (TYPE(_trader_tasks_by_m) == T_NIL)
			c_trader_tasks_by_mkt[i] = 0;
		else
			c_trader_tasks_by_mkt[i] = rbary_sz(_trader_tasks_by_m);

		VALUE _last_t_suggest_new_spike_p = rb_hash_aref(my_last_t_suggest_new_spike_price, cv_mkts[i]);
		if (TYPE(_last_t_suggest_new_spike_p) == T_NIL)
			c_my_last_t_suggest_new_spike_prices[i] = 0;
		else
			c_my_last_t_suggest_new_spike_prices[i] = NUM2DBL(_last_t_suggest_new_spike_p);
		
		VALUE v_m_type = rb_funcall(cv_mkt_clients[i], rb_intern("market_type"), 0);
		if (rb_to_id(v_m_type) == rb_intern("future"))
			c_is_fut_mkts[i] = true;
		else
			c_is_fut_mkts[i] = false;
	}

	c_vol_zero = NUM2DBL(rb_ivar_get(self, rb_intern("@vol_zero")));
	c_price_precise = NUM2INT(rb_ivar_get(self, rb_intern("@price_precise")));
	c_size_precise = NUM2INT(rb_ivar_get(self, rb_intern("@size_precise")));
	c_typebc_cap_rate = NUM2DBL(rb_ivar_get(self, rb_intern("@typeBC_capacity_rate")));
	c_single_vol_ratio = NUM2DBL(rb_ivar_get(self, rb_intern("@single_vol_ratio")));
	c_my_debug = (TYPE(rb_ivar_get(self, id_my_debug)) == T_TRUE) ? true : false;

	VALUE const_urn = rb_const_get(rb_cObject, rb_intern("URN"));
	const_urn_mkt_bull = rb_const_get(const_urn, rb_intern("MKT_BULL"));
	const_urn_mkt_bear = rb_const_get(const_urn, rb_intern("MKT_BEAR"));

	str_sell = rb_str_new2("sell");
	str_buy = rb_str_new2("buy");
	str_T = rb_str_new2("T");
	str_p = rb_str_new2("p");
	str_s = rb_str_new2("s");
	str_pair = rb_str_new2("pair");
	str_market = rb_str_new2("market");
	str_p_take = rb_str_new2("p_take");
	str_i = rb_str_new2("i");
	str_status = rb_str_new2("status");
	str_executed = rb_str_new2("executed");
	str_remained = rb_str_new2("remained");
	str_v = rb_str_new2("v");
	str_remained_v = rb_str_new2("remained_v");
	str_executed_v = rb_str_new2("executed_v");
	str_t = rb_str_new2("t");

	// char c_spike_catas[2][MAX_SPIKES][8] : [BUY/SELL][X] = "SBX" or "SCX"
	for (int t = 0; t < 2; t++) {
		for (int s = 0; s < MAX_SPIKES; s++) {
			sprintf(c_spike_catas[t][s], "S%c%d",
				(t==BUY ? 'B' : 'C'), s+1);
		}
	}
	URN_LOGF("cache_trader_attrs() done for trader_obj_id %ld", cache_trader_obj_id);
}

static void rborder_to_order(VALUE v_o, struct order *o) {
	strcpy(o->i, RSTRING_PTR(rb_hash_aref(v_o, str_i)));
	strcpy(o->m, RSTRING_PTR(rb_hash_aref(v_o, str_market)));
	strcpy(o->status, RSTRING_PTR(rb_hash_aref(v_o, str_status)));
	o->p = rbnum_to_dbl(rb_hash_aref(v_o, str_p));
	o->s = rbnum_to_dbl(rb_hash_aref(v_o, str_s));
	o->executed = rbnum_to_dbl(rb_hash_aref(v_o, str_executed));
	o->remained = rbnum_to_dbl(rb_hash_aref(v_o, str_remained));
	if (TYPE(rb_hash_aref(v_o, str_v)) == T_NIL) {
		o->vol_based = true;
		o->v = rbnum_to_long(rb_hash_aref(v_o, str_v));
		o->executed_v = rbnum_to_long(rb_hash_aref(v_o, str_executed_v));
		o->remained_v = rbnum_to_long(rb_hash_aref(v_o, str_remained_v));
	} else
		o->vol_based = false;
	o->ts_e3 = rbnum_to_long(rb_hash_aref(v_o, str_t));
	char *type = RSTRING_PTR(rb_hash_aref(v_o, str_T));
	if (strcmp("buy", type) == 0)
		o->buy = true;
	else
		o->buy = false;
	o->next = NULL;
	o->valid = true;
}
static char *rborder_desc(struct order *o) {
	long sec = o->ts_e3 / 1000;
	long msec = o->ts_e3 - sec*1000;
	sprintf(o->desc, "%s%7s %4s %3.10lf %3.5lf/%3.5f"
		" %9s %6s %02lu:%02lu:%02lu.%03lu" CLR_RST,
		(o->buy ? CLR_GREEN : CLR_RED),
		o->m, (o->buy ? "BUY" : "SELL"), o->p, o->executed, o->s,
		o->status, (o->i)+strlen(o->i)-6,
		((sec+URN_TIMEZONE) % 86400)/3600,
		(sec % 3600)/60,
		(sec % 60), msec);
	return o->desc;
}

/*
 * for method_detect_arbitrage_pattern() usage only
 * Replacement of:
 * p = market_client(m1).format_price_str(@pair, 'buy', p, adjust:true, verbose:false).to_f
 */
static double _format_price(double p, double step, int type) {
	long toint = 10000000000; // 10^11 is precise enough.
	long step_i = lround(step * toint);
	long price_i = lround(p * toint);
	long new_price_i = price_i / step_i * step_i;
	if (new_price_i == price_i)
		return p;
	if (type == SEL)
		new_price_i += step_i;
	double new_p = ((double)(new_price_i)) / toint;
	return urn_round(new_p, 10);
}
/* A min s that:
 * 	1: s*p >= min_vol
 * 	2: s = TIMES of size step
 * 	3: s >= min_qty
 */
static double _min_order_size(int m_idx, double p, int type) {
	double adj_p = _format_price(p, c_p_steps[m_idx], type);
	if (adj_p <= 0) return c_min_qtys[m_idx];
	double s = c_min_vols[m_idx] / adj_p;
	// Use _format_price() to set adjusted size
	double adj_s = _format_price(s, c_qty_steps[m_idx], SEL);
	return URN_MAX(adj_s, c_min_qtys[m_idx]);
}

struct timeval _ab3_dbg_start_t;
#define _ab3_dbg(...) \
	if (c_my_debug) { \
		URN_LOGF( __VA_ARGS__ ); \
		URN_LOGF("\t\t +%6ldms +%9ldus", \
			urn_msdiff(_ab3_dbg_start_t, urn_global_tv), \
			urn_usdiff(_ab3_dbg_start_t, urn_global_tv)); \
	}
#define _ab3_dbgc(...) \
	if (c_my_debug) { \
		URN_LOGF_C( __VA_ARGS__ ); \
		URN_LOGF("\t\t +%6ldms +%9ldus", \
			urn_msdiff(_ab3_dbg_start_t, urn_global_tv), \
			urn_usdiff(_ab3_dbg_start_t, urn_global_tv)); \
	}
#define _ab3_dbg_end \
	if (c_my_debug) { \
		gettimeofday(&urn_global_tv, NULL); \
		_ab3_dbg("detect_arbitrage_pattern_c end, cost usec %ld", \
			urn_usdiff(_ab3_dbg_start_t, urn_global_tv)); \
	}

#define _compute_shown_price(p_real, rate, type) \
	(((type) == BUY) ? ((p_real)*(1-(rate))) : ((p_real)/(1-(rate))))
#define _compute_real_price(p, rate, type) \
	(((type) == BUY) ? ((p)/(1-(rate))) : ((p)*(1-(rate))))
//static double _compute_real_price(double p, double rate, int type) {
//	if (type == BUY) {
//		return (p/(1-rate));
//	} else if (type == SEL) {
//		return (p*(1-rate));
//	} else
//		rb_raise(rb_eTypeError, "invalid type for _compute_real_price");
//}

/* for method_detect_arbitrage_pattern() usage only,
 * same as:
 * common/mkt.rb
 * 	def aggressive_arbitrage_orders(odbk_bid, odbk_ask, min_price_diff, opt={})
 * 		with opt[:use_real_price] true
 * But args format is not same:
 * 	odbk_bid/ask : [[p, s, p_take], [], ...]
 * 	len = max_odbk_depth
 * 	p <= 0 means invalid
 * Save orders in buy_order and sell_order, returns if order find.
 */
static bool _aggressive_arbitrage_orders_c(
	double (*bids_map)[max_odbk_depth][3],
	double (*asks_map)[max_odbk_depth][3],
	double min_price_diff,
	int    mkt_client_bid_idx,
	int    mkt_client_ask_idx,
	double vol_max, double vol_min,
	struct order *buy_order, struct order *sell_order
) {
	// Scan bids and asks one by one, scan bids first.
	// [SEL/BUY][finished(0/1), idx, size_sum, price, p_real, avg_price]
	if ((*bids_map)[0][0] == 0 || (*asks_map)[0][0] == 0)
		return false;
	// Do the fast check, compare top p_take, it saves time!
	double top_bid_p = (*bids_map)[0][0];
	double top_ask_p = (*asks_map)[0][0];
	double top_bid_p_take = (*bids_map)[0][2];
	double top_ask_p_take = (*asks_map)[0][2];
	double fast_price_diff = diff(top_bid_p_take, top_ask_p_take);
	if (fast_price_diff <= min_price_diff) {
		_ab3_dbg("\t bid0:%4.8lf,%4.8lf ask0:%4.8lf,%4.8lf "
			"fast price diff:%1.5lf < %1.5lf stop",
			top_bid_p, top_bid_p_take,
			top_ask_p, top_ask_p_take,
			fast_price_diff, min_price_diff);
		return false;
	}

	VALUE mkt_client_bid = cv_mkt_clients[mkt_client_bid_idx];
	VALUE mkt_client_ask = cv_mkt_clients[mkt_client_ask_idx];
	double scan_status[2][7] = {0};
	int _idx_fin=0, _idx_idx=1, _idx_size_sum=2, _idx_p=3, _idx_p_real=4, _idx_avg_p=5, _idx_p_take=6;

	double avg_price_max_diff = 0.001;
	bool scan_buy_now = true;
	bool balance_limited=false, vol_max_reached=false, vol_min_limited=false;
	int  loop_ct = 0;

	double max_ask_sz = -1; // will be calculated once
	while (1) {
		loop_ct ++;
		if (loop_ct > max_odbk_depth*3) {
			URN_WARNF("Bug 1 triggerred, should save arguments for debug later");
			return false;
		}
		// break if scan_status['buy']['finished'] && scan_status['sell']['finished']
		if ((scan_status[BUY][_idx_fin] == 1) && (scan_status[SEL][_idx_fin] == 1))
			break;
		int type = scan_buy_now ? BUY : SEL;
		int opposite_type = scan_buy_now ? SEL : BUY;
		double (*odbk)[max_odbk_depth][3] = scan_buy_now ? bids_map : asks_map;
		int idx = (int)(scan_status[type][_idx_idx]);
		if ((scan_status[opposite_type][_idx_fin] == 1) &&
				(scan_status[type][_idx_size_sum] > scan_status[opposite_type][_idx_size_sum])) {
			_ab3_dbg("%d side is already finished, "
				"%d side size_sum is larger than opposite, all finished.",
				opposite_type, type);
			scan_status[type][_idx_fin] = 1;
			break;
		}
		double p=0, s=0, p_take=0;
		if (idx < max_odbk_depth)
			p=(*odbk)[idx][0], s=(*odbk)[idx][1], p_take=(*odbk)[idx][2];

		if ((scan_status[opposite_type][_idx_fin] == 1) &&
				((idx >= max_odbk_depth) || (p <= 0))) {
			_ab3_dbg("%d side is already finished, "
				"all %d data is scanned, all finished.",
				opposite_type, type);
			scan_status[type][_idx_fin] = 1;
			break;
		}

		// Scan other side if this side is marked as finished.
		// Scan other side if this orderbook is all checked.
		if ((scan_status[type][_idx_fin] == 1) ||
				((idx >= max_odbk_depth) || (p <= 0))) {
			_ab3_dbg("-> flip to %d", opposite_type);
			scan_status[type][_idx_fin] = 1;
			scan_buy_now = !scan_buy_now;
			continue;
		}

		// _ab3_dbg("\tODBK %d %d %lf %lf %lf", type, idx, p, s, p_take);
		if ((p <= 0) || (s <= 0) || (p_take <= 0)) {
			rb_raise(rb_eTypeError, "invalid odbk data, p/s/p_take <= 0");
			break;
		}
		double size_sum = scan_status[type][_idx_size_sum];
		double avg_price = scan_status[type][_idx_avg_p];
		bool   vol_max_reached = false;
		// Scan always stop at this side when max vol is reached.
		if (s + size_sum > vol_max) {
			s = vol_max - size_sum;
			vol_max_reached = true;
			scan_status[type][_idx_fin] = 1;
			_ab3_dbg("Max vol reached, vol_max %lf", vol_max);
		}
		// Cap the size according to maximum order size, again:
		// mkt_client_bid is the market client of bid orderbook, where ask order should be placed.
		// mkt_client_ask is the market client of ask orderbook, where bid order should be placed.
		double max_size = 0;
		if (type == BUY) {
			// max_size = mkt_client_ask.max_order_size(pair, type, p)
			max_size = NUM2DBL(rb_funcall(
				mkt_client_ask, id_max_order_size, 3,
				my_pair, str_buy, DBL2NUM(p))
			);
		} else if (type == SEL) {
			// max_size = mkt_client_bid.max_order_size(pair, type, p)
			if (max_ask_sz < 0) {
				max_ask_sz = NUM2DBL(rb_funcall(
					mkt_client_bid, id_max_order_size, 3,
					my_pair, str_sell, DBL2NUM(p))
				);
			}
			max_size = max_ask_sz;
		}
		if (s + size_sum > max_size) {
			s = max_size - size_sum;
			scan_status[type][_idx_fin] = 1;
			balance_limited = true;
			_ab3_dbg("Balance limited, %d max=%lf p=%lf", type, max_size, p);
		}

		// Compare this price with opposite price.
		double opposite_p = scan_status[opposite_type][_idx_p];
		double opposite_p_take = scan_status[opposite_type][_idx_p_real];
		double opposite_s = scan_status[opposite_type][_idx_size_sum];
		// Initializing from this side.
		if (opposite_s <= 0) {
			scan_status[type][_idx_idx] += 1;
			scan_status[type][_idx_size_sum] += s;
			scan_status[type][_idx_p] = p;
			scan_status[type][_idx_p_take] = p_take;
			scan_status[type][_idx_avg_p] = p;
			_ab3_dbg("Scan %d orderbook %d: O p %lf p_tk %lf s %lf init this side.",
				type, idx, p, p_take, s);
			scan_buy_now = !scan_buy_now;
			continue;
		}
		_ab3_dbg("Scan %d orderbook %d: O p %lf p_tk %lf s %lf", type, idx, p, p_take, s);

		// Compare this price with opposite_p
		double price_diff = scan_buy_now ? diff(p_take, opposite_p_take) : diff(opposite_p_take, p_take);
		if (price_diff <= min_price_diff) {
			_ab3_dbg("\ttype:%d p:%lf,%lf opposite_p:%lf,%lf "
				"price diff:%lf < %lf side finished",
				type, p, p_take, opposite_p, opposite_p_take,
				price_diff, min_price_diff);
			scan_status[type][_idx_fin] = 1;
			continue;
		}

		// Compare future average price, should not deviate from price too much.
		if (size_sum + s == 0) continue;
		double next_avg_price = (avg_price*size_sum + p*s) / (size_sum + s);
		_ab3_dbg("\tnext_avg: %8.8lf diff(next_avg, p): %8.8f",
			next_avg_price, URN_ABS(diff(next_avg_price, p)));
		if (URN_ABS(diff(next_avg_price, p)) > avg_price_max_diff) {
			_ab3_dbg("\tdiff(next_avg, p).abs too large, this side finished");
			scan_status[type][_idx_fin] = 1;
			continue;
		}
		size_sum += s;
		// Record scan progress.
		double price = p;
		avg_price = next_avg_price;
		scan_status[type][_idx_size_sum] = size_sum;
		scan_status[type][_idx_p] = price;
		scan_status[type][_idx_p_take] = p_take;
		scan_status[type][_idx_avg_p] = next_avg_price;
		_ab3_dbg("\tnow %d size_sum:%lf p:%lf avg_p:%lf", type, size_sum, p, next_avg_price);
		scan_status[type][_idx_idx] += 1;
		// Compare future size_sum with opposite_s, if this side is fewer, continue scan.
		if (size_sum < opposite_s) continue;
		scan_buy_now = !scan_buy_now;
	}

	// Return if no suitable orders.
	if ((scan_status[BUY][_idx_idx] == 0) || (scan_status[SEL][_idx_idx] == 0))
		return false;

	// Scanning finished, generate a pair of order with same size.
	sell_order->p = scan_status[BUY][_idx_p]; // Sell at BIDS
	buy_order->p  = scan_status[SEL][_idx_p]; // Buy  at ASKS
	double max_bid_sz = NUM2DBL(rb_funcall(
		mkt_client_ask, id_max_order_size, 3,
		my_pair, str_buy, DBL2NUM(buy_order->p))
	);
	buy_order->s  = URN_MIN(scan_status[SEL][_idx_size_sum], max_bid_sz); // Buy  at ASKS
	sell_order->s = URN_MIN(scan_status[BUY][_idx_size_sum], max_ask_sz); // Sell at BIDS
	double size = URN_MIN(buy_order->s, sell_order->s);
	size = urn_round(size, c_size_precise);
	buy_order->s  = size;
	sell_order->s = size;

	if (size < vol_min)
		return false;
	double min_bid_sz = _min_order_size(mkt_client_ask_idx,  buy_order->p, BUY); // Buy  at ASKS
	double min_ask_sz = _min_order_size(mkt_client_bid_idx, sell_order->p, SEL); // Sell at BIDS
	if ((size < min_bid_sz) || (size < min_ask_sz))
		return false;

	return true;
}

static VALUE rborder_new(int m_idx, double p, double s, int type) {
	VALUE v_os = rb_hash_new();
	rb_hash_aset(v_os, str_pair, my_pair);
	rb_hash_aset(v_os, str_market, cv_mkts[m_idx]);
	rb_hash_aset(v_os, str_p, DBL2NUM(p));
	rb_hash_aset(v_os, str_s, DBL2NUM(s));
	if (type == BUY)
		rb_hash_aset(v_os, str_T, str_buy);
	else if (type == SEL)
		rb_hash_aset(v_os, str_T, str_sell);
	return v_os;
}

VALUE _new_rb_ary(int num, ...) {
	VALUE v_ary = rb_ary_new2(num);
	va_list list;
	va_start(list, num);
	for(int i=1; i<=num; i++) {
		VALUE v = va_arg(list, VALUE);
		rb_ary_push(v_ary, v);
	}
	va_end(list);
	return v_ary;
}

VALUE _new_ab3_chance(
	VALUE self, VALUE main_orders,
	int m_idx, bool as_taker,
	double child_price_threshold,
	double ideal_profit, char *cata
) {
	VALUE c = rb_hash_new();
	VALUE mo = rbary_el(main_orders, 0);
	VALUE mo2 = rbary_el(main_orders, 1); // might be Qnil
	char *mo_type = RSTRING_PTR(rb_hash_aref(mo, str_T));
	bool mo_buy = (strcmp("buy", mo_type) == 0) ? true : false;

	double p = NUM2DBL(rb_hash_aref(mo, str_p));
	double rate = -1;
	if (as_taker && mo_buy)  rate = devi_map_tk_buy[m_idx];
	if (as_taker && !mo_buy) rate = devi_map_tk_sel[m_idx];
	if ((!as_taker) && mo_buy)  rate = devi_map_mk_buy[m_idx];
	if ((!as_taker) && !mo_buy) rate = devi_map_mk_sel[m_idx];
	double p_real = _compute_real_price(p, rate, (mo_buy ? BUY : SEL));

	VALUE v_m = rb_hash_aref(mo, str_market);
	if (v_m != cv_mkts[m_idx])
		rb_raise(rb_eTypeError, "unconinstent market with m_idx");

	rb_hash_aset(c, sym_main_orders, main_orders);
	rb_hash_aset(c, sym_p_real, DBL2NUM(p_real));
	rb_hash_aset(c, sym_price, rb_hash_aref(mo, str_p));
	rb_hash_aset(c, sym_type, rb_hash_aref(mo, str_T));
	rb_hash_aset(c, sym_market, v_m);
	rb_hash_aset(c, sym_child_type, mo_buy ? rb_str_new2("sell") : rb_str_new2("buy"));
	if (child_price_threshold <= 0) {
		child_price_threshold = mo_buy ?
		       (p_real*(1+c_last_order_arbitrage_min)) :
		       (p_real/(1+c_last_order_arbitrage_min));
	}
	rb_hash_aset(c, sym_child_price_threshold, DBL2NUM(child_price_threshold));
	rb_hash_aset(c, sym_ideal_profit, DBL2NUM(ideal_profit));
	rb_hash_aset(c, sym_cata, rb_str_new2(cata));
	rb_hash_aset(c, sym_suggest_size, rb_hash_aref(mo, str_s));
	char explain[256];
	if (strcmp("A", cata) == 0) {
		sprintf(explain, "Cata  %4s: %s %8.8lf at %8.8lf %8s, %s at %s",
			cata,
			RSTRING_PTR(rb_hash_aref(mo, str_T)),
			NUM2DBL(rb_hash_aref(mo, str_s)),
			NUM2DBL(rb_hash_aref(mo, str_p)),
			RSTRING_PTR(v_m),
			RSTRING_PTR(rb_hash_aref(mo2, str_T)),
			RSTRING_PTR(rb_hash_aref(mo2, str_market)));
	} else if ((strcmp("B", cata) == 0) || (strcmp("C", cata) == 0)) {
		sprintf(explain, "Cata  %4s: %s %8.8lf at %8.8lf %8s and wait",
			cata,
			RSTRING_PTR(rb_hash_aref(mo, str_T)),
			NUM2DBL(rb_hash_aref(mo, str_s)),
			NUM2DBL(rb_hash_aref(mo, str_p)),
			RSTRING_PTR(v_m));
	} else if (strncmp("S", cata, 1) == 0) { // SBx or SCx
		sprintf(explain, "Spike %4s: %s %8.8lf at %8.8lf %8s and wait",
			cata,
			RSTRING_PTR(rb_hash_aref(mo, str_T)),
			NUM2DBL(rb_hash_aref(mo, str_s)),
			NUM2DBL(rb_hash_aref(mo, str_p)),
			RSTRING_PTR(v_m));
	} else
		rb_raise(rb_eTypeError, "invalid cata in _new_ab3_chance()");
	_ab3_dbg("\tnew %s chance %s", cata, explain);
	rb_hash_aset(c, sym_explain, rb_str_new2(explain));
	return c;
}
#define rbstr(v) ((TYPE(v)==T_STRING) ? RSTRING_PTR(v) : ((TYPE(v)==T_NIL) ? "(N)" : "(?)"))
#define rbdbl(v) ((TYPE(v)==T_FLOAT || TYPE(v)==T_FIXNUM || TYPE(v)==T_BIGNUM) ? NUM2DBL(v) : ((TYPE(v)==T_NIL) ? -1 : -1))
static long sprintf_ab3_chance(char *s, VALUE c) {
	char *original_s = s;
	VALUE v = rb_hash_aref(c, sym_market);
	s += sprintf(s, "%8s ", rbstr(v));
	v = rb_hash_aref(c, sym_cata);
	s += sprintf(s, "%3s\n\tINFO: ", rbstr(v));
	v = rb_hash_aref(c, sym_type);
	s += sprintf(s, "%4s ", rbstr(v));
	v = rb_hash_aref(c, sym_size);
	s += sprintf(s, "%4.4f ", rbdbl(v));
	v = rb_hash_aref(c, sym_price);
	s += sprintf(s, "at %4.8f ", rbdbl(v));

	v = rb_hash_aref(c, sym_p_real);
	s += sprintf(s, "p_real %4.8f ", rbdbl(v));
	v = rb_hash_aref(c, sym_child_type);
	s += sprintf(s, ", %4s ", rbstr(v));
	v = rb_hash_aref(c, sym_child_price_threshold);
	s += sprintf(s, "than %4.8f\n\tExpl: ", rbdbl(v));
	v = rb_hash_aref(c, sym_explain);
	s += sprintf(s, "%s", rbstr(v));

	VALUE v_orders = rb_hash_aref(c, sym_main_orders);
	long idx, max; // for rbary_each()
	VALUE v_el;
	rbary_each(v_orders, idx, max, v_el) {
		s += sprintf(s, "\n\t\tO %ld/%ld ", idx+1, max);
		v = rb_hash_aref(v_el, str_market);
		s += sprintf(s, "M %8s ", rbstr(v));
		v = rb_hash_aref(v_el, str_pair);
		s += sprintf(s, "P %12s ", rbstr(v));
		v = rb_hash_aref(v_el, str_T);
		s += sprintf(s, "T %4s ", rbstr(v));
		v = rb_hash_aref(v_el, str_s);
		s += sprintf(s, "s %4.4f ", rbdbl(v));
		v = rb_hash_aref(v_el, str_s);
		s += sprintf(s, "at p %4.8f ", rbdbl(v));
	}
	return s-original_s;
}
struct rbv_w_dbl {
	VALUE v;
	double d;
};
int rbv_w_dbl_comparitor(const void *c1, const void *c2) {
	if (((struct rbv_w_dbl *)c1)->d > ((struct rbv_w_dbl *)c2)->d) return 1;
	if (((struct rbv_w_dbl *)c1)->d < ((struct rbv_w_dbl *)c2)->d) return -1;
	return 0;
}

static void _free_order_chain(struct order *o) {
	while(1) {
		if (o == NULL) return;
		o = o->next;
		free(o);
	}
}

/* Replacement of:
 * trader/ab3.rb
 * 	def detect_arbitrage_pattern(opt={})
 */
VALUE method_detect_arbitrage_pattern(VALUE self, VALUE v_opt) {
	cache_trader_attrs(self);
	/*
	 * STEP 0 prepare instance variables.
	 */
	_ab3_dbg("STEP 0 prepare vars");
	gettimeofday(&_ab3_dbg_start_t, NULL);
	_ab3_dbg("detect_arbitrage_pattern_c");
	double now = (double)(_ab3_dbg_start_t.tv_sec) + (double)(_ab3_dbg_start_t.tv_usec)/1000000.0;

	long idx, max; // for rbary_each()
	VALUE v_el, v_tmp; // for tmp usage.

	c_my_debug = (TYPE(rb_ivar_get(self, id_my_debug)) == T_TRUE) ? true : false;
c_my_debug = true; // force enable debug
	VALUE my_vol_max = rb_ivar_get(self, id_my_vol_max);
	VALUE my_avg_last = rb_ivar_get(self, id_my_avg_last);
	VALUE my_need_balance_refresh = rb_ivar_get(self, id_my_need_balance_refresh);
	VALUE my_main_order_min_num_map = rb_ivar_get(self, id_my_main_order_min_num_map);
	int main_order_min_num_map[MAX_AB3_MKTS] = {0};
	for (int i=0; i < my_markets_sz; i++) {
		v_tmp = rb_hash_aref(my_main_order_min_num_map, cv_mkts[i]);
		if (TYPE(v_tmp) == T_NIL) continue;
		main_order_min_num_map[i] = NUM2INT(v_tmp);
	}

	double single_vol_ratio = NUM2DBL(rb_ivar_get(self, id_my_single_vol_ratio));
	double my_vol_min = NUM2DBL(rb_ivar_get(self, id_my_vol_min));
	double my_vol_omit = NUM2DBL(rb_ivar_get(self, id_my_vol_omit));
	double vol_max_a = NUM2DBL(rbhsh_el(my_vol_max, "A"));
	double vol_max_b = NUM2DBL(rbhsh_el(my_vol_max, "B"));
	double vol_max_c = NUM2DBL(rbhsh_el(my_vol_max, "C"));
	double avg_last = (TYPE(my_avg_last) == T_NIL) ? -1 : NUM2DBL(my_avg_last);

	// Dont know why but must re-init these str again.
	// SYMBOL and instance vars could be kept, but ruby str can't
	// These ruby strings seems to be recycled after few rounds.
	str_sell = rb_str_new2("sell");
	str_buy = rb_str_new2("buy");
	str_T = rb_str_new2("T");
	str_p = rb_str_new2("p");
	str_s = rb_str_new2("s");
	str_pair = rb_str_new2("pair");
	str_market = rb_str_new2("market");
	str_p_take = rb_str_new2("p_take");
	str_i = rb_str_new2("i");
	str_status = rb_str_new2("status");
	str_executed = rb_str_new2("executed");
	str_remained = rb_str_new2("remained");
	str_v = rb_str_new2("v");
	str_remained_v = rb_str_new2("remained_v");
	str_executed_v = rb_str_new2("executed_v");
	str_t = rb_str_new2("t");

	// Cache latest c_valid_mkts
	VALUE v_valid_mkts = rb_funcall(my_market_data_agt, id_valid_markets, 1, my_markets);
	for (int i=0; i < my_markets_sz; i++)
		c_valid_mkts[i] = false;
	rbary_each(v_valid_mkts, idx, max, v_el) {
		char *m = RSTRING_PTR(v_el);
		// _ab3_dbg("P v_valid_mkts %ld %s", idx, m);
		for (int i=0; i < my_markets_sz; i++) {
			if (strcmp(m, c_mkts[i]) == 0) {
				c_valid_mkts[i] = true;
				break;
			}
		}
	}

	int valid_mkts_sz = max;
	_ab3_dbg("P valid markets %d/%d", valid_mkts_sz, my_markets_sz);
	// COST_US 22

	if (valid_mkts_sz < 2) {
		// @markets.each { |m| @_keep_exist_spike_order[m] = true }
		for (int i=0; i < MAX_AB3_MKTS; i++) {
			rb_hash_aset(my_keep_exist_spike_order, cv_mkts[i], Qtrue);
		}
		_ab3_dbg_end;
		return rb_ary_new();
	}

	// Cache latest deviation map
	VALUE my_deviation_map = rb_ivar_get(self, id_my_deviation_map);
	for (int i=0; i < my_markets_sz; i++) {
		// read @deviation_map[m][maker/buy]
		VALUE devi_by_m = rb_hash_aref(my_deviation_map, cv_mkts[i]);
		if (TYPE(devi_by_m) == T_NIL) continue;
		v_tmp = rbhsh_el(devi_by_m, "maker/buy");
		devi_map_mk_buy[i] = (TYPE(v_tmp) == T_NIL) ? 1 : NUM2DBL(v_tmp);
		v_tmp = rbhsh_el(devi_by_m, "taker/buy");
		devi_map_tk_buy[i] = (TYPE(v_tmp) == T_NIL) ? 1 : NUM2DBL(v_tmp);
		v_tmp = rbhsh_el(devi_by_m, "maker/sell");
		devi_map_mk_sel[i] = (TYPE(v_tmp) == T_NIL) ? 1 : NUM2DBL(v_tmp);
		v_tmp = rbhsh_el(devi_by_m, "taker/sell");
		devi_map_tk_sel[i] = (TYPE(v_tmp) == T_NIL) ? 1 : NUM2DBL(v_tmp);
	}

	double reserved_bal[MAX_AB3_MKTS][2] = {0};
	double main_reserved_bal[MAX_AB3_MKTS][2] = {0};
	/*
	 * STEP 1 organize own orders
	 */
	_ab3_dbg("STEP 1 organize orders");
	// linked orders instead of ruby array, must free them at last.
	struct order *own_main_orders[MAX_AB3_MKTS][2] = {NULL};
	struct order *own_spike_main_orders[MAX_AB3_MKTS][2] = {NULL};
	struct order *own_live_orders[MAX_AB3_MKTS][2] = {NULL};
	struct order *own_legacy_orders[MAX_AB3_MKTS][2] = {NULL};

	VALUE v_orders = rb_hash_aref(v_opt, sym_own_main_orders);
	if (TYPE(v_orders) != T_NIL) {
		rbary_each(v_orders, idx, max, v_el) {
			VALUE v_bool = rb_funcall(self, id_order_alive_qm, 1, v_el);
			if (TYPE(v_bool) != T_TRUE)
				continue;
			int m_idx = index_of(my_markets_sz, &c_mkts, RSTRING_PTR(rb_hash_aref(v_el, str_market)));
			if (m_idx < 0)
				continue;
			struct order *o = malloc(sizeof(struct order));
			rborder_to_order(v_el, o);
			// own_main_orders[m][t].push o
			if (o->buy) {
				o->next = own_main_orders[m_idx][BUY];
				own_main_orders[m_idx][BUY] = o;
				// reserved_bal[m][t] += o['remained']
				reserved_bal[m_idx][BUY] += o->remained;
				main_reserved_bal[m_idx][BUY] += o->remained;
			} else {
				o->next = own_main_orders[m_idx][SEL];
				own_main_orders[m_idx][SEL] = o;
				// reserved_bal[m][t] += (o['p']*o['remained'])
				reserved_bal[m_idx][BUY] += ((o->p) * (o->remained));
				main_reserved_bal[m_idx][BUY] += ((o->p) * (o->remained));
			}
		}
	}
	v_orders = rb_hash_aref(v_opt, sym_own_spike_main_orders);
	if (TYPE(v_orders) != T_NIL) {
		rbary_each(v_orders, idx, max, v_el) {
			VALUE v_bool = rb_funcall(self, id_order_alive_qm, 1, v_el);
			if (TYPE(v_bool) != T_TRUE)
				continue;
			int m_idx = index_of(my_markets_sz, &c_mkts, RSTRING_PTR(rb_hash_aref(v_el, str_market)));
			if (m_idx < 0)
				continue;
			struct order *o = malloc(sizeof(struct order));
			rborder_to_order(v_el, o);
			// own_spike_main_orders[m][t].push o
			if (o->buy) {
				o->next = own_spike_main_orders[m_idx][BUY];
				own_spike_main_orders[m_idx][BUY] = o;
				// reserved_bal[m][t] += o['remained']
				reserved_bal[m_idx][BUY] += o->remained;
			} else {
				o->next = own_spike_main_orders[m_idx][SEL];
				own_spike_main_orders[m_idx][SEL] = o;
				// reserved_bal[m][t] += (o['p']*o['remained'])
				reserved_bal[m_idx][BUY] += ((o->p) * (o->remained));
			}
		}
	}
	v_orders = rb_hash_aref(v_opt, sym_own_live_orders);
	if (TYPE(v_orders) != T_NIL) {
		rbary_each(v_orders, idx, max, v_el) {
			VALUE v_bool = rb_funcall(self, id_order_alive_qm, 1, v_el);
			if (TYPE(v_bool) != T_TRUE)
				continue;
			int m_idx = index_of(my_markets_sz, &c_mkts, RSTRING_PTR(rb_hash_aref(v_el, str_market)));
			if (m_idx < 0)
				continue;
			struct order *o = malloc(sizeof(struct order));
			rborder_to_order(v_el, o);
			// own_live_orders[m][t].push o
			if (o->buy) {
				o->next = own_live_orders[m_idx][BUY];
				own_live_orders[m_idx][BUY] = o;
			} else {
				o->next = own_live_orders[m_idx][SEL];
				own_live_orders[m_idx][SEL] = o;
			}
		}
	}
	v_orders = rb_hash_aref(v_opt, sym_own_legacy_orders);
	if (TYPE(v_orders) != T_NIL) {
		rbary_each(v_orders, idx, max, v_el) {
			VALUE v_bool = rb_funcall(self, id_order_alive_qm, 1, v_el);
			if (TYPE(v_bool) != T_TRUE)
				continue;
			int m_idx = index_of(my_markets_sz, &c_mkts, RSTRING_PTR(rb_hash_aref(v_el, str_market)));
			if (m_idx < 0)
				continue;
			struct order *o = malloc(sizeof(struct order));
			rborder_to_order(v_el, o);
			// own_legacy_orders[m][t].push o
			if (o->buy) {
				o->next = own_legacy_orders[m_idx][BUY];
				own_legacy_orders[m_idx][BUY] = o;
			} else {
				o->next = own_legacy_orders[m_idx][SEL];
				own_legacy_orders[m_idx][SEL] = o;
			}
		}
	}
	// COST_US 30 - no orders.

	/*
	 * STEP 2 prepare market data: bids_map and asks_map
	 * RUBY: bids_map = mkts.map { |m| [m, @market_snapshot.dig(m, :orderbook, 0) || []] }.to_h
	 * C:    bids_map[m_idx] => [[p, s, p_take], [3], ... ], p<=0 means invalid data.
	 */
	_ab3_dbg("STEP 2 market data");
	double bids_map[MAX_AB3_MKTS][max_odbk_depth][3] = {0};
	double asks_map[MAX_AB3_MKTS][max_odbk_depth][3] = {0};
	for (int i=0; i < my_markets_sz; i++) {
		if (c_valid_mkts[i] != true) continue;
		_ab3_dbg("D mkts %d/%d %8s", i, my_markets_sz, c_mkts[i]);
		v_tmp = rb_funcall(my_market_snapshot, id_dig, 3, cv_mkts[i], sym_orderbook, INT2NUM(0));
		if (TYPE(v_tmp) != T_NIL) { // copy bids_map
			rbary_each(v_tmp, idx, max, v_el) {
				if (idx >= max_odbk_depth) break;
				bids_map[i][idx][0] = rbdbl(rb_hash_aref(v_el, str_p));
				bids_map[i][idx][1] = rbdbl(rb_hash_aref(v_el, str_s));
				bids_map[i][idx][2] = rbdbl(rb_hash_aref(v_el, str_p_take));
			}
		}
		v_tmp = rb_funcall(my_market_snapshot, id_dig, 3, cv_mkts[i], sym_orderbook, INT2NUM(1));
		if (TYPE(v_tmp) != T_NIL) { // copy asks_map
			rbary_each(v_tmp, idx, max, v_el) {
				if (idx >= max_odbk_depth) break;
				asks_map[i][idx][0] = rbdbl(rb_hash_aref(v_el, str_p));
				asks_map[i][idx][1] = rbdbl(rb_hash_aref(v_el, str_s));
				asks_map[i][idx][2] = rbdbl(rb_hash_aref(v_el, str_p_take));
			}
		}
		_ab3_dbg("D mkts %d/%d %8s bid0 p %4.6lf p_tk %4.6lf s %8lf ask0 p %4.6lf p_tk %4.6lf s %8lf",
			i, my_markets_sz, c_mkts[i],
			bids_map[i][0][0], bids_map[i][0][2], bids_map[i][0][1],
			asks_map[i][0][0], asks_map[i][0][2], asks_map[i][0][1]);
		// COST_US - each round 10us
	}
	// COST_US 84 - 5 markets

	VALUE v_chances = rb_ary_new();

	/* STEP 3 Detection for case A => "[A] Direct sell and buy" */
	_ab3_dbg("STEP 3 type A detection started");
	for (int m1_idx=0; m1_idx < my_markets_sz; m1_idx++) {
		// check m1 bids and m2 asks, sell at m1 and buy at m2
		if (!c_run_mode_a) break;
		if (c_valid_mkts[m1_idx] != true) continue;
		double top_bid_p = bids_map[m1_idx][0][0];
		double top_bid_p_take = bids_map[m1_idx][0][2];
		if (top_bid_p <= 0) continue;
		for (int m2_idx=0; m2_idx < my_markets_sz; m2_idx++) {
			if (c_valid_mkts[m2_idx] != true) continue;
			if (m1_idx == m2_idx) continue;
			// Compare first, before call _aggressive_arbitrage_orders_c(0
			double top_ask_p = asks_map[m2_idx][0][0];
			double top_ask_p_take = asks_map[m2_idx][0][2];
			if (top_ask_p <= 0) continue;
			double fast_price_diff = (top_bid_p_take-top_ask_p_take)/top_ask_p_take;
			if (fast_price_diff <= c_arbitrage_min)
				continue;

			_ab3_dbg("\t A fast check passed "
				"%s bid0:%4.8lf %4.8lf %s ask0:%4.8lf %4.8lf "
				"fast price diff:%1.5lf >= %1.5lf "
				"_aggressive_arbitrage_orders_c start",
				c_mkts[m1_idx], top_bid_p, top_bid_p_take,
				c_mkts[m2_idx], top_ask_p, top_ask_p_take,
				fast_price_diff, c_arbitrage_min);

			struct order buy_order, sell_order;
			bool found = _aggressive_arbitrage_orders_c(
				&bids_map[m1_idx], &asks_map[m2_idx],
				c_arbitrage_min,
				m1_idx, m2_idx,
				URN_MAX(single_vol_ratio*vol_max_a, my_vol_min*1.1), my_vol_min,
				&buy_order, &sell_order
			);
			// COST_US 100~200 _aggressive_arbitrage_orders_c()
			_ab3_dbg("_aggressive_arbitrage_orders_c end");
			if (!found) continue;
			buy_order.buy = true;
			sell_order.buy = false;
			strcpy(buy_order.m, c_mkts[m2_idx]);
			strcpy(sell_order.m, c_mkts[m1_idx]);
			URN_LOGF("Type A Chances from:\n"
				"mkts %d/%d %8s bid0 p %4.6lf p_tk %4.6lf s %8lf ask0 p %4.6lf p_tk %4.6lf s %8lf\n"
				"mkts %d/%d %8s bid0 p %4.6lf p_tk %4.6lf s %8lf ask0 p %4.6lf p_tk %4.6lf s %8lf\n"
				"BUY  p %8.8f s %8.8f %s \n"
				"SELL p %8.8f s %8.8f %s \n",
				m1_idx, my_markets_sz, c_mkts[m1_idx],
				bids_map[m1_idx][0][0], bids_map[m1_idx][0][2], bids_map[m1_idx][0][1],
				asks_map[m1_idx][0][0], asks_map[m1_idx][0][2], asks_map[m1_idx][0][1],
				m2_idx, my_markets_sz, c_mkts[m2_idx],
				bids_map[m2_idx][0][0], bids_map[m2_idx][0][2], bids_map[m2_idx][0][1],
				asks_map[m2_idx][0][0], asks_map[m2_idx][0][2], asks_map[m2_idx][0][1],
				buy_order.p, buy_order.s, buy_order.m,
				sell_order.p, sell_order.s, sell_order.m);

			// Prepare ruby order hashmap
			VALUE v_os = rborder_new(m1_idx, sell_order.p, sell_order.s, SEL);
			VALUE v_ob = rborder_new(m2_idx, buy_order.p, buy_order.s, BUY);
			VALUE v_orders = _new_rb_ary(2, v_os, v_ob);
			VALUE v_clients = _new_rb_ary(2, cv_mkt_clients[m1_idx], cv_mkt_clients[m2_idx]);

			// equalize_order_size(orders, clients) # might break min_order_size
			rb_funcall(self, id_equalize_order_size, 2, v_orders, v_clients);
			// orders.each { |o| downgrade_order_size_for_all_market(o, all_clients) }
			rb_funcall(self, id_downgrade_order_size_for_all_market, 2, v_os, v_clients);
			rb_funcall(self, id_downgrade_order_size_for_all_market, 2, v_ob, v_clients);
			URN_LOGF("After equalize_order_size() and downgrade_order_size_for_all_market()"
				"BUY  p %8.8f s %8.8f %s \n"
				"SELL p %8.8f s %8.8f %s \n",
				NUM2DBL(rb_hash_aref(v_ob, str_p)), NUM2DBL(rb_hash_aref(v_ob, str_s)), buy_order.m,
				NUM2DBL(rb_hash_aref(v_os, str_p)), NUM2DBL(rb_hash_aref(v_os, str_s)), sell_order.m);

			// equalize_order_size() might break min_order_size
			double min_bid_sz = _min_order_size(m2_idx, buy_order.p, BUY);
			double min_ask_sz = _min_order_size(m1_idx, sell_order.p, SEL);
			if (NUM2DBL(rb_hash_aref(v_os, str_s)) < min_ask_sz) {
				URN_LOGF("\t Sell order break min_order_size %lf", min_ask_sz);
				continue;
			} else if (NUM2DBL(rb_hash_aref(v_ob, str_s)) < min_bid_sz) {
				URN_LOGF("\t Buy  order break min_order_size %lf", min_bid_sz);
				continue;
			}

			// Shit exchanges allow old and new price precision in same orderbook.
			// TODO

			double suggest_size = NUM2DBL(rb_hash_aref(v_ob, str_s));
			double ideal_profit = (sell_order.p - buy_order.p)*suggest_size;
			rb_ary_push(v_chances, _new_ab3_chance(self, v_orders, m1_idx, true, -1, ideal_profit, "A"));
		}
	}

	/* STEP 4 Detection for case waiting Spike buy/sell */

	// If market price higher than lowest_bid_p a lot, dont place spike buy order
	// If market price lower than highest_ask_p a lot, dont place spike sell order
	_ab3_dbg("STEP 4 spike orders"
		"S URN::MKT_BULL %d %d URN::MKT_BEAR %d %d",
		RB_TYPE_P(const_urn_mkt_bull, T_TRUE),
		RB_TYPE_P(const_urn_mkt_bull, T_FALSE),
		RB_TYPE_P(const_urn_mkt_bear, T_TRUE),
		RB_TYPE_P(const_urn_mkt_bear, T_FALSE));
	// COST_US 169us
	double lowest_bid_p=-1, highest_ask_p=-1;
	for (int m1_idx=0; m1_idx < my_markets_sz; m1_idx++) {
		if (c_valid_mkts[m1_idx] != true) continue;
		if (bids_map[m1_idx][0][0] > 0) {
			if (lowest_bid_p < 0)
				lowest_bid_p = bids_map[m1_idx][0][0];
			else
				lowest_bid_p = URN_MIN(lowest_bid_p, bids_map[m1_idx][0][0]);
		}
		if (asks_map[m1_idx][0][0] > 0) {
			if (highest_ask_p < 0)
				highest_ask_p = asks_map[m1_idx][0][0];
			else
				highest_ask_p = URN_MAX(highest_ask_p, asks_map[m1_idx][0][0]);
		}
	}
	_ab3_dbg("S highest_ask_p %lf lowest_bid_p %lf", highest_ask_p, lowest_bid_p);
	if (c_my_spike_catcher_enabled && (c_run_mode_d || c_run_mode_e)) {
		for (int m1_idx=0; m1_idx < my_markets_sz; m1_idx++) {
			if (c_valid_mkts[m1_idx] != true) continue;
			double bid_p = bids_map[m1_idx][0][0];
			double ask_p = asks_map[m1_idx][0][0];
			if ((bid_p <= 0) || (ask_p <= 0)) {
				URN_LOG("S market orderbook invalid, do not cancel spike orders.");
				// Should be temporary data incomplete
				rb_hash_aset(my_keep_exist_spike_order, cv_mkts[m1_idx], Qtrue);
				continue;
			}
			rb_hash_aset(my_keep_exist_spike_order, cv_mkts[m1_idx], Qfalse);
			_ab3_dbg("S spike      %8s BID %4.8lf %4.8lf ASK %4.8lf %4.8lf", c_mkts[m1_idx],
				bid_p, bids_map[m1_idx][0][2],
				ask_p, asks_map[m1_idx][0][2]);
			// Price should not be far away from @avg_last
			// bid_p is higher than avg_last, very abonormal
			// ask_p is lower than avg_last, very abonormal
			if ((avg_last > 0) && (URN_ABS(diff(bid_p, avg_last)) > 0.01)) {
				URN_LOGF_C(RED, "S %8s price B %4.8lf A %4.8lf is too far away from "
					"@avg_last %4.8lf, skip spike catching",
					c_mkts[m1_idx], bid_p, ask_p, avg_last);
				continue;
			}

			double price_ratio_range_min = c_price_ratio_ranges[m1_idx][0];
			double price_ratio_range_max = c_price_ratio_ranges[m1_idx][1];
			double spike_buy_prices[MAX_SPIKES][2] = {
				0.955, 2.0,
				0.91, 3.0,
				0.83, 2.0,
				0.62, 1.0,
				0, 0, 0, 0, 0, 0
			};
			double spike_buy_prices_bittrex[MAX_SPIKES][2] = {
				0.965, 0.5,
				0.955, 2.0,
				0.91, 2.0,
				0.82, 2.0,
				0.72, 2.0,
				0.62, 1.0,
				0.52, 1.0
			}; // 0.965 might work for those rats in ETH-ADA
			bool m1_bittrex = (strcmp(c_mkts[m1_idx], "Bittrex") == 0) ? true : false;
			if (m1_bittrex)
				memcpy(spike_buy_prices, spike_buy_prices_bittrex, sizeof(spike_buy_prices));
			double p_threshold = 1;
			if (TYPE(const_urn_mkt_bear) == T_TRUE)
				p_threshold = 0.93;
			// If market price higher than lowest_bid_p a lot, dont place
			if (bid_p >= 1.01 * lowest_bid_p)
				p_threshold = 1 - (1 - lowest_bid_p/bid_p) * 3; // 0.99 -> 0.97, 0.98 -> 0.94
			// Prepare final spike bid price and size
			double bid_prices[MAX_SPIKES] = {0};
			double bid_sizes[MAX_SPIKES] = {0};
			int bid_idx = 0;
			bool spike_bid = true;
			if (c_run_mode_d == false) spike_bid = false;
			// If market price higher than lowest_bid_p a lot 2%, dont place bids
			if (bid_p >= 1.02 * lowest_bid_p) spike_bid = false;
			// spike_buy_prices = [] if (@deviation_map.dig(m1, 'maker/buy') || 1) >= 0.03
			// spike_buy_prices = [] if (@deviation_map.dig(m1, 'taker/buy') || 1) >= 0.03
			if (devi_map_mk_buy[m1_idx] >= 0.03) spike_bid = false;
			if (devi_map_tk_buy[m1_idx] >= 0.03) spike_bid = false;
			for (int i=0; spike_bid && (i < MAX_SPIKES); i++) {
				if ((spike_buy_prices[i][0] >= p_threshold) ||
						(spike_buy_prices[i][0] <= price_ratio_range_min) ||
						(spike_buy_prices[i][0] >= price_ratio_range_max)) {
					spike_buy_prices[i][0] = 0;
					spike_buy_prices[i][1] = 0;
				}
				double p = bid_p * spike_buy_prices[i][0];
				if (p <= 0) continue;
				// p = market_client(m1).format_price_str(@pair, 'buy', p, adjust:true, verbose:false).to_f
				p = _format_price(p, c_p_steps[m1_idx], BUY);
				// Price might be duplicated after formatted
				if (bid_idx > 0 && bid_prices[bid_idx-1] == p)
					continue;
				bid_prices[bid_idx] = p;
				bid_sizes[bid_idx] = spike_buy_prices[i][1];
				bid_idx++;
			}
			// Cut buy orders by avaiable balance
			// market_client(m1).max_order_size(@pair, 'buy', bid_p) + reserved_bal[m1]['buy']/bid_p*0.99
			double first_order_size_avail = NUM2DBL(rb_funcall(
				cv_mkt_clients[m1_idx], id_max_order_size, 3,
				my_pair, str_buy, DBL2NUM(bid_p)));
			first_order_size_avail += (reserved_bal[m1_idx][BUY]/bid_p*0.99);
			// Assign X to spike_buy_prices
			// Place more in Bittrex ETH and BTC pair, more chances here.
			double buy_o_quota = 0;
			if (m1_bittrex && (strncmp(c_my_pair, "BTC-", 4) == 0)) {
				// For buy orders, place X*@vol_max_const when balance >= (3+2X) * @vol_max_const
				buy_o_quota = ((first_order_size_avail / c_vol_max_const - 3) / 2) - 0.1;
			} else if (m1_bittrex && (strncmp(c_my_pair, "ETH-", 4) == 0)) {
				// For buy orders, place X*@vol_max_const when balance >= (3+4X) * @vol_max_const
				buy_o_quota = ((first_order_size_avail / c_vol_max_const - 3) / 4) - 0.1;
			} else {
				// For buy orders, place X*@vol_max_const when balance >= (15+10X) * @vol_max_const
				buy_o_quota = ((first_order_size_avail / c_vol_max_const - 15) / 10.0) - 0.1;
			}
			_ab3_dbg("S spike_buy  %8s f_o_s_avail %4.8lf quota %4.4lf x %4.4f",
				c_mkts[m1_idx], first_order_size_avail, buy_o_quota, c_vol_max_const);
			// Set real size for each spike bid_sizes
			for (int i=0; i<bid_idx; i++) {
				if (buy_o_quota <= 0) {
					bid_prices[i]=0, bid_sizes[i]=0;
				} else if (buy_o_quota < bid_sizes[i]) { // not enough
					if (buy_o_quota <= 0.3) { // Too little size
						bid_prices[i]=0, bid_sizes[i]=0;
						continue;
					}
					bid_sizes[i] = buy_o_quota * c_vol_max_const;
					buy_o_quota = 0;
				} else {
					buy_o_quota -= bid_sizes[i];
					bid_sizes[i] *= c_vol_max_const;
				}
			}

			// Decide the price to catch knives. Use asset quota wisely for sell side.
			double spike_sell_prices[MAX_SPIKES][2] = {
				1.049, 2.0,
				1.095, 3.0,
				1.14, 1.0,
				1.29, 1.0,
				1.48, 1.0,
				0, 0, 0, 0
			};
			p_threshold = 1;
			// If market price higher than highest_ask_p a lot 10%, dont place ask
			if (highest_ask_p >= 1.01 * ask_p)
				p_threshold = (highest_ask_p/ask_p - 1) * 3 + 1; // 1.01 -> 1.03, 1.02 -> 1.06
			// Prepare final spike ask price and size
			double ask_prices[MAX_SPIKES] = {0};
			double ask_sizes[MAX_SPIKES] = {0};
			int ask_idx = 0;
			bool spike_ask = true;
			if (c_run_mode_e == false) spike_ask = false;
			// If market price lower than highest_ask_p a lot 10%, dont place
			if (ask_p * 1.1 <= highest_ask_p) spike_ask = false;
			// spike_sell_prices = [] if (@deviation_map.dig(m1, 'maker/sell') || 1) >= 0.03
			// spike_sell_prices = [] if (@deviation_map.dig(m1, 'taker/sell') || 1) >= 0.03
			if (devi_map_mk_sel[m1_idx] >= 0.03) spike_ask = false;
			if (devi_map_tk_sel[m1_idx] >= 0.03) spike_ask = false;
			if (strncmp(c_my_pair, "BTC-", 4) == 0) spike_ask = false;
			if (strncmp(c_my_pair, "ETH-", 4) == 0) spike_ask = false;
			for (int i=0; spike_ask && (i < MAX_SPIKES); i++) {
				if ((spike_sell_prices[i][0] <= p_threshold) ||
						(spike_sell_prices[i][0] <= price_ratio_range_min) ||
						(spike_sell_prices[i][0] >= price_ratio_range_max)) {
					spike_sell_prices[i][0] = 0;
					spike_sell_prices[i][1] = 0;
				}
				double p = ask_p * spike_sell_prices[i][0];
				if (p <= 0) continue;
				// p = market_client(m1).format_price_str(@pair, 'sell', p, adjust:true, verbose:false).to_f
				p = _format_price(p, c_p_steps[m1_idx], SEL);
				// Price might be duplicated after formatted
				if (ask_idx > 0 && ask_prices[ask_idx-1] == p)
					continue;
				ask_prices[ask_idx] = p;
				ask_sizes[ask_idx] = spike_sell_prices[i][1];
				ask_idx++;
			}
			// Cut sell orders by avaiable balance
			// market_client(m1).max_order_size(@pair, 'sell', ask_p) + reserved_bal[m1]['sell']
			first_order_size_avail = NUM2DBL(rb_funcall(
				cv_mkt_clients[m1_idx], id_max_order_size, 3,
				my_pair, str_sell, DBL2NUM(ask_p)));
			first_order_size_avail += reserved_bal[m1_idx][SEL];
			// For sell orders, spike orders are decided by how many trade files in task/
			int traders_num = c_trader_tasks_by_mkt[m1_idx];
			double keep_bal = URN_MAX(traders_num*vol_max_c, c_vol_max_const);
			keep_bal = URN_MIN(keep_bal, c_vol_max_const); // No more than @vol_max_const
			// Method A:
			// If all traders have not placed spike catch orders.
			double sell_o_quota_a = ((first_order_size_avail - keep_bal) / c_vol_max_const) - 0.1;
			// But this waste assets at last.
			// Method B: Use total balance instead of available balance
			// Use floor(0) to reduce competition, if need
			VALUE v_bal_each = rb_funcall(cv_mkt_clients[m1_idx], id_balance_ttl_cash_asset, 1, my_pair);
			double total_cash_balance = NUM2DBL(rbary_el(v_bal_each, 0));
			double total_asset_balance = NUM2DBL(rbary_el(v_bal_each, 1));
			double sell_o_quota_b = ((total_asset_balance - keep_bal) / traders_num / c_vol_max_const) - 0.1;
			double sell_o_quota = URN_MIN(sell_o_quota_a, sell_o_quota_b);
			_ab3_dbg("S spike_sell %8s f_o_s_avail %4.8lf quota %4.4lf x %4.4f",
				c_mkts[m1_idx], first_order_size_avail, sell_o_quota, c_vol_max_const);
			// Set real size for each spike ask_sizes
			for (int i=0; i<ask_idx; i++) {
				if (sell_o_quota <= 0) {
					ask_prices[i]=0, ask_sizes[i]=0;
				} else if (sell_o_quota < ask_sizes[i]) { // not enough
					if (sell_o_quota <= 0.3) { // Too little size
						ask_prices[i]=0, ask_sizes[i]=0;
						continue;
					}
					ask_sizes[i] = sell_o_quota * c_vol_max_const;
					sell_o_quota = 0;
				} else {
					sell_o_quota -= ask_sizes[i];
					ask_sizes[i] *= c_vol_max_const;
				}
			}
			// print size details
			_ab3_dbg("S spike_sell %8s ask_sizes[0..3]: %4.4f %4.4f %4.4f %4.4f",
				c_mkts[m1_idx], ask_sizes[0], ask_sizes[1], ask_sizes[2], ask_sizes[3]);
			// If sell_o_quota still has some, apply to first items in spike_sell_prices_in_use
			if ((sell_o_quota > 0) && (ask_prices[0] > 0)) {
				// don't add too much, double is the most
				double max_size = sell_o_quota * c_vol_max_const;
				for (int i=0; i<ask_idx; i++) {
					if ((ask_prices[i] <= 0) || (ask_sizes[i] <= 0))
						continue;
					double s_add = URN_MIN(max_size, ask_sizes[i]);
					ask_sizes[i] += s_add;
					max_size -= s_add;
					if (max_size <= 0) break;
				}
			}
			// print size details
			_ab3_dbg("S spike_sell %8s ask_sizes[0..3]: %4.4f %4.4f %4.4f %4.4f",
				c_mkts[m1_idx], ask_sizes[0], ask_sizes[1], ask_sizes[2], ask_sizes[3]);
			// Find any exist spike order nearby, match its price and size.
			// If there is an order nearby, and 0.3x <= size <= 1.1x, use it.
			// Why 0.3 ~ 1.1? spike order size might shrink to 1/2 or enlarged to 2x
			// Too small size range will make order placing and cancelling too often
			// Too big size range will make order takes too much balance.
			for (int t=0; t<2; t++) { // t = [SEL, BUY]
			for (int i=0; i<(t==BUY ? bid_idx : ask_idx); i++) {
				double p = (t==BUY ? bid_prices[i] : ask_prices[i]);
				double s = (t==BUY ? bid_sizes[i] : ask_sizes[i]);
				if (p <= 0 || s <= 0) continue;
				struct order *match_o = NULL;
				for (struct order *o = own_spike_main_orders[m1_idx][t]; o != NULL; o=o->next) {
					if ((i < 2) && (URN_ABS(diff(o->p, p)) < 0.01) &&
							(o->s <= 1.1*s) && (o->s >= 0.3*s)) {
						match_o = o; break;
					} else if ((i >= 2) && (URN_ABS(diff(o->p, p)) < 0.03) &&
							(o->s <= 1.1*s) && (o->s >= 0.3*s)) {
						match_o = o; break;
					}
				}
				if (match_o == NULL) {
					// Would be a new spike catcher price.
					double last_add_spike_t = c_my_last_t_suggest_new_spike_prices[m1_idx];
					if (i <= 0) {
						// Always suggest first order.
					} else if (now - last_add_spike_t > 30) {
						// Speed control for placing non-first level.
						c_my_last_t_suggest_new_spike_prices[m1_idx] = now;
						_ab3_dbg("\tSuggest adding one more %s spike %d order",
							(t==BUY ? "BUY" : "SEL"), i+1);
					} else {
						_ab3_dbg("\tSkip adding %s spike %d order too fast",
							(t==BUY ? "BUY" : "SEL"), i+1);
						if (t == BUY) {
							bid_prices[i] = 0;
							bid_sizes[i] = 0;
						} else {
							ask_prices[i] = 0;
							ask_sizes[i] = 0;
						}
						continue;
					}
				} else {
					_ab3_dbg("\tNearby spike catcher found for %s %4.8lf %4.8lf:\n%s",
						"BUY", p, s, rborder_desc(match_o));
					if (t == BUY) {
						bid_prices[i] = match_o->p;
						bid_sizes[i] = match_o->s;
					} else {
						ask_prices[i] = match_o->p;
						ask_sizes[i] = match_o->s;
					}
				}
				// Made into chances
				// trigger_order_opt = {}
				p = (t==BUY ? bid_prices[i] : ask_prices[i]);
				s = (t==BUY ? bid_sizes[i] : ask_sizes[i]);
				p = urn_round(p, c_price_precise);
				s = urn_round(s, c_size_precise);
				VALUE v_mo = rborder_new(m1_idx, p, s, t);
				VALUE v_orders = _new_rb_ary(1, v_mo);
				double rate = (t == BUY ? devi_map_mk_buy[m1_idx] : devi_map_mk_sel[m1_idx]);
				// child_price_threshold is for placing last child order,
				// means no chance to take profit in short time, the worst case.
				double child_price_threshold = -1;
				if ((i == 0) && (t == SEL)) {
					// For first spike order, use (price + 2*market_price)/3 to take profit.
					// Try best to get close to market_price
					child_price_threshold = _compute_real_price((2*ask_p+p)/3, rate, t);
				} else if ((i == 0) && (t == BUY)) {
					child_price_threshold = _compute_real_price((2*bid_p+p)/3, rate, t);
				} else if (t == SEL) {
					// Use (price + market_price)/2 to take profit.
					child_price_threshold = _compute_real_price((ask_p+p)/2, rate, t);
				} else if (t == BUY) {
					// Use (price + market_price)/2 to take profit.
					child_price_threshold = _compute_real_price((bid_p+p)/2, rate, t);
				}
				VALUE c = _new_ab3_chance(self, v_orders, m1_idx,
					false, child_price_threshold, 0, c_spike_catas[t][i]);
				rb_ary_push(v_chances, c);
			}
			}
		}
	}

	/* STEP 5 For type B and C */
	_ab3_dbg("STEP 5 type B and C");
	// COST_US 684
	// if low_balance_market[any] is true,
	// rb_ivar_set(self, id_my_need_balance_refresh, Qtrue); at last
	bool low_balance_market[MAX_AB3_MKTS] = {false};
	double exist_price_map[2][MAX_AB3_MKTS] = {-1};
	bool future_side_for_close[MAX_AB3_MKTS][2] = {false};
	for (int m1_idx=0; m1_idx < my_markets_sz; m1_idx++) {
		if (!c_is_fut_mkts[m1_idx]) continue;
		v_tmp = rb_funcall(cv_mkt_clients[m1_idx],
			id_future_side_for_close, 2, my_pair, str_sell);
		if (TYPE(v_tmp) == T_TRUE)
			future_side_for_close[m1_idx][SEL] = true;
		v_tmp = rb_funcall(cv_mkt_clients[m1_idx],
			id_future_side_for_close, 2, my_pair, str_buy);
		if (TYPE(v_tmp) == T_TRUE)
			future_side_for_close[m1_idx][BUY] = true;
	}
	// Detection for case B => "[B] Place main buy order and wait",
	// Detection for case C => "[C] Place main sell order and wait",
	for (int t=0; t<2; t++) { // t = [SEL, BUY]
		if ((t == BUY) && (!c_run_mode_b)) continue;
		if ((t == SEL) && (!c_run_mode_c)) continue;
		char cata[2] = "B";
		if (t==SEL) cata[0] = 'C';
		// Get child market count, split child_capacity.
		int child_mkt_ct[MAX_AB3_MKTS] = {0};
		// TRIGGER_OPTS : [
		// 	price, p_real, child_price_threshold,
		// 	main_bal, type, m_idx,
		// 	child_type, child_capacity // child_mkt_stat:{} seperated
		// ]
		double trigger_order_opt_map[MAX_AB3_MKTS][MAX_AGRSV_TYPES][TRIGGER_OPTS] = {-1};
		// 2: for each trigger_order_opt_map, [sub_mkt][2] : [order_size_sum, bal_avail]
		double child_mkt_stat[MAX_AB3_MKTS][MAX_AGRSV_TYPES][MAX_AB3_MKTS][2] = {-1};
		for (int m1_idx=0; m1_idx < my_markets_sz; m1_idx++) {
			double bid_p = bids_map[m1_idx][0][0];
			double ask_p = asks_map[m1_idx][0][0];
			_ab3_dbg("0.  %s %8s valid %d bid_p %4.8f ask_p %4.8f",
				(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx],
				c_valid_mkts[m1_idx], bid_p, ask_p);
			// COST_US 692 1119
			if (c_valid_mkts[m1_idx] != true) continue;
			if ((bid_p <= 0) || (ask_p <= 0)) continue;
			double (*orders_map)[MAX_AB3_MKTS][max_odbk_depth][3] = (t==BUY ? &bids_map : &asks_map);
			double price_1st = (*orders_map)[m1_idx][0][0];
			double price_2nd = (*orders_map)[m1_idx][1][0];
			double odbk_top_size = (*orders_map)[m1_idx][0][1];
			double (*oppo_orders_map)[MAX_AB3_MKTS][max_odbk_depth][3] = (t==BUY ? &asks_map : &bids_map);
			double oppo_order_price = (*oppo_orders_map)[m1_idx][0][0];
			double p_step = c_p_steps[m1_idx];
			// Pre-compute indicators for this market orderbook.
			double own_live_orders_at_first_size = 0;
			long own_live_orders_at_first_age = -1;
			bool own_live_orders_at_first = false;
			double exist_price = -1;
			for (struct order *o = own_live_orders[m1_idx][t]; o != NULL; o=o->next) {
				if (o->p == price_1st) {
					own_live_orders_at_first = true;
					own_live_orders_at_first_size += (o->remained);
					if (own_live_orders_at_first_age < 0)
						own_live_orders_at_first_age = o->ts_e3;
					else
						own_live_orders_at_first_age = URN_MIN(
							own_live_orders_at_first_age, o->ts_e3);
				}
				if (exist_price < 0) {
					exist_price = o->p;
				} else if (exist_price != (o->p)) {
					for (struct order *o2 = own_live_orders[m1_idx][t]; o2 != NULL; o2=o2->next)
						URN_LOG(rborder_desc(o2));
					rb_raise(rb_eTypeError, "Own main orders prices should be coinsistent.");
				}
			}
			exist_price_map[t][m1_idx] = exist_price;
			bool own_live_orders_at_first_totally = false; // almost all.
			if (odbk_top_size <= 1.1*own_live_orders_at_first_size)
				own_live_orders_at_first_totally = true;
			if (odbk_top_size <= own_live_orders_at_first_size + c_vol_zero)
				own_live_orders_at_first_totally = true;
			bool own_legacy_orders_at_first = false;
			bool own_legacy_orders_at_second = false;
			for (struct order *o = own_legacy_orders[m1_idx][t]; o != NULL; o=o->next) {
				if (o->p == price_1st)
					own_legacy_orders_at_first = true;
				if (o->p == price_2nd)
					own_legacy_orders_at_second = true;
			}
			bool large_price_step = ((p_step/price_1st) >= 0.01) ? true : false;

			// known best price that does not work:
			// 	lowest price for BIDS, highest for ASKS
			double last_tried_p = -1;
			for(int agg=0; agg < MAX_AGRSV_TYPES; agg++) {
				// :yes, :no, :active, :retain
				_ab3_dbg("0.1 %s %8s agg %d",
					(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx], agg);
				// COST_US 698 792 1030, 100us for each p in price_candidates
				int price_candidates_max_ct = 9;
				double price_candidates[9] = {-1};
				if (agg == 0) { // yes
					if (own_live_orders_at_first) // Do not jump over aggressively.
						continue;
					if (own_legacy_orders_at_first) // Do not jump over aggressively.
						continue;
					if (t == SEL) {
						double agg_p = urn_round(price_1st-p_step, c_price_precise);
						if (agg_p <= oppo_order_price)
							continue;
						price_candidates[0] = agg_p;
					} else if (t == BUY) {
						double agg_p = urn_round(price_1st+p_step, c_price_precise);
						if (agg_p >= oppo_order_price)
							continue;
						price_candidates[0] = agg_p;
					} else
						rb_raise(rb_eTypeError, "what the heck is type?");
				} else if (agg == 1) { // no
					price_candidates[0] = price_1st;
					if (large_price_step) {
						/* Just keep using 1st price for large price step pair:
						 * 	HitBTC[DNT/ZRX/TNT]
						 * Set price status to first price of orderbook.
						 * Don't do any step change for market.*/
					} else {
						if (own_live_orders_at_first && own_live_orders_at_first_totally) {
							// Do not block own legacy orders.
							if (own_legacy_orders_at_second) break;
							// Retreat to arbitraged price_2nd if it is too far away.
							// And if it is old enough.
							if ((URN_ABS(price_1st-price_2nd)/price_1st >= c_arbitrage_min) &&
									(own_live_orders_at_first_age >= 20*1000)) {
								if (t == SEL)
									price_candidates[0] = urn_round(price_2nd-p_step, c_price_precise);
								else if (t == BUY)
									price_candidates[0] = urn_round(price_2nd+p_step, c_price_precise);
							}
						}
						if (own_legacy_orders_at_first && !own_legacy_orders_at_second) {
							// Back off for legacy orders when second slot is not legacy too.
							if (t == SEL)
								price_candidates[0] = urn_round(price_1st+p_step, c_price_precise);
							else if (t == BUY)
								price_candidates[0] = urn_round(price_1st-p_step, c_price_precise);
						}
					}
				} else if (agg == 2) { // active
					// Target: get low risk trading volume maximized
					// Maker order should be fully hedged.
					// Maker order should be easy to hit.
					// Dont change order too frequently.
					//
					// Choose [2nd_p+step, 3rd_p+step, 5th_p+step, 9th_p+step] & [valid_range]
					//
					// valid_range = [price_1st, price_1st/(1+@arbitrage_min)] if type == 'buy'
					// valid_range = [price_1st, price_1st/(1-@arbitrage_min)] if type == 'sell'
					double valid_range[2] = {price_1st, price_1st/(1+c_arbitrage_min)};
					valid_range[1] = price_1st/(1-c_arbitrage_min);
					if (valid_range[0] > valid_range[1]) {
						double tmp = valid_range[0];
						valid_range[0] = valid_range[1];
						valid_range[1] = tmp;
					}
					int p_idx_candidates[4] = {1, 2, 4, 8};
					double p_candidates_last = -1;
					int price_candidates_next_idx = 0;
					for (int p_idx=0; p_idx < 4; p_idx++) {
						int idx = p_idx_candidates[p_idx];
						double p_candi = (*orders_map)[m1_idx][idx][0];
						if (p_candi <= 0) continue;
						if (p_candi != exist_price) {
							if (t == BUY)
								p_candi += p_step;
							else
								p_candi -= p_step;
						}
						if (p_candi < valid_range[0]) {
							continue;
						} else if (p_candi > valid_range[1]) {
							continue;
						}
						p_candi = urn_round(p_candi, c_price_precise);
						p_candidates_last = p_candi;
						price_candidates[price_candidates_next_idx] = p_candi;
						price_candidates_next_idx ++;
					}
					// If existed_price is better than price_candidate.worst, keep it.
					// Reset all price_candidates, keep exist_price only.
					if ((p_candidates_last > 0) && (exist_price > 0)) {
						if ((t == BUY) && (exist_price >= p_candidates_last)) {
							memset(price_candidates, 0, sizeof(price_candidates));
							price_candidates[0] = exist_price;
						} else if ((t == SEL) && (exist_price <= p_candidates_last)) {
							memset(price_candidates, 0, sizeof(price_candidates));
							price_candidates[0] = exist_price;
						}
					}
					_ab3_dbg("\t active p select %8s exist %4.8lf ->[%4.8lf %4.8lf %4.8lf %4.8lf]",
						c_mkts[m1_idx], exist_price,
						price_candidates[0], price_candidates[1],
						price_candidates[2], price_candidates[3]);
				} else if (agg == 3) { // No chance, maybe keep exist main orders.
					if (exist_price <= 0) // No exist main order.
						continue;
					if (URN_ABS(diff(exist_price, price_1st)) > 0.03) // Price is too far away.
						continue;
					price_candidates[0] = exist_price;
				} else {
					rb_raise(rb_eTypeError, "what the heck is agg?");
				}
				// # Discard abnormal prices.
				bool price_choosen = false;
				_ab3_dbg("0.2 %s %8s agg %d top_p %4.8lf p_candidates[%4.8lf %4.8lf %4.8lf %4.8lf]",
					(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx], agg, price_1st,
					price_candidates[0], price_candidates[1],
					price_candidates[2], price_candidates[3]);
				// COST_US 712
				double rate = (t == BUY ? devi_map_mk_buy[m1_idx] : devi_map_mk_sel[m1_idx]);
				for(int p_idx=0; p_idx < price_candidates_max_ct; p_idx++) {
					double price = price_candidates[p_idx];
					if (price <= 0) continue;
					// Skip higher price in BIDS and lower price in ASKS
					if ((last_tried_p > 0) && (t == BUY) && (price >= last_tried_p)) {
						_ab3_dbg("0.3 %s %8s agg %d skip p %4.8lf",
							(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx],
							agg, price);
						continue;
					} else if ((last_tried_p > 0) && (t == SEL) && (price <= last_tried_p)) {
						_ab3_dbg("0.3 %s %8s agg %d skip p %4.8lf",
							(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx],
							agg, price);
						continue;
					}
					// price = urn_round(price, c_price_precise+2); // did this before
					// Compute real price of main orders as MAKER.
					// market_client(m1).preprocess_deviation(@pair, t:"maker/#{type}")
					double p_real = _compute_real_price(price, rate, t);
					// trigger_order_opt = {}
					double (*trigger_order_opt)[TRIGGER_OPTS] = &(trigger_order_opt_map[m1_idx][agg]);
					(*trigger_order_opt)[0] = price;
					(*trigger_order_opt)[1] = p_real;
					if (t == BUY)
						(*trigger_order_opt)[2] = p_real*(1+c_last_order_arbitrage_min);
					else
						(*trigger_order_opt)[2] = p_real/(1+c_last_order_arbitrage_min);
					// child_price_threshold price precision must +2 or MAX
					double child_price_threshold = (*trigger_order_opt)[2];
					// Do this later, after checked children markets
					// (*trigger_order_opt)[3] = first_order_size_avail;
					(*trigger_order_opt)[4] = t;
					(*trigger_order_opt)[5] = m1_idx;
					(*trigger_order_opt)[6] = (t==BUY ? SEL : BUY);
					int child_type = (*trigger_order_opt)[6];
					_ab3_dbg("1.  %s %8s agg %d p %4.8lf exist %4.8lf large_p_step "
						"%d rate %4.8lf p_real %4.10lf cpt %4.10lf",
						(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx],
						agg, price, exist_price, large_price_step,
						rate, p_real, child_price_threshold);
					// All other market ask orders could be target orders.
					int child_m_choosen = 0;
					for (int m2_idx=0; m2_idx < my_markets_sz; m2_idx++) {
						if (c_valid_mkts[m2_idx] != true) continue;
						if (m1_idx == m2_idx) continue;
						/*
						_ab3_dbg("1.1 %s %8s %8s %d",
							(t==BUY ? "BUY" : "SEL"),
							c_mkts[m1_idx], c_mkts[m2_idx], agg);
						*/
						double orders[max_odbk_depth][3] = {0};
						double order_size_sum = 0;
						for(int d=0; d<max_odbk_depth; d++) {
							double p_take = (*orders_map)[m2_idx][d][2];
							bool valid = false;
							if ((t == SEL) && (p_take <= child_price_threshold))
								valid = true;
							if ((t == BUY) && (p_take >= child_price_threshold))
								valid = true;
							// print in GREEN if valid
							_ab3_dbg("%s1.2 %s %8s agg %d %8s px[%d] chk tk %4.10f"
								" %c= %4.10lf ? %d" CLR_RST,
								(valid ? CLR_GREEN : ""),
								(t==BUY ? "BUY" : "SEL"),
								c_mkts[m1_idx], agg, c_mkts[m2_idx], d,
								p_take, (t==SEL ? '<' : '>'),
								child_price_threshold, valid);
							if (!valid) break; // orders_map sorted already.
							orders[d][0] = (*orders_map)[m2_idx][d][0]; // p
							orders[d][1] = (*orders_map)[m2_idx][d][1]; // s
							orders[d][2] = (*orders_map)[m2_idx][d][2]; // p_take
							order_size_sum += orders[d][1];
						}
						// Fast check
						if (order_size_sum < my_vol_min) continue;

						_ab3_dbg("1.3 %s %8s %8s agg %d fast check passed "
							"o_s_sum %4.8lf p %4.8lf step %4.8lf %4.8lf",
							(t==BUY ? "BUY" : "SEL"),
							c_mkts[m1_idx], c_mkts[m2_idx], agg,
							order_size_sum, price, p_step, my_vol_min);
						double rate_c = devi_map_mk_buy[m2_idx];
						if (child_type == SEL)
							rate_c = devi_map_mk_sel[m2_idx];
						double ideal_child_p = _compute_shown_price(
							child_price_threshold, rate_c, child_type);
						if (ideal_child_p <= 0) continue;

						double bal_avail = NUM2DBL(rb_funcall(cv_mkt_clients[m2_idx],
							id_max_order_size, 3, my_pair,
							(child_type==BUY ? str_buy : str_sell),
							DBL2NUM(ideal_child_p)));
						_ab3_dbg("1.5 %s %8s %8s %d (%4.8lf %4.8lf %4.8lf) %4.8lf %4.8lf %4.8lf",
							(t==BUY ? "BUY" : "SEL"),
							c_mkts[m1_idx], c_mkts[m2_idx], agg,
							(*orders_map)[m2_idx][0][0],
							(*orders_map)[m2_idx][0][1],
							(*orders_map)[m2_idx][0][2],
							bal_avail, reserved_bal[m2_idx][child_type],
							ideal_child_p);
						if (t == BUY) // Add child market m2 reserved_bal, so type is opponent
							bal_avail += reserved_bal[m2_idx][child_type];
						else
							bal_avail += reserved_bal[m2_idx][child_type]/ideal_child_p*0.99;
						bal_avail = urn_round(bal_avail, c_size_precise);
						// @need_balance_refresh ||= bal_avail < @vol_min
						if ((bal_avail < my_vol_min) && (TYPE(my_need_balance_refresh) == T_TRUE))
							low_balance_market[m2_idx] = true;
						if (bal_avail < my_vol_min) { //Low balance.
							// Ignore vol_min when closing position for future markets.
							// if market_client(m).future_side_for_close(@pair, child_type)
							if (future_side_for_close[m2_idx][child_type]) {
								// nothing
							} else {
								_ab3_dbgc(RED,
									"  Low bal %8s: %s back for %8s %4.8lf < %4.8lf",
									c_mkts[m2_idx],
									(child_type==BUY ? "BUY" : "SEL"),
									c_mkts[m1_idx], bal_avail, my_vol_min);
								low_balance_market[m2_idx] = true;
								continue;
							}
						}
						child_mkt_stat[m1_idx][agg][m2_idx][0] = order_size_sum;
						child_mkt_stat[m1_idx][agg][m2_idx][1] = bal_avail;
						_ab3_dbg("2.5 choosen %s %8s -> %8s agg %d "
							"c_m_stat[%d][%d][%d]: o_s_sum %4.4lf b_avail %4.4lf",
							(t==BUY ? "BUY" : "SEL"),
							c_mkts[m1_idx], c_mkts[m2_idx], agg,
							m1_idx, agg, m2_idx,
							order_size_sum, bal_avail);
						child_mkt_ct[m2_idx] += 1;
						child_m_choosen ++;
					} // For m2_idx
//					_ab3_dbg("2.7 p_real %4.8lf", p_real); // cause Abort trap 6
					_ab3_dbg("2.8 %s %8s agg %d p %4.8lf exist %4.8lf large_p_step "
						"%d rate %4.8lf cpt %4.8lf child_m_choosen %d",
						(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx],
						agg, price, exist_price, large_price_step,
						rate, child_price_threshold, child_m_choosen);
					// next if trigger_order_opt[:child_mkt_stat].empty?
					if (child_m_choosen == 0) {
						(*trigger_order_opt)[0] = 0; // mark as invalid
						// price does not work, update last_tried_p
						// record lowest price for BIDS, highest for ASKS
						if (last_tried_p < 0)
							last_tried_p = price;
						else if ((t == BUY) && (last_tried_p > price))
							last_tried_p = price;
						else if ((t == SEL) && (last_tried_p < price))
							last_tried_p = price;
						continue;
					}
					// Calculate first_order_size_avail at last, time consuming.
					double first_order_size_avail = NUM2DBL(rb_funcall(
						cv_mkt_clients[m1_idx], id_max_order_size, 3,
						my_pair, (t==BUY ? str_buy : str_sell), DBL2NUM(price)));
					if (t == BUY)
						first_order_size_avail += reserved_bal[m1_idx][t]/price*0.99;
					else
						first_order_size_avail += reserved_bal[m1_idx][t];
					first_order_size_avail = urn_round(first_order_size_avail, c_size_precise);
					(*trigger_order_opt)[3] = first_order_size_avail;
					// Warn low balance only when chance is appearred.
					if ((first_order_size_avail < my_vol_min) && !c_is_fut_mkts[m1_idx]) {
						_ab3_dbgc(RED, "  Low bal %8s: %s  %4.8lf < %4.8lf ---",
							c_mkts[m1_idx],
							(t==BUY ? "BUY" : "SEL"),
							first_order_size_avail, my_vol_min);
						low_balance_market[m1_idx] = true;
						(*trigger_order_opt)[0] = 0; // mark as invalid
						// price does not work, update last_tried_p
						// record lowest price for BIDS, highest for ASKS
						if (last_tried_p < 0)
							last_tried_p = price;
						else if ((t == BUY) && (last_tried_p > price))
							last_tried_p = price;
						else if ((t == SEL) && (last_tried_p < price))
							last_tried_p = price;
						continue;
					}
					// trigger_order_opt_map[m1][aggressive] = trigger_order_opt
					// pointer set already.
					price_choosen = true;
					// Only choose one price for one aggressive mode.
					_ab3_dbg("2.9 %s %8s agg %d p %4.8lf exist %4.8lf large_p_step "
						"%d rate %4.8lf cpt %4.8lf price_chosen",
						(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx],
						agg, price, exist_price, large_price_step,
						rate, child_price_threshold);
//					_ab3_dbg("2.9 p_real %4.8lf", p_real); // cause Abort trap 6
					break;
				} // For price in price_candidate
				// Only choose one aggressive mode.
				// TODO so a bad aggressive choice would omit other good ones.
				if (price_choosen) break;
			} // For agg in [yes no active retain]
		} // For m1_idx
		// Below should be done out of the loop:
		// @low_balance_market = low_balance_market.uniq.sort
		int unprocessed_child_mkt_ct[MAX_AB3_MKTS] = {0};
		// unprocessed_child_mkt_ct = child_mkt_ct.clone
		memcpy(unprocessed_child_mkt_ct, child_mkt_ct, sizeof(child_mkt_ct));
		double reserved_child_capacity[MAX_AB3_MKTS] = {0};
		// Sort market descend by its child role count.
		// Continue building trigger_order_opt
		// mkt_ct, mkt_idx = mkts.size, 0
		// mkts.sort_by { |m| child_mkt_ct[m] }.reverse.each { |m1| } 
		struct two_i mkt_w_child_ct[my_markets_sz];
		for (int i=0; i < my_markets_sz; i++) {
			mkt_w_child_ct[i].a = 0-child_mkt_ct[i]; // reverse
			mkt_w_child_ct[i].b = i;
		}
		qsort(&mkt_w_child_ct, my_markets_sz, sizeof(struct two_i), two_i_comparitor);
		_ab3_dbg("3 %s loop start", (t==BUY ? "BUY" : "SEL"));
		for(int lp_i=0; lp_i < my_markets_sz; lp_i++) {
			int m1_idx = mkt_w_child_ct[lp_i].b;
			if (c_valid_mkts[m1_idx] != true) continue;
			for(int agg=0; agg < MAX_AGRSV_TYPES; agg++) {
				// :yes, :no, :active, :retain
				double (*trigger_order_opt)[TRIGGER_OPTS] = &(trigger_order_opt_map[m1_idx][agg]);
				double price = (*trigger_order_opt)[0];
				double p_real = (*trigger_order_opt)[1];
				_ab3_dbg("3 %s %8s [%d] agg %d price %4.8lf",
					(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx],
					m1_idx, agg, price);
				if (price <= 0) continue;
				double suggest_size = 0;
				double reserved_child_capacity_this_turn[MAX_AB3_MKTS] = {0};
				// Abort generating explains in C version
				// explain_details = {'overall'=>"#{short_ts}[#{trigger_order_opt[:child_mkt_stat].size}]"}
				//
				// trigger_order_opt[:child_mkt_stat] = trigger_order_opt[:child_mkt_stat].to_a.
				// 	sort_by { |mv| child_mkt_ct[mv[0]] }.reverse.to_h
				for(int lp_j=0; lp_j < my_markets_sz; lp_j++) {
					int m2_idx = mkt_w_child_ct[lp_j].b; // reverse sorted by child_mkt_ct
					if (c_valid_mkts[m2_idx] != true) continue;
					// order_size_sum is sum(size) of target child orders in market m2
					double order_size_sum = child_mkt_stat[m1_idx][agg][m2_idx][0];
					double bal_avail = child_mkt_stat[m1_idx][agg][m2_idx][1];
					order_size_sum = URN_MIN(order_size_sum * c_typebc_cap_rate, bal_avail);
					_ab3_dbg("3 %s %8s %8s [%d] agg %d match o_s_sum %4.4f "
						"bal_avail %4.4f rsv_c_capa %4.4lf rsv_c_capa_t %4.4lf",
						(t==BUY ? "BUY" : "SEL"),
						c_mkts[m1_idx], c_mkts[m2_idx],
						m2_idx, agg, order_size_sum, bal_avail,
						reserved_child_capacity[m2_idx],
						reserved_child_capacity_this_turn[m2_idx]);
					// next if order_size_sum < @vol_omit
					if (order_size_sum < my_vol_omit) continue;

					double m2_min_o_sz = _min_order_size(m2_idx, price, t);
					double vol_min = URN_MAX(my_vol_min, m2_min_o_sz);
					// Minus reserved value.
					order_size_sum -= reserved_child_capacity[m2_idx];
					order_size_sum -= reserved_child_capacity_this_turn[m2_idx];
					double capacity = order_size_sum/unprocessed_child_mkt_ct[m2_idx];
					// When available capacity is very small, do something special.
					if ((capacity < vol_min) && (order_size_sum < vol_min)) {
						// Save capacity for last one.
						capacity = (lp_j < my_markets_sz-1) ? 0 : order_size_sum;
					} else if ((capacity < vol_min) && (order_size_sum >= vol_min) && (order_size_sum < 2*vol_min)) {
						capacity = order_size_sum; // Use full capacity.
					} else if ((capacity < my_vol_omit) && (order_size_sum >= 2*my_vol_omit)) {
						capacity = vol_min;
					}
					_ab3_dbg("3 %s %8s %8s agg %d un_c_m_ct %d c_m_ct %d "
						"capa %4.8lf [%4.8lf %4.8lf]",
						(t==BUY ? "BUY" : "SEL"),
						c_mkts[m1_idx], c_mkts[m2_idx], agg,
						unprocessed_child_mkt_ct[m2_idx], child_mkt_ct[m2_idx], capacity,
						order_size_sum, bal_avail);
					reserved_child_capacity_this_turn[m2_idx] += capacity;
					_ab3_dbg("3 Suppose reserved_child_capacity[%8s]"
						" %4.8lf + %4.8lf bal_avail %4.8lf",
						c_mkts[m2_idx],
						reserved_child_capacity[m2_idx],
						reserved_child_capacity_this_turn[m2_idx],
						bal_avail
					);
					suggest_size += capacity;
					unprocessed_child_mkt_ct[m2_idx] --;
				}
				suggest_size = urn_round(suggest_size, c_size_precise);
				(*trigger_order_opt)[7] = suggest_size;
				double main_bal = (*trigger_order_opt)[3];
				double first_order_size_avail = main_bal;
				const double min_size = _min_order_size(m1_idx, price, t);
				// next if @vol_max[cata] < min_size
				double vol_max_cata = vol_max_b;
				if (cata[0] == 'C')
					vol_max_cata = vol_max_c;
				if (vol_max_cata < min_size) continue;
				double max_size = vol_max_cata;
				// In many cases, this leads too small size
				if (c_single_vol_ratio*vol_max_cata >= my_vol_min)
					max_size = c_single_vol_ratio*vol_max_cata;
				if ((max_size < min_size) && (vol_max_cata >= min_size))
					max_size = min_size;
				_ab3_dbg("4.1 f_o_s_avail %5.8lf sug_s %5.8lf max_s %5.8lf "
					"s_v_ratio %lf vol_max_cata %5.8lf",
					first_order_size_avail, suggest_size, max_size,
					c_single_vol_ratio, vol_max_cata);
				suggest_size = URN_MIN(first_order_size_avail, suggest_size);
				suggest_size = URN_MIN(suggest_size, max_size);
				_ab3_dbgc(RED, "urn_round %lf to %d", suggest_size, c_size_precise);
				suggest_size = urn_round(suggest_size, c_size_precise);
				_ab3_dbg("4.2 %s %8s %d %4.8lf@%4.4lf vol_min %4.4lf fsfc %d",
					(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx], agg,
					suggest_size, price, my_vol_min,
					!future_side_for_close[m1_idx][t]);
				if ((suggest_size < my_vol_min) && !future_side_for_close[m1_idx][t])
					continue;
				double ideal_profit = suggest_size * p_real * c_arbitrage_min;
				double exist_main_order_size = main_reserved_bal[m1_idx][t];
				double typeBC_max_single_main_order_size = URN_MAX(
						c_vol_max_const/main_order_min_num_map[m1_idx],
						my_vol_min
				);
				double typeBC_min_single_main_order_size = URN_MAX(
					c_vol_max_const/10, my_vol_min
				);
				double min_bar = 0;
				if (suggest_size > 1.5 * exist_main_order_size) {
					// Consider the existed main orders, get the incremental size.
					suggest_size -= exist_main_order_size;
					// We want to split incremental suggest size into at least N pieces.
					suggest_size = URN_MIN(suggest_size,
						c_vol_max_const/main_order_min_num_map[m1_idx]);
					suggest_size = URN_MIN(suggest_size,
						typeBC_max_single_main_order_size);
					min_bar = URN_MAX(my_vol_min,
						suggest_size/main_order_min_num_map[m1_idx]);
					min_bar = URN_MAX(min_bar,
						typeBC_min_single_main_order_size);
					suggest_size = URN_MIN(suggest_size, min_bar);
					// Restore suggest_size by adding current existed main order size.
					suggest_size += exist_main_order_size;
					suggest_size = urn_round(suggest_size, c_size_precise);
					suggest_size = URN_MAX(suggest_size, min_size);
				}
				_ab3_dbg("4.3 %s %8s %d %4.8lf@%4.4lf "
					"exist_m_s %4.4lf ["
					"v_max_const/m_o_m_num %4.4lf "
					"bc_max_s_m_o_s %4.4lf "
					"m_o_m_num %d "
					"bc_min_s_m_o_s %4.4lf "
					"min_s %4.4lf "
					"min_bar %4.4lf",
					(t==BUY ? "BUY" : "SEL"), c_mkts[m1_idx], agg,
					price, suggest_size, exist_main_order_size,
					c_vol_max_const/main_order_min_num_map[m1_idx],
					typeBC_max_single_main_order_size,
					main_order_min_num_map[m1_idx],
					typeBC_min_single_main_order_size,
					min_size, min_bar);
				if ((suggest_size < my_vol_min) && !future_side_for_close[m1_idx][t])
					continue;
				// Do not enlarge a tiny exist main order size.
				if ((suggest_size-exist_main_order_size) < min_size)
					if (suggest_size > exist_main_order_size)
						suggest_size = exist_main_order_size;
				if (exist_price_map[t][m1_idx] == price) {
					double exist_size = main_reserved_bal[m1_idx][t];
					if (suggest_size < exist_size) continue;
					_ab3_dbg("Keep own main orders %8s %s",
						c_mkts[m1_idx], (t==BUY ? "BUY" : "SEL"));
					suggest_size = exist_size;
				}
				for (int m2_idx=0; m2_idx < my_markets_sz; m2_idx++) {
					_ab3_dbg("Make reserved_child_capacity[%8s]=%4.8f + %4.8f",
						c_mkts[m2_idx], reserved_child_capacity[m2_idx],
						reserved_child_capacity_this_turn[m2_idx]);
					reserved_child_capacity[m2_idx] += reserved_child_capacity_this_turn[m2_idx];
				}
				if (strcmp(c_mkts[m1_idx], "Binance") == 0) {
					// Shit exchanges allows old and new price precision in same orderbook.
					double p_step = c_p_steps[m1_idx];
					double adj_p = _format_price(price, p_step, t);
					if (price != adj_p) {
						_ab3_dbgc(RED,
							"%8s Order price adjusted from %4.8f to %4.8f, "
							"type %s chance changed",
							c_mkts[m1_idx], price, adj_p, cata);
						price = adj_p;
						(*trigger_order_opt)[0] = adj_p;
					}
				}
				VALUE mo = rborder_new(m1_idx, price, suggest_size, t);
				rb_hash_aset(mo, rb_str_new2("p_real"), p_real);
				// Main order size of type B/C should be choosen wisely,
				// according to rules of other markets,
				// to avoid tiny remain orders.
				VALUE all_orders = rb_ary_new2(MAX_AB3_MKTS);
				VALUE all_clients = rb_ary_new2(MAX_AB3_MKTS);
				for (int m2_idx=0; m2_idx < my_markets_sz; m2_idx++) {
					rb_ary_push(all_clients, cv_mkt_clients[m2_idx]);
					if (m1_idx == m2_idx) {
						rb_ary_push(all_orders, mo);
						continue;
					}
					VALUE o2 = rborder_new(m2_idx, price, suggest_size, (t==BUY ? SEL : BUY));
					rb_ary_push(all_orders, o2);
				}
				rb_funcall(self, id_equalize_order_size, 2, all_orders, all_clients);
				VALUE adjusted_mo = rbary_el(all_orders, m1_idx);
				double adjusted_mo_s = NUM2DBL(rb_hash_aref(adjusted_mo, str_s));
				if (suggest_size != adjusted_mo_s) {
					_ab3_dbg("mo -> adjusted_mo %4.8f -> %4.8f",
						suggest_size, adjusted_mo_s);
					suggest_size = adjusted_mo_s;
				}
				// equalize_order_size() and downgrade_order_size_for_all_market()
				// would shrink order size a little smaller than vol_min
				if (adjusted_mo_s <= 0) continue;
				if ((adjusted_mo_s < 0.8*my_vol_min) && !future_side_for_close[m1_idx][t])
					continue;
				VALUE main_orders = rb_ary_new2(1);
				rb_ary_push(main_orders, adjusted_mo);
				VALUE c = _new_ab3_chance(self, main_orders, m1_idx, false, -1, ideal_profit, cata);
				rb_hash_aset(c, sym_aggressive, c_aggr_syms[agg]);
				rb_ary_push(v_chances, c);
			} // For agg in [yes no active retain]
		} // For m1_idx
	} // For t [SEL BUY]

	for (int i=0; i < MAX_AB3_MKTS; i++) {
		for (int t=0; t < 2; t++) {
			_free_order_chain(own_main_orders[i][t]);
			_free_order_chain(own_spike_main_orders[i][t]);
			_free_order_chain(own_live_orders[i][t]);
			_free_order_chain(own_legacy_orders[i][t]);
		}
	}

	// @low_balance_market = low_balance_market.uniq.sort
	VALUE my_low_balance_market = rb_ary_new();
	for (int i=0; i < my_markets_sz; i++) {
		if (low_balance_market[i]) {
			_ab3_dbg("E mark %d %s as low balance", i, c_mkts[i]);
			rb_ary_push(my_low_balance_market, rb_str_new2(c_mkts[i]));
		}
	}
	rb_ivar_set(self, id_my_low_balance_market, my_low_balance_market);

	if (TYPE(rbary_el(v_chances, 0)) == T_NIL) {
		_ab3_dbg_end;
		return rb_ary_new();
	}

	// Sort chances by ideal profit, processing them in this order to avoid hitting own orders.
	int chance_ct = rbary_sz(v_chances);
	_ab3_dbg("Sort %d chances by ideal profit", chance_ct);
	struct rbv_w_dbl c_w_ip[chance_ct];
	rbary_each(v_chances, idx, max, v_el) {
		c_w_ip[idx].v = v_el;
		v_tmp = rb_hash_aref(v_el, sym_ideal_profit);
		if (TYPE(v_tmp) == T_NIL)
			c_w_ip[idx].d = 0;
		else
			c_w_ip[idx].d = 0-NUM2DBL(v_tmp); // reverse
	}
	qsort(&c_w_ip, chance_ct, sizeof(struct rbv_w_dbl), rbv_w_dbl_comparitor);
	_ab3_dbg("Return %ld chances, should group them by cata then mkt", max);

	for (int i=0; c_my_debug && i<max; i++) {
		char c_desc[1024];
		VALUE c = c_w_ip[i].v;
		sprintf_ab3_chance(c_desc, c);
		_ab3_dbg("\t%d/%ld %s", i+1, max, c_desc);
	}

	VALUE result = rb_ary_new();
	for (int i=0; i<max; i++) {
		VALUE c = c_w_ip[i].v;
		rb_ary_push(result, c);
	}

	_ab3_dbg("method_detect_arbitrage_pattern() done");
	_ab3_dbg_end;
	return result;
}
