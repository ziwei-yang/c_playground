#ifndef ORDER_H
#define ORDER_H

#include <stdbool.h>

// Define the struct Order
typedef struct Order {
    char i[128];
    char client_oid[128];
    char pair[32];
    char asset[16];
    char currency[16];
    char status[16];
    char T[8];
    char market[16];
    // 360 bytes

    // Negative value means not initialised
    double p;
    double s;
    double remained;
    double executed;
    double avg_price;
    double fee;
    double maker_size;
    double p_real;
    unsigned long t;
    double v; // only valid for volume based order
    double executed_v; // only valid for volume based order
    double remained_v; // only valid for volume based order
    // 96 bytes

    // fee rates
    double fee_maker_buy;
    double fee_taker_buy;
    double fee_maker_sell;
    double fee_taker_sell;
    // 32 bytes

    // deviation ratio
    double dv_maker_buy;
    double dv_taker_buy;
    double dv_maker_sell;
    double dv_taker_sell;
    // 32 bytes

    bool _buy;  // true if T is set 'buy'
    bool _sell; // true if T is set 'sell'
    // Cache fields are valid only when _status_cached is true
    bool _status_cached;
    bool _alive;
    bool _cancelled;
    // 5 bytes

    char padding[243];
    char _desc[256]; // for format_trade(o)
} Order; // 1024 bytes

#define ORDER_DESC(o) \
	snprintf(o->_desc, sizeof(o->_desc), \
			"Order: i=%s, client_oid=%s, pair=%s, asset=%s, currency=%s, status=%s, T=%s, market=%s, " \
			"p=%.6f, s=%.6f, remained=%.6f, executed=%.6f, avg_price=%.6f, fee=%.6f, maker_size=%.6f, " \
			"t=%lu, _buy=%d, _status_cached=%d, _alive=%d, _cancelled=%d, " \
			"p_real=%.6f, v=%.6f, executed_v=%.6f, remained_v=%.6f, " \
			"fee_maker_buy=%.6f, fee_taker_buy=%.6f, fee_maker_sell=%.6f, fee_taker_sell=%.6f, " \
			"dv_maker_buy=%.6f, dv_taker_buy=%.6f, dv_maker_sell=%.6f, dv_taker_sell=%.6f", \
			o->i, o->client_oid, o->pair, o->asset, o->currency, o->status, o->T, o->market, \
			o->p, o->s, o->remained, o->executed, o->avg_price, o->fee, o->maker_size, \
			o->t, o->_buy, o->_status_cached, o->_alive, o->_cancelled, \
			o->p_real, o->v, o->executed_v, o->remained_v, \
			o->fee_maker_buy, o->fee_taker_buy, o->fee_maker_sell, o->fee_taker_sell, \
			o->dv_maker_buy, o->dv_taker_buy, o->dv_maker_sell, o->dv_taker_sell); \

#define ORDER_DEBUG(o) \
	ORDER_DESC(o); URN_DEBUG(o->_desc)

#define ORDER_LOG(o) \
	ORDER_DESC(o); URN_LOG(o->_desc)

#define INIT_ORDER(o) do { \
	memset((o), 0, sizeof(Order)); \
	(o)->p = -99999.0; \
	(o)->s = -99999.0; \
	(o)->remained = -99999.0; \
	(o)->executed = -99999.0; \
	(o)->avg_price = -99999.0; \
	(o)->fee = -99999.0; \
	(o)->maker_size = -99999.0; \
	(o)->p_real = -99999.0; \
	(o)->v = -99999.0; \
	(o)->executed_v = -99999.0; \
	(o)->remained_v = -99999.0; \
	(o)->fee_maker_buy = -99999.0; \
	(o)->fee_taker_buy = -99999.0; \
	(o)->fee_maker_sell = -99999.0; \
	(o)->fee_taker_sell = -99999.0; \
	(o)->dv_maker_buy = -99999.0; \
	(o)->dv_taker_buy = -99999.0; \
	(o)->dv_maker_sell = -99999.0; \
	(o)->dv_taker_sell = -99999.0; \
} while (0)

extern void order_from_hash(VALUE hash, Order* o);

extern void order_to_hash(VALUE hash, Order* o);
extern VALUE rb_order_to_hash(VALUE self);
extern void order_bind(VALUE urn_core_module);

#endif // ORDER_H

