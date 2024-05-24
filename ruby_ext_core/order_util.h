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

// Function to initialize the module and register methods
extern void order_util_bind(VALUE urn_core_module);

#endif // ORDER_UTIL_H

