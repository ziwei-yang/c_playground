#ifndef ORDER_UTIL_H
#define ORDER_UTIL_H

#include "order.h"
#include <ruby.h>
#include <stdbool.h>

// Pure C Methods
extern bool is_future(const char* pair);
extern int parse_contract(const char* contract_str, char* asset1, char* asset2, char* expiry);
extern int diff(double f1, double f2);
extern double rough_num(double f);
extern void stat_array(double* array, int n, double* stat_result);
extern void short_order_status(const char* status, char* short_status);
extern void trim(char* str, char* new_s);
extern void format_trade_time(unsigned long t, char* s);
extern void format_trade(Order* o, char* str);
extern void order_to_s(Order* o, char* str);
extern void order_set_dead(Order* o);
extern void order_status_evaluate(Order *o);
extern long order_age(Order* o);
extern bool order_full_filled(Order* t, double omit_size);
extern bool order_same(Order* o1, Order* o2);
extern bool order_changed(Order* o1, Order* o2);
extern bool order_should_update(Order* o1, Order* o2);
extern int order_stat(Order* orders, int precise, double* result);
extern double order_real_vol(Order* o);
extern bool order_same_mkt_pair(Order* orders[]);

/////////////// RUBY interface below ///////////////
extern void order_util_bind(VALUE urn_core_module);

#endif // ORDER_UTIL_H

