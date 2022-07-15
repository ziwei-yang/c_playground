#include "ruby.h"
#include "math.h"

#include "../urn.h"

/* Replacement of:
 * bootstrap.rb/MathUtil
 * 	def diff(f1, f2)
 */
VALUE method_diff(VALUE self, VALUE v_f1, VALUE v_f2) {
	// return 9999999 if [f1, f2].min <= 0
	// (f1 - f2) / [f1, f2].min
	double f1, f2;
	if (RB_TYPE_P(v_f1, T_FIXNUM) == 1) {
		f1 = (double)NUM2INT(v_f1);
	} else if (RB_TYPE_P(v_f1, T_FLOAT) == 1) {
		f1 = NUM2DBL(v_f1);
	} else
		rb_raise(rb_eTypeError, "invalid type for diff() f1");
	if (RB_TYPE_P(v_f2, T_FIXNUM) == 1) {
		f2 = (double)NUM2INT(v_f2);
	} else if (RB_TYPE_P(v_f2, T_FLOAT) == 1) {
		f2 = NUM2DBL(v_f2);
	} else
		rb_raise(rb_eTypeError, "invalid type for diff() f2");
	if (f1 <= 0 || f2 <= 0) return INT2NUM(9999999);
	double min = f1;
	if (f2 < min) min = f2;
	return DBL2NUM((f1-f2)/min);
}

/* Replacement of:
 * bootstrap.rb/MathUtil
 * 	def stat_array(array)
 */
VALUE method_stat_array(VALUE self, VALUE v_ary) {
	VALUE r_result = rb_ary_new_capa(4);
	long sz = RARRAY_LEN(v_ary);
	double sum = 0;
	double devi = 0;
	double mean;
	double ary[sz];

	if (RB_TYPE_P(v_ary, T_NIL) == 1)
		goto four_zeros;
	if (RB_TYPE_P(v_ary, T_ARRAY) != 1)
		rb_raise(rb_eTypeError, "invalid type for stat_array() array");
	if (sz == 0)
		goto four_zeros;

	for (int i=0; i<sz; i++) {
		ary[i] = NUM2DBL(rb_ary_entry(v_ary, i));
		sum += ary[i];
	}
	mean = sum/sz;
	for (int i=0; i<sz; i++)
		devi = devi + (ary[i] - mean)*(ary[i] - mean);
	devi = sqrt(devi/sz);
	// return [n, sum, mean, deviation]
	rb_ary_push(r_result, INT2NUM(sz));
	rb_ary_push(r_result, DBL2NUM(sum));
	rb_ary_push(r_result, DBL2NUM(mean));
	rb_ary_push(r_result, DBL2NUM(devi));
	return r_result;

four_zeros:
	rb_ary_push(r_result, INT2NUM(0));
	rb_ary_push(r_result, INT2NUM(0));
	rb_ary_push(r_result, INT2NUM(0));
	rb_ary_push(r_result, INT2NUM(0));
	return r_result;
}

/* Replacement of:
 * bootstrap.rb/MathUtil
 * 	def rough_num(f)
 */
VALUE method_rough_num(VALUE self, VALUE v_f) {
	double f;
	if (RB_TYPE_P(v_f, T_FIXNUM) == 1) {
		f = (double)NUM2INT(v_f);
	} else if (RB_TYPE_P(v_f, T_FLOAT) == 1) {
		f = NUM2DBL(v_f);
	} else
		rb_raise(rb_eTypeError, "invalid type for rough_num() f");

	double fabs = f;
	if (f < 0) fabs = 0 - f;
	if (fabs >= 100)
		return DBL2NUM(round(f));
	else if (fabs >= 1)
		return DBL2NUM(urn_round(f, 2));
	else if (fabs >= 0.01)
		return DBL2NUM(urn_round(f, 4));
	else if (fabs >= 0.0001)
		return DBL2NUM(urn_round(f, 6));
	else if (fabs >= 0.000001)
		return DBL2NUM(urn_round(f, 8));
	else
		return v_f;
}

/* Replacement of:
 * bootstrap.rb/MathUtil
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
	rb_define_method(URN_CORE, "diff", method_diff, 2);
	rb_define_method(URN_CORE, "stat_array", method_stat_array, 1);
	rb_define_method(URN_CORE, "rough_num", method_rough_num, 1);
	rb_define_method(URN_CORE, "format_num", method_format_num, -1);
}
