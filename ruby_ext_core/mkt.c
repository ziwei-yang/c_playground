#include "ruby.h"

static VALUE URN_CORE = Qnil;

/*
 * util.c
 * Replacement of bootstrap.rb/MathUtil
 */
VALUE method_diff(VALUE self, VALUE v_f1, VALUE v_f2);
VALUE method_stat_array(VALUE self, VALUE v_ary);
VALUE method_rough_num(VALUE self, VALUE v_f);
VALUE method_format_num(int argc, VALUE *argv, VALUE klass);

void order_bind(VALUE urn_core_module);      // order.c

void order_util_bind(VALUE urn_core_module); // order_util.c

void ab3_init();
VALUE method_detect_arbitrage_pattern(VALUE self, VALUE v_opt);

void Init_urn_core() {
	ab3_init();

	URN_CORE = rb_define_module("URN_CORE");
	rb_define_method(URN_CORE, "diff", method_diff, 2);
	rb_define_method(URN_CORE, "stat_array", method_stat_array, 1);
	rb_define_method(URN_CORE, "rough_num", method_rough_num, 1);
	rb_define_method(URN_CORE, "format_num", method_format_num, -1);
	rb_define_method(URN_CORE, "detect_arbitrage_pattern_c", method_detect_arbitrage_pattern, 1);

	order_bind(URN_CORE);
	order_util_bind(URN_CORE);
}
