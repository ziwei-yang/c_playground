#ifndef ORDER_UTIL_H
#define ORDER_UTIL_H

#include "order.h"
#include <ruby.h>
#include <stdbool.h>

// Macros
#define extract_ruby_order(v_order, opvar) \
        Order _temp_o;memset(&_temp_o, 0, sizeof(Order));\
        Order* opvar = _attach_or_parse_ruby_order(v_order, &_temp_o);

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

// Ruby C Extension Methods
extern VALUE rb_order_canceling(VALUE self, VALUE v_order);
extern VALUE rb_order_pending(VALUE self, VALUE v_order);
extern VALUE rb_format_trade(int argc, VALUE* argv, VALUE self);
extern VALUE rb_order_cancelled(VALUE self, VALUE v_order);
extern VALUE rb_order_active(VALUE self, VALUE v_order);
extern VALUE rb_order_alive(VALUE self, VALUE v_order);
extern VALUE rb_order_completed(VALUE self, VALUE v_order);
extern VALUE rb_order_to_s(VALUE self, VALUE v_order);
extern VALUE rb_order_set_dead(VALUE self, VALUE v_order);

// Function to initialize the module and register methods
extern void order_util_bind(VALUE urn_core_module);

#endif // ORDER_UTIL_H

