#include "ruby.h"

#include "../urn.h"

/*
 * Replacement of:
 * bootstrap.rb/
 * 	def format_num(f, float=8, decimal=8)
 */
VALUE method_format_num(int argc, VALUE *argv, VALUE klass) {
	VALUE v_f, v_fraclen, v_decilen;
	// 1 required arg, 2 optional args
	rb_scan_args(argc, argv, "12", &v_f, &v_fraclen, &v_decilen);
	int fraclen = 0, decilen = 0;
	if (RB_TYPE_P(v_fraclen, T_FIXNUM) != 1)
		rb_raise(rb_eTypeError, "invalid type for format_num() fraclen");
	if (RB_TYPE_P(v_decilen, T_FIXNUM) != 1)
		rb_raise(rb_eTypeError, "invalid type for format_num() decilen");
	fraclen = NUM2INT(v_fraclen);
	decilen = NUM2INT(v_decilen);

	/* INIT string as all spaces */
	char *str = malloc(fraclen+decilen+2);
	memset(str, ' ', fraclen+decilen+2);
	str[fraclen+decilen+1] = '\0';

	int fint = 0;
	double frac = 0;
	if (RB_TYPE_P(v_f, T_NIL) == 1) {
		return rb_str_new_cstr(str);
	} else if (RB_TYPE_P(v_f, T_STRING) == 1) {
		// return rjust(f)
		char *s = RSTRING_PTR(v_f);
		int pos = strlen(str) - strlen(s);
		if (pos >= 0)
			strcpy(str+pos, s);
		else
			strcpy(str, s-pos);
		return rb_str_new_cstr(str);
	} if (RB_TYPE_P(v_f, T_FIXNUM) == 1) {
		fint = NUM2INT(v_f);
		if (fint == 0)
			return rb_str_new_cstr(str);
	} else if (RB_TYPE_P(v_f, T_FLOAT) == 1) {
		frac = NUM2DBL(v_f);
		fint = (int)frac;
		frac = frac - fint;
	} else
		rb_raise(rb_eTypeError, "invalid type for format_num() f");

	URN_DEBUGF("%d (%d) + %f (%d)", fint, decilen, frac, fraclen);
	int sz = sprintf(str, "%*d", decilen, fint);
	if (frac == 0) {
		for(int i = sz; i <fraclen + 1 + decilen; i++)
			str[i] = ' ';
		str[fraclen+decilen+1] = '\0';
		return rb_str_new_cstr(str);
	}

	int remained_sz = fraclen + 1 + decilen - sz - 1;
	sprintf(str+sz-1, "%0.*f", remained_sz, frac);
	/* change tailing zero to space */
	for(int i = fraclen+decilen; i > sz; i--)
		if (*(str+i) == '0')
			*(str+i) = ' ';
		else
			break;
	/* print integer part again to overwirte '0.', change '\0' to '.' */
	sz = sprintf(str, "%*d", decilen, fint);
	str[sz] = '.';
	return rb_str_new_cstr(str);
}

VALUE URN_CORE = Qnil;

void Init_urn_core() {
	URN_CORE = rb_define_module("URN_CORE");
	rb_define_method(URN_CORE, "format_num", method_format_num, -1);
}
