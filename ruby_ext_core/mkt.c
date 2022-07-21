#include "ruby.h"

static VALUE URN_CORE = Qnil;
static VALUE URN_CORE_Order = Qnil;

/* in util.c
 * Replacement of:
 * bootstrap.rb/MathUtil
 */
VALUE method_diff(VALUE self, VALUE v_f1, VALUE v_f2);
VALUE method_stat_array(VALUE self, VALUE v_ary);
VALUE method_rough_num(VALUE self, VALUE v_f);
VALUE method_format_num(int argc, VALUE *argv, VALUE klass);
void ab3_init();
VALUE method_detect_arbitrage_pattern(VALUE self, VALUE v_opt);

typedef struct Order {
	char i[64];
} Order;

VALUE order_new(VALUE self) {
	Order *o = ALLOC(Order);
	strcpy(o->i, "123456");
	VALUE _data = Data_Wrap_Struct(URN_CORE_Order, NULL, xfree, o);
	rb_ivar_set(self, rb_intern("@_data"), _data);
	return self;
}

VALUE order_hashget(VALUE self, VALUE v_key) {
	Order *o = NULL;
	char *s = RSTRING_PTR(v_key);
	VALUE _data = rb_ivar_get(self, rb_intern("@_data"));
	Data_Get_Struct(_data, Order, o);
	return rb_str_new_cstr(o->i);
}

void Init_urn_core() {
	ab3_init();
	URN_CORE = rb_define_module("URN_CORE");
	rb_define_method(URN_CORE, "diff", method_diff, 2);
	rb_define_method(URN_CORE, "stat_array", method_stat_array, 1);
	rb_define_method(URN_CORE, "rough_num", method_rough_num, 1);
	rb_define_method(URN_CORE, "format_num", method_format_num, -1);
	rb_define_method(URN_CORE, "detect_arbitrage_pattern_c", method_detect_arbitrage_pattern, 1);

	URN_CORE_Order = rb_define_class_under(URN_CORE, "Order", rb_cObject);
	rb_define_method(URN_CORE_Order, "initialize", order_new, 0);
	rb_define_method(URN_CORE_Order, "[]", order_hashget, 1);
}
