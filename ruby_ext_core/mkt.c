#include "ruby.h"

/*
 * util.c
 * Replacement of bootstrap.rb/MathUtil
 */
VALUE rb_diff(VALUE self, VALUE v_f1, VALUE v_f2);
VALUE rb_stat_array(VALUE self, VALUE v_ary);
VALUE rb_rough_num(VALUE self, VALUE v_f);
VALUE rb_format_num(int argc, VALUE *argv, VALUE klass);

void order_bind(VALUE urn_core_module);      // order.c

void order_util_bind(VALUE urn_core_module); // order_util.c

void ab3_init();
VALUE method_detect_arbitrage_pattern(VALUE self, VALUE v_opt);

void Init_urn_core() {
	ab3_init();

	VALUE urn_core_module = rb_define_module("URN_CORE");

	VALUE cMathUtil = rb_define_module_under(urn_core_module, "MathUtil");
	rb_define_method(cMathUtil, "diff", rb_diff, 2);
	rb_define_method(cMathUtil, "stat_array", rb_stat_array, 1);
	rb_define_method(cMathUtil, "rough_num", rb_rough_num, 1);
	rb_define_method(cMathUtil, "format_num", rb_format_num, -1);
	rb_define_method(cMathUtil, "detect_arbitrage_pattern_c", method_detect_arbitrage_pattern, 1);

	order_bind(urn_core_module);
	order_util_bind(urn_core_module);
}
