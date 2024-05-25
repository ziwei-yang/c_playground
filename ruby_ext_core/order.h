#ifndef ORDER_H
#define ORDER_H

#include <stdbool.h>

// Define the struct Order
typedef struct Order {
    char i[128];
    char client_oid[128];
    char account[64]; // reserved field
    char pair[32];
    char asset[16];
    char currency[16];
    char status[16];
    char T[8];
    char market[16];

    // Negative value means not initialised
    double p;
    double s;
    double remained;
    double executed;
    double avg_price;
    double fee;
    double maker_size;
    double p_real;
    double v; // only valid for volume based order
    double executed_v; // only valid for volume based order
    double remained_v; // only valid for volume based order
    unsigned long t;

    bool _status_cached;
    bool _buy;
    bool _alive;
    bool _cancelled;
} Order;

#define ORDER_DEBUG(o) \
    URN_DEBUGF("Order: i=%s, client_oid=%s, pair=%s, asset=%s, currency=%s, status=%s, T=%s, market=%s, p=%f, s=%f, v=%f, remained=%f, remained_v=%f, executed=%f, executed_v=%f, avg_price=%f, fee=%f, maker_size=%f, p_real=%f, t=%lu, _buy=%d, _status_cached=%d, _alive=%d, _cancelled=%d", \
        (o)->i, (o)->client_oid, (o)->pair, (o)->asset, (o)->currency, (o)->status, (o)->T, (o)->market, \
        (o)->p, (o)->s, (o)->v, (o)->remained, (o)->remained_v, (o)->executed, (o)->executed_v, (o)->avg_price, \
	(o)->fee, (o)->maker_size, (o)->p_real, (o)->t, (o)->_buy, (o)->_status_cached, (o)->_alive, (o)->_cancelled)

#define ORDER_LOG(o) \
    URN_LOGF("Order: i=%s, client_oid=%s, pair=%s, asset=%s, currency=%s, status=%s, T=%s, market=%s, p=%f, s=%f, v=%f, remained=%f, remained_v=%f, executed=%f, executed_v=%f, avg_price=%f, fee=%f, maker_size=%f, p_real=%f, t=%lu, _buy=%d, _status_cached=%d, _alive=%d, _cancelled=%d", \
        (o)->i, (o)->client_oid, (o)->pair, (o)->asset, (o)->currency, (o)->status, (o)->T, (o)->market, \
        (o)->p, (o)->s, (o)->v, (o)->remained, (o)->remained_v, (o)->executed, (o)->executed_v, (o)->avg_price, \
	(o)->fee, (o)->maker_size, (o)->p_real, (o)->t, (o)->_buy, (o)->_status_cached, (o)->_alive, (o)->_cancelled)


#define INIT_ORDER(o) do { \
    memset((o), 0, sizeof(Order)); \
    (o)->p = -99999; \
    (o)->s = -99999; \
    (o)->remained = -99999; \
    (o)->executed = -99999; \
    (o)->avg_price = -99999; \
    (o)->fee = -99999; \
    (o)->maker_size = -99999; \
    (o)->p_real = -99999; \
    (o)->v = -99999; \
    (o)->executed_v = -99999; \
    (o)->remained_v = -99999; \
} while (0)

extern void order_from_hash(VALUE hash, Order* o);

extern void order_to_hash(VALUE hash, Order* o);
extern VALUE rb_order_to_hash(VALUE self);
extern void order_bind(VALUE urn_core_module);

#endif // ORDER_H

