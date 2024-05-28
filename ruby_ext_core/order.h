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
    unsigned long t;    // in milliseconds
    unsigned long t_ns; // in micro seconds
    double v; // only valid for volume based order
    double executed_v; // only valid for volume based order
    double remained_v; // only valid for volume based order
    // 104 bytes

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

    char _desc[491]; // for format_trade(o)
} Order; // 1024 bytes

#define ORDER_DESC(o) \
	snprintf(o->_desc, sizeof(o->_desc), \
			"Order: %s, %s, %s, %s-%s, %s, T=%s, market=%s, " \
			"p=%f, s= %f / %f, remained=%f, avg_price=%f, fee=%f, maker_size=%f, " \
			"t=%lu, _buy=%d, _status_cached=%d, _alive=%d, _cancelled=%d, " \
			"p_real=%f, v=%f, executed_v=%f, remained_v=%f, " \
			"fee mb=%f, tb=%f, ms=%f, ts=%f, " \
			"dv mb=%f, tb=%f, ms=%f, ts=%f", \
			o->i, o->client_oid, o->pair, o->asset, o->currency, o->status, o->T, o->market, \
			o->p, o->executed, o->s, o->remained, o->avg_price, o->fee, o->maker_size, \
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

