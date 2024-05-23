#include "ruby.h"

VALUE rb_is_future(VALUE self, VALUE v_pair) {
	char *s = RSTRING_PTR(v_pair);
	if (strstr(s, "@") == NULL)
		return Qfalse;
	return Qtrue;
}

void order_util_bind(VALUE urn_core_module) {
	rb_define_method(urn_core_module, "is_future?", rb_is_future, 1);
}
