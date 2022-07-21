#include "ruby.h"
#include "math.h"

//#define URN_MAIN_DEBUG // enable URN_DEBUGF
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
ID id_min_order_size;
ID id_equalize_order_size;
ID id_downgrade_order_size_for_all_market;
ID id_price_real;
ID id_price_step;
ID id_price_ratio_range;
ID id_object_id;
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
VALUE sym_suggest_size;
VALUE sym_explain;
ID id_my_market_data_agt;
ID id_my_markets;
ID id_my_keep_exist_spike_order;
ID id_my_market_snapshot;
ID id_my_arbitrage_min;
ID id_my_pair;
ID id_my_single_vol_ratio;
ID id_my_vol_max;
ID id_my_vol_min;
ID id_my_last_order_arbitrage_min;
ID id_my_run_mode;
ID id_my_spike_catcher_enabled;
ID id_my_avg_last;
void ab3_init() {
	if (ab3_inited) return;
	id_valid_markets = rb_intern("valid_markets");
	id_size = rb_intern("size");
	id_dig = rb_intern("dig");
	id_order_alive_qm = rb_intern("order_alive?");
	id_market_client = rb_intern("market_client");
	id_max_order_size = rb_intern("max_order_size");
	id_min_order_size = rb_intern("min_order_size");
	id_equalize_order_size = rb_intern("equalize_order_size");
	id_downgrade_order_size_for_all_market = rb_intern("downgrade_order_size_for_all_market");
	id_price_real = rb_intern("price_real");
	id_price_step = rb_intern("price_step");
	id_price_ratio_range = rb_intern("price_ratio_range");
	id_object_id = rb_intern("object_id");

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
	sym_suggest_size = ID2SYM(rb_intern("suggest_size"));
	sym_explain = ID2SYM(rb_intern("explain"));

	id_my_market_data_agt = rb_intern("@market_data_agt");
	id_my_markets = rb_intern("@markets");
	id_my_keep_exist_spike_order = rb_intern("@_keep_exist_spike_order");
	id_my_market_snapshot = rb_intern("@market_snapshot");
	id_my_arbitrage_min = rb_intern("@arbitrage_min");
	id_my_pair = rb_intern("@pair");
	id_my_single_vol_ratio = rb_intern("@single_vol_ratio");
	id_my_vol_max = rb_intern("@vol_max");
	id_my_vol_min = rb_intern("@vol_min");
	id_my_last_order_arbitrage_min = rb_intern("@last_order_arbitrage_min");
	id_my_run_mode = rb_intern("@run_mode");
	id_my_spike_catcher_enabled = rb_intern("@spike_catcher_enabled");
	id_my_avg_last = rb_intern("@avg_last");

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
	((double)((RB_TYPE_P(v, T_FIXNUM) == 1) ? NUM2LONG(v) : NUM2DBL(v)))
#define rbnum_to_long(v) \
	((long)((RB_TYPE_P(v, T_FIXNUM) == 1) ? NUM2LONG(v) : NUM2DBL(v)))

#define BUY 1
#define SEL 0
struct order {
	char m[32];
	char status[32];
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
static void rborder_to_order(VALUE v_o, struct order *o) {
	strcpy(o->m, RSTRING_PTR(rbhsh_el(v_o, "market")));
	strcpy(o->status, RSTRING_PTR(rbhsh_el(v_o, "status")));
	o->p = rbnum_to_dbl(rbhsh_el(v_o, "p"));
	o->s = rbnum_to_dbl(rbhsh_el(v_o, "s"));
	o->executed = rbnum_to_dbl(rbhsh_el(v_o, "executed"));
	o->remained = rbnum_to_dbl(rbhsh_el(v_o, "remained"));
	if (RB_TYPE_P(rbhsh_el(v_o, "v"), T_NIL) != 1) {
		o->vol_based = true;
		o->v = rbnum_to_long(rbhsh_el(v_o, "v"));
		o->executed_v = rbnum_to_long(rbhsh_el(v_o, "executed_v"));
		o->remained_v = rbnum_to_long(rbhsh_el(v_o, "remained_v"));
	} else
		o->vol_based = false;
	o->ts_e3 = rbnum_to_long(rbhsh_el(v_o, "t"));
	char *type = RSTRING_PTR(rbhsh_el(v_o, "T"));
	if (strcmp("buy", type) == 0)
		o->buy = true;
	else
		o->buy = false;
	o->next = NULL;
	o->valid = true;
}
static int index_of(int max, char **strings, char *s) {
	if (s == NULL) return -1;
	for (int i=0; i<max; i++) {
		if (strings[i] == NULL)
			continue;
		if (strcmp(strings[i], s) == 0)
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

/*
 * Local cache for trader
 */
#define MAX_AB3_MKTS 9
#define MAX_SPIKES 7
long cache_trader_obj_id = -1;
VALUE my_market_data_agt;
VALUE my_markets;
VALUE my_keep_exist_spike_order;
VALUE my_market_snapshot;
VALUE c_vol_max_const;
VALUE my_pair;
char *c_my_pair;
VALUE my_run_mode;
VALUE my_spike_catcher_enabled;
int my_markets_sz;
char *c_mkts[MAX_AB3_MKTS] = {NULL};
VALUE cv_mkts[MAX_AB3_MKTS] = {Qnil};
VALUE cv_mkt_clients[MAX_AB3_MKTS] = {Qnil};
bool c_run_mode_a=false, c_run_mode_b=false, c_run_mode_c=false;
bool c_run_mode_d=false, c_run_mode_e=false;
double c_arbitrage_min;
double c_last_order_arbitrage_min;
bool c_valid_mkts[MAX_AB3_MKTS] = {false};
double c_price_ratio_ranges[MAX_AB3_MKTS][2] = {-1};
double c_p_steps[MAX_AB3_MKTS] = {-1};
VALUE const_urn = Qnil;
VALUE const_urn_mkt_bull = Qnil;
VALUE const_urn_mkt_bear = Qnil;
double c_deviation_map_mk_buy[MAX_AB3_MKTS] = {1};
double c_deviation_map_tk_buy[MAX_AB3_MKTS] = {1};
double c_deviation_map_mk_sel[MAX_AB3_MKTS] = {1};
double c_deviation_map_tk_sel[MAX_AB3_MKTS] = {1};

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
	long idx, max; // for rbary_each()
	VALUE v_el, v_tmp; // for tmp usage.

	my_market_data_agt = rb_ivar_get(self, id_my_market_data_agt);
	my_markets = rb_ivar_get(self, id_my_markets);
	my_keep_exist_spike_order = rb_ivar_get(self, id_my_keep_exist_spike_order);
	my_market_snapshot = rb_ivar_get(self, id_my_market_snapshot);
	c_vol_max_const = NUM2DBL(rb_ivar_get(self, rb_intern("@vol_max_const")));
	my_pair = rb_ivar_get(self, id_my_pair);
	c_my_pair = RSTRING_PTR(my_pair);
	my_run_mode = rb_ivar_get(self, id_my_run_mode);
	rbary_each(my_run_mode, idx, max, v_el) {
		char * mode = RSTRING_PTR(v_el);
		if (strcmp(mode, "A") == 0) c_run_mode_a = true;
		if (strcmp(mode, "B") == 0) c_run_mode_b = true;
		if (strcmp(mode, "C") == 0) c_run_mode_c = true;
	}
	c_run_mode_d=c_run_mode_b, c_run_mode_e=c_run_mode_c;
	my_spike_catcher_enabled = rb_ivar_get(self, id_my_spike_catcher_enabled);

	my_markets_sz = rbary_sz(my_markets);
	if (my_markets_sz > MAX_AB3_MKTS)
		rb_raise(rb_eTypeError, "MAX_AB3_MKTS too small, rise it now");
	for (int i=0; i < my_markets_sz; i++) {
		cv_mkts[i] = rbary_el(my_markets, i);
		cv_mkt_clients[i] = rb_funcall(self, id_market_client, 1, cv_mkts[i]);
		c_mkts[i] = RSTRING_PTR(cv_mkts[i]);
	}
	for (int i=my_markets_sz; i < MAX_AB3_MKTS; i++) {
		cv_mkts[i] = Qnil;
		cv_mkt_clients[i] = Qnil;
		c_mkts[i] = NULL;
	}

	c_arbitrage_min = NUM2DBL(rb_ivar_get(self, id_my_arbitrage_min));
	c_last_order_arbitrage_min = NUM2DBL(rb_ivar_get(self, id_my_last_order_arbitrage_min));

	VALUE my_deviation_map = rb_ivar_get(self, rb_intern("@deviation_map"));
	for (int i=0; i < my_markets_sz; i++) {
		VALUE v_price_ratio_range = rb_funcall(cv_mkt_clients[i], id_price_ratio_range, 1, my_pair);
		c_price_ratio_ranges[i][0] = NUM2DBL(rbary_el(v_price_ratio_range, 0));
		c_price_ratio_ranges[i][1] = NUM2DBL(rbary_el(v_price_ratio_range, 1));
		if (c_price_ratio_ranges[i][0] >= c_price_ratio_ranges[i][1]) {
			URN_WARNF("c_price_ratio_ranges %s %lf %lf", c_mkts[i],
				c_price_ratio_ranges[i][0], c_price_ratio_ranges[i][1]);
			rb_raise(rb_eTypeError, "price_ratio_range should be [min, max]");
		}
		c_p_steps[i] = NUM2DBL(rb_funcall(cv_mkt_clients[i],
			id_price_step, 1, my_pair));

		v_tmp = rbhsh_el(my_deviation_map, "maker/buy");
		c_deviation_map_mk_buy[i] = (RB_TYPE_P(v_tmp, T_NIL) == 1) ? 1 : NUM2DBL(v_tmp);

		v_tmp = rbhsh_el(my_deviation_map, "taker/buy");
		c_deviation_map_tk_buy[i] = (RB_TYPE_P(v_tmp, T_NIL) == 1) ? 1 : NUM2DBL(v_tmp);

		v_tmp = rbhsh_el(my_deviation_map, "maker/sell");
		c_deviation_map_mk_sel[i] = (RB_TYPE_P(v_tmp, T_NIL) == 1) ? 1 : NUM2DBL(v_tmp);

		v_tmp = rbhsh_el(my_deviation_map, "taker/sell");
		c_deviation_map_tk_sel[i] = (RB_TYPE_P(v_tmp, T_NIL) == 1) ? 1 : NUM2DBL(v_tmp);
	}

	const_urn = rb_const_get(rb_cObject, rb_intern("URN"));
	const_urn_mkt_bull = rb_const_get(const_urn, rb_intern("MKT_BULL"));
	const_urn_mkt_bear = rb_const_get(const_urn, rb_intern("MKT_BEAR"));
	URN_LOGF("cache_trader_attrs() done for trader_obj_id %ld", cache_trader_obj_id);
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
	VALUE  mkt_client_bid,
	VALUE  mkt_client_ask,
	VALUE  my_pair,
	double vol_max, double vol_min,
	struct order *buy_order, struct order *sell_order
) {
	VALUE str_buy = rb_str_new2("buy");
	VALUE str_sell = rb_str_new2("sell");

	double avg_price_max_diff = 0.001;
	// Scan bids and asks one by one, scan bids first.
	// [SEL/BUY][finished(0/1), idx, size_sum, price, p_real, avg_price]
	if ((*bids_map)[0][0] == 0 || (*asks_map)[0][0] == 0)
		return false;
	double scan_status[2][7] = {0};
	int _idx_fin=0, _idx_idx=1, _idx_size_sum=2, _idx_p=3, _idx_p_real=4, _idx_avg_p=5, _idx_p_take=6;

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
			URN_DEBUGF("%d side is already finished, "
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
			URN_DEBUGF("%d side is already finished, "
				"all %d data is scanned, all finished.",
				opposite_type, type);
			scan_status[type][_idx_fin] = 1;
			break;
		}

		// Scan other side if this side is marked as finished.
		// Scan other side if this orderbook is all checked.
		if ((scan_status[type][_idx_fin] == 1) ||
				((idx >= max_odbk_depth) || (p <= 0))) {
			URN_DEBUGF("-> flip to %d", opposite_type);
			scan_status[type][_idx_fin] = 1;
			scan_buy_now = !scan_buy_now;
			continue;
		}

		// URN_DEBUGF("\tODBK %d %d %lf %lf %lf", type, idx, p, s, p_take);
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
			URN_DEBUGF("Max vol reached, vol_max %lf", vol_max);
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
			URN_DEBUGF("Balance limited, %d max=%lf p=%lf", type, max_size, p);
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
			URN_DEBUGF("Scan %d orderbook %d: O p %lf p_tk %lf s %lf init this side.",
				type, idx, p, p_take, s);
			scan_buy_now = !scan_buy_now;
			continue;
		}
		URN_DEBUGF("Scan %d orderbook %d: O p %lf p_tk %lf s %lf", type, idx, p, p_take, s);

		// Compare this price with opposite_p
		double price_diff = scan_buy_now ? diff(p_take, opposite_p_take) : diff(opposite_p_take, p_take);
		if (price_diff <= min_price_diff) {
			URN_DEBUGF("\ttype:%d p:%lf,%lf opposite_p:%lf,%lf "
				"price diff:%lf < %lf side finished",
				type, p, p_take, opposite_p, opposite_p_take,
				price_diff, min_price_diff);
			scan_status[type][_idx_fin] = 1;
			continue;
		}

		// Compare future average price, should not deviate from price too much.
		if (size_sum + s == 0) continue;
		double next_avg_price = (avg_price*size_sum + p*s) / (size_sum + s);
		URN_DEBUGF("\tnext_avg: %8.8lf diff(next_avg, p): %8.8f",
			next_avg_price, URN_ABS(diff(next_avg_price, p)));
		if (URN_ABS(diff(next_avg_price, p)) > avg_price_max_diff) {
			URN_DEBUGF("\tdiff(next_avg, p).abs too large, this side finished");
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
		URN_DEBUGF("\tnow %d size_sum:%lf p:%lf avg_p:%lf", type, size_sum, p, next_avg_price);
		scan_status[type][_idx_idx] += 1;
		// Compare future size_sum with opposite_s, if this side is fewer, continue scan.
		if (size_sum < opposite_s) continue;
		scan_buy_now = !scan_buy_now;
	}

	// Return if no suitable orders.
	if ((scan_status[BUY][_idx_idx] == 0) || (scan_status[SEL][_idx_idx] == 0))
		return false;

	// Scanning finished, generate a pair of order with same size.
	buy_order->p  = scan_status[BUY][_idx_p];
	sell_order->p = scan_status[SEL][_idx_p];
	double max_bid_sz = NUM2DBL(rb_funcall(
		mkt_client_ask, id_max_order_size, 3,
		my_pair, str_buy, DBL2NUM(buy_order->p))
	);
	buy_order->s  = URN_MIN(scan_status[BUY][_idx_size_sum], max_bid_sz);
	sell_order->s = URN_MIN(scan_status[SEL][_idx_size_sum], max_ask_sz);
	double size = URN_MIN(buy_order->s, sell_order->s);
	buy_order->s  = size;
	sell_order->s = size;
	// o['p_real'] = scan_status[o['T']]['p_take']

	if (size < vol_min)
		return false;
	double min_bid_sz = rb_funcall(mkt_client_ask, id_min_order_size, 2,
		my_pair, DBL2NUM(buy_order->p));
	double min_ask_sz = rb_funcall(mkt_client_bid, id_min_order_size, 2,
		my_pair, DBL2NUM(sell_order->p));
	if ((size < min_bid_sz) || (size < min_ask_sz))
		return false;

	return true;
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
	URN_DEBUGF("detect_arbitrage_pattern_c");
	struct timeval start_t = urn_global_tv;

	VALUE my_vol_max = rb_ivar_get(self, id_my_vol_max);
	VALUE my_avg_last = rb_ivar_get(self, id_my_avg_last);

	double single_vol_ratio = NUM2DBL(rb_ivar_get(self, id_my_single_vol_ratio));
	double vol_min = NUM2DBL(rb_ivar_get(self, id_my_vol_min));
	double vol_max_a = NUM2DBL(rbhsh_el(my_vol_max, "A"));
	double vol_max_b = NUM2DBL(rbhsh_el(my_vol_max, "B"));
	double vol_max_c = NUM2DBL(rbhsh_el(my_vol_max, "C"));
	double avg_last = (RB_TYPE_P(my_avg_last, T_NIL) == 1) ? -1 : NUM2DBL(my_avg_last);

	long idx, max; // for rbary_each()
	VALUE v_el, v_tmp; // for tmp usage.

	// Mark c_valid_mkts for this round.
	VALUE v_valid_mkts = rb_funcall(my_market_data_agt, id_valid_markets, 1, my_markets);
	rbary_each(v_valid_mkts, idx, max, v_el) {
		char *m = RSTRING_PTR(v_el);
		for (int i=0; i < my_markets_sz; i++) {
			c_valid_mkts[i] = false;
			if (strcmp(m, c_mkts[i]) == 0) {
				c_valid_mkts[i] = true;
				break;
			}
		}
	}
	int valid_mkts_sz = max;
	URN_DEBUGF("valid markets %d/%d", valid_mkts_sz, my_markets_sz);

	if (valid_mkts_sz < 2) {
		// @markets.each { |m| @_keep_exist_spike_order[m] = true }
		for (int i=0; i < MAX_AB3_MKTS; i++) {
			rb_hash_aset(my_keep_exist_spike_order, cv_mkts[i], Qtrue);
		}
		return rb_hash_new();
	}

	double reserved_bal[MAX_AB3_MKTS][2] = {0};
	/*
	 * STEP 1 organize own orders
	 */
	// linked order instead of ruby array
	struct order *own_main_orders[MAX_AB3_MKTS][2] = {NULL};
	struct order *own_spike_main_orders[MAX_AB3_MKTS][2] = {NULL};
	struct order *own_live_orders[MAX_AB3_MKTS][2] = {NULL};
	struct order *own_legacy_orders[MAX_AB3_MKTS][2] = {NULL};

	VALUE v_orders = rb_hash_aref(v_opt, sym_own_main_orders);
	if (RB_TYPE_P(v_orders, T_NIL) != 1) {
		rbary_each(v_orders, idx, max, v_el) {
			VALUE v_bool = rb_funcall(self, id_order_alive_qm, 1, v_el);
			if (RB_TYPE_P(v_bool, T_TRUE) != 1)
				continue;
			int m_idx = index_of(my_markets_sz, c_mkts, RSTRING_PTR(rbhsh_el(v_el, "market")));
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
			} else {
				o->next = own_main_orders[m_idx][SEL];
				own_main_orders[m_idx][SEL] = o;
				// reserved_bal[m][t] += (o['p']*o['remained'])
				reserved_bal[m_idx][BUY] += ((o->p) * (o->remained));
			}
		}
	}
	v_orders = rb_hash_aref(v_opt, sym_own_spike_main_orders);
	if (RB_TYPE_P(v_orders, T_NIL) != 1) {
		rbary_each(v_orders, idx, max, v_el) {
			VALUE v_bool = rb_funcall(self, id_order_alive_qm, 1, v_el);
			if (RB_TYPE_P(v_bool, T_TRUE) != 1)
				continue;
			int m_idx = index_of(my_markets_sz, c_mkts, RSTRING_PTR(rbhsh_el(v_el, "market")));
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
	if (RB_TYPE_P(v_orders, T_NIL) != 1) {
		rbary_each(v_orders, idx, max, v_el) {
			VALUE v_bool = rb_funcall(self, id_order_alive_qm, 1, v_el);
			if (RB_TYPE_P(v_bool, T_TRUE) != 1)
				continue;
			int m_idx = index_of(my_markets_sz, c_mkts, RSTRING_PTR(rbhsh_el(v_el, "market")));
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
	if (RB_TYPE_P(v_orders, T_NIL) != 1) {
		rbary_each(v_orders, idx, max, v_el) {
			VALUE v_bool = rb_funcall(self, id_order_alive_qm, 1, v_el);
			if (RB_TYPE_P(v_bool, T_TRUE) != 1)
				continue;
			int m_idx = index_of(my_markets_sz, c_mkts, RSTRING_PTR(rbhsh_el(v_el, "market")));
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

	/*
	 * STEP 2 prepare market data: bids_map and asks_map
	 * RUBY: bids_map = mkts.map { |m| [m, @market_snapshot.dig(m, :orderbook, 0) || []] }.to_h
	 * C:    bids_map[m_idx] => [[p, s, p_take], [3], ... ], p<=0 means invalid data.
	 */
	double bids_map[MAX_AB3_MKTS][max_odbk_depth][3] = {0};
	double asks_map[MAX_AB3_MKTS][max_odbk_depth][3] = {0};
	for (int i=0; i < my_markets_sz; i++) {
		if (c_valid_mkts[i] != true) continue;
		v_tmp = rb_funcall(my_market_snapshot, id_dig, 3, cv_mkts[i], sym_orderbook, INT2NUM(0));
		if (RB_TYPE_P(v_tmp, T_NIL) != 1) { // copy bids_map
			rbary_each(v_tmp, idx, max, v_el) {
				if (idx >= max_odbk_depth) break;
				bids_map[i][idx][0] = NUM2DBL(rbhsh_el(v_el, "p"));
				bids_map[i][idx][1] = NUM2DBL(rbhsh_el(v_el, "s"));
				bids_map[i][idx][2] = NUM2DBL(rbhsh_el(v_el, "p_take"));
			}
		}
		v_tmp = rb_funcall(my_market_snapshot, id_dig, 3, cv_mkts[i], sym_orderbook, INT2NUM(1));
		if (RB_TYPE_P(v_tmp, T_NIL) != 1) { // copy asks_map
			rbary_each(v_tmp, idx, max, v_el) {
				if (idx >= max_odbk_depth) break;
				asks_map[i][idx][0] = NUM2DBL(rbhsh_el(v_el, "p"));
				asks_map[i][idx][1] = NUM2DBL(rbhsh_el(v_el, "s"));
				asks_map[i][idx][2] = NUM2DBL(rbhsh_el(v_el, "p_take"));
			}
		}
		URN_DEBUGF("mkts %d/%d %8s bid0 p %4.6lf p_tk %4.6lf s %8lf ask0 p %4.6lf p_tk %4.6lf s %8lf",
			i, my_markets_sz, c_mkts[i],
			bids_map[i][0][0], bids_map[i][0][2], bids_map[i][0][1],
			asks_map[i][0][0], asks_map[i][0][2], asks_map[i][0][1]);
	}

	VALUE v_chances = rb_ary_new2(2);
	VALUE str_buy = rb_str_new2("buy");
	VALUE str_sell = rb_str_new2("sell");
	// Detection for case A => "[A] Direct sell and buy",
	for (int m1_idx=0; m1_idx < my_markets_sz; m1_idx++) {
		// check m1 bids and m2 asks, sell at m1 and buy at m2
		if (!c_run_mode_a) break;
		if (c_valid_mkts[m1_idx] != true) continue;
		if (bids_map[m1_idx][0][0] <= 0) continue;
		for (int m2_idx=0; m2_idx < my_markets_sz; m2_idx++) {
			if (c_valid_mkts[m2_idx] != true) continue;
			if (m1_idx == m2_idx) continue;
			if (asks_map[m2_idx][0][0] <= 0) continue;
			struct order buy_order, sell_order;
			bool found = _aggressive_arbitrage_orders_c(
				&bids_map[m1_idx], &asks_map[m2_idx],
				c_arbitrage_min,
				cv_mkt_clients[m1_idx], cv_mkt_clients[m2_idx],
				my_pair,
				URN_MAX(single_vol_ratio*vol_max_a, vol_min*1.1), vol_min,
				&buy_order, &sell_order
			);
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
			VALUE v_os = rb_hash_new();
			rb_hash_aset(v_os, rb_str_new2("market"), cv_mkts[m1_idx]);
			rb_hash_aset(v_os, rb_str_new2("p"), DBL2NUM(sell_order.p));
			rb_hash_aset(v_os, rb_str_new2("s"), DBL2NUM(sell_order.s));
			rb_hash_aset(v_os, rb_str_new2("T"), rb_str_new2("sell"));
			VALUE v_ob = rb_hash_new();
			rb_hash_aset(v_ob, rb_str_new2("market"), cv_mkts[m2_idx]);
			rb_hash_aset(v_ob, rb_str_new2("p"), DBL2NUM(buy_order.p));
			rb_hash_aset(v_ob, rb_str_new2("s"), DBL2NUM(buy_order.s));
			rb_hash_aset(v_ob, rb_str_new2("T"), rb_str_new2("buy"));

			VALUE v_orders = rb_ary_new2(2);
			rb_ary_push(v_orders, v_os);
			rb_ary_push(v_orders, v_ob);
			VALUE v_clients = rb_ary_new2(2);
			rb_ary_push(v_clients, cv_mkt_clients[m1_idx]); // for sell
			rb_ary_push(v_clients, cv_mkt_clients[m2_idx]); // for buy

			// equalize_order_size(orders, clients) # might break min_order_size
			rb_funcall(self, id_equalize_order_size, 2, v_orders, v_clients);
			// orders.each { |o| downgrade_order_size_for_all_market(o, all_clients) }
			rb_funcall(self, id_downgrade_order_size_for_all_market, 2, v_os, v_clients);
			rb_funcall(self, id_downgrade_order_size_for_all_market, 2, v_ob, v_clients);
			URN_LOGF("After equalize_order_size() and downgrade_order_size_for_all_market()"
				"BUY  p %8.8f s %8.8f %s \n"
				"SELL p %8.8f s %8.8f %s \n",
				NUM2DBL(rbhsh_el(v_ob, "p")), NUM2DBL(rbhsh_el(v_ob, "s")), buy_order.m,
				NUM2DBL(rbhsh_el(v_os, "p")), NUM2DBL(rbhsh_el(v_os, "s")), sell_order.m);

			// equalize_order_size() might break min_order_size
			double min_bid_sz = NUM2DBL(rb_funcall(cv_mkt_clients[m2_idx], id_min_order_size, 1, v_ob));
			double min_ask_sz = NUM2DBL(rb_funcall(cv_mkt_clients[m1_idx], id_min_order_size, 1, v_os));
			if (NUM2DBL(rbhsh_el(v_os, "s")) < min_ask_sz) {
				URN_LOGF("\t Sell order break min_order_size %lf", min_ask_sz);
				continue;
			} else if (NUM2DBL(rbhsh_el(v_ob, "s")) < min_bid_sz) {
				URN_LOGF("\t Buy  order break min_order_size %lf", min_bid_sz);
				continue;
			}

			// Huobi OKEX allows old and new price precision in same orderbook.
			// TODO

			// Build up results.
			VALUE mo = v_os;
			VALUE type12_result = rb_hash_new();
			VALUE v_p_real = rb_funcall(self, id_price_real, 3, mo, Qnil, rb_str_new2("taker"));
			double p_real = NUM2DBL(v_p_real);
			rb_hash_aset(type12_result, sym_p_real, v_p_real);
			rb_hash_aset(type12_result, sym_price, rbhsh_el(mo, "p"));
			rb_hash_aset(type12_result, sym_type, rbhsh_el(mo, "T"));
			rb_hash_aset(type12_result, sym_market, rbhsh_el(mo, "market"));
			rb_hash_aset(type12_result, sym_child_type, rb_str_new2("buy"));
			// main type is always sell
			double child_price_threshold = p_real/(1+c_last_order_arbitrage_min);
			rb_hash_aset(type12_result, sym_child_price_threshold, DBL2NUM(child_price_threshold));
			rb_hash_aset(type12_result, sym_cata, rb_str_new2("A"));
			rb_hash_aset(type12_result, sym_suggest_size, rbhsh_el(mo, "s"));
			char explain[256];
			sprintf(explain, "Direct sell %8.8f at %s and buy at %s",
				NUM2DBL(rbhsh_el(mo, "s")), c_mkts[m1_idx], c_mkts[m2_idx]);
			URN_LOGF("A chance %s", explain);
			rb_hash_aset(type12_result, sym_explain, rb_str_new2(explain));
			rb_ary_push(v_chances, type12_result);
		}
	}

	// Detection for case waiting Spike buy/sell
	// If market price higher than lowest_bid_p a lot, dont place spike buy order
	// If market price lower than highest_ask_p a lot, dont place spike sell order
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
	URN_DEBUGF("highest_ask_p %lf lowest_bid_p %lf", highest_ask_p, lowest_bid_p);
	if ((RB_TYPE_P(my_spike_catcher_enabled, T_TRUE) == 1) && (c_run_mode_d || c_run_mode_e)) {
		for (int m1_idx=0; m1_idx < my_markets_sz; m1_idx++) {
			if (c_valid_mkts[m1_idx] != true) continue;
			double bid_p = bids_map[m1_idx][0][0];
			double ask_p = asks_map[m1_idx][0][0];
			if ((bid_p <= 0) || (ask_p <= 0)) {
				URN_LOG("market orderbook invalid, do not cancel spike orders.");
				// Should be temporary data incomplete
				rb_hash_aset(my_keep_exist_spike_order, cv_mkts[m1_idx], Qtrue);
				continue;
			}
			rb_hash_aset(my_keep_exist_spike_order, cv_mkts[m1_idx], Qfalse);
			URN_DEBUGF("Spike %s BID %lf %lf ASK %lf %lf", c_mkts[m1_idx],
				bid_p, bids_map[m1_idx][0][2],
				ask_p, asks_map[m1_idx][0][2]);
			// Price should not be far away from @avg_last
			// bid_p is higher than avg_last, very abonormal
			// ask_p is lower than avg_last, very abonormal
			if ((avg_last > 0) && (URN_ABS(diff(bid_p, avg_last)) > 0.01)) {
				URN_LOGF_C(RED, "%s price B %lf A %lf is too far away from "
					"@avg_last %lf, skip spike catching",
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
			URN_DEBUGF("URN::MKT_BULL %d %d URN::MKT_BEAR %d %d",
				RB_TYPE_P(const_urn_mkt_bull, T_TRUE),
				RB_TYPE_P(const_urn_mkt_bull, T_FALSE),
				RB_TYPE_P(const_urn_mkt_bear, T_TRUE),
				RB_TYPE_P(const_urn_mkt_bear, T_FALSE));
			bool m1_bittrex = (strcmp(c_mkts[m1_idx], "Bittrex") == 0) ? true : false;
			if (m1_bittrex)
				memcpy(spike_buy_prices, spike_buy_prices_bittrex, sizeof(spike_buy_prices));
			double p_threshold = 1;
			if (RB_TYPE_P(const_urn_mkt_bear, T_TRUE) == 1)
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
			if (c_deviation_map_mk_buy[m1_idx] >= 0.03) spike_bid = false;
			if (c_deviation_map_tk_buy[m1_idx] >= 0.03) spike_bid = false;
			for (int i=0; i < MAX_SPIKES; i++) {
				if (!spike_bid) break;
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
			if (m1_bittrex && c_my_pair[0] == 'B' && c_my_pair[0] == 'T'
					&& c_my_pair[0] == 'C' && c_my_pair[0] == '-') {
				// For buy orders, place X*@vol_max_const when balance >= (3+2X) * @vol_max_const
				buy_o_quota = ((first_order_size_avail / c_vol_max_const - 3) / 2) - 0.1;
			} else if (m1_bittrex && c_my_pair[0] == 'E' && c_my_pair[0] == 'T'
					&& c_my_pair[0] == 'H' && c_my_pair[0] == '-') {
				// For buy orders, place X*@vol_max_const when balance >= (3+4X) * @vol_max_const
				buy_o_quota = ((first_order_size_avail / c_vol_max_const - 3) / 4) - 0.1;
			} else {
				// For buy orders, place X*@vol_max_const when balance >= (15+10X) * @vol_max_const
				buy_o_quota = ((first_order_size_avail / c_vol_max_const - 15) / 10.0) - 0.1;
			}
			URN_DEBUGF("spike_buy first_order_size_avail %s %lf %lf",
				c_mkts[m1_idx], first_order_size_avail, buy_o_quota);
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
		}
	}

	URN_DEBUGF("detect_arbitrage_pattern_c end");
	struct timeval end_t = urn_global_tv;
	long cost_t_e6 = (end_t.tv_sec - start_t.tv_sec)*1000000 + (end_t.tv_usec-start_t.tv_usec);
	URN_DEBUGF("detect_arbitrage_pattern_c cost usec %ld", cost_t_e6);
	return Qnil;
}
