#include "ruby.h"
#include "math.h"

#include "urn.h"

double urn_round(double f, int precision);

/* Replacement of:
 * bootstrap.rb/MathUtil
 * 	def diff(f1, f2)
 */
double diff(double f1, double f2) {
	if (f1 <= 0 || f2 <= 0)
		return 9999999.0;
	double min_val = f1 < f2 ? f1 : f2;
	return (f1 - f2) / min_val;
}
VALUE rb_diff(VALUE self, VALUE v_f1, VALUE v_f2) {
	double f1 = NUM2DBL(v_f1);
	double f2 = NUM2DBL(v_f2);
	double result = diff(f1, f2);
	return rb_float_new(result);
}

/* Replacement of:
 * bootstrap.rb/MathUtil
 * 	def stat_array(array)
 */
void stat_array(double* array, size_t size, double* stat_result) {
	if (size == 0) {
		stat_result[0] = 0;
		stat_result[1] = 0;
		stat_result[2] = 0;
		stat_result[3] = 0;
		return;
	}

	double sum = 0;
	for (size_t i = 0; i < size; i++)
		sum += array[i];

	double mean = sum / size;
	double deviation_sum = 0;
	for (size_t i = 0; i < size; i++) {
		double diff = array[i] - mean;
		deviation_sum += diff * diff;
	}

	double deviation = sqrt(deviation_sum / size);

	stat_result[0] = size;
	stat_result[1] = sum;
	stat_result[2] = mean;
	stat_result[3] = deviation;
}
VALUE rb_stat_array(VALUE self, VALUE v_ary) {
	VALUE result = rb_ary_new2(4);
	if (NIL_P(v_ary)) {
		// Return [0, 0, 0, 0] if the input array is nil
		rb_ary_store(result, 0, rb_float_new(0.0));
		rb_ary_store(result, 1, rb_float_new(0.0));
		rb_ary_store(result, 2, rb_float_new(0.0));
		rb_ary_store(result, 3, rb_float_new(0.0));
		return result;
	}

	Check_Type(v_ary, T_ARRAY);

	size_t size = RARRAY_LEN(v_ary);
	double* array = ALLOCA_N(double, size);

	for (size_t i = 0; i < size; i++)
		array[i] = NUM2DBL(rb_ary_entry(v_ary, i));

	double stat_result[4];
	stat_array(array, size, stat_result);

	rb_ary_store(result, 0, rb_float_new(stat_result[0]));
	rb_ary_store(result, 1, rb_float_new(stat_result[1]));
	rb_ary_store(result, 2, rb_float_new(stat_result[2]));
	rb_ary_store(result, 3, rb_float_new(stat_result[3]));

	return result;
}

/* Replacement of:
 * bootstrap.rb/MathUtil
 * 	def rough_num(f)
 */
double rough_num(double f) {
	if (fabs(f) > 100) {
		return round(f);
	} else if (fabs(f) > 1) {
		return urn_round(f, 2);
	} else if (fabs(f) > 0.01) {
		return urn_round(f, 4);
	} else if (fabs(f) > 0.0001) {
		return urn_round(f, 6);
	} else if (fabs(f) > 0.000001) {
		return urn_round(f, 8);
	} else {
		return f;
	}
}
VALUE rb_rough_num(VALUE self, VALUE v_f) {
	double f = NIL_P(v_f) ? 0.0 : NUM2DBL(v_f);
	double result = rough_num(f);
	return rb_float_new(result);
}

/* Replacement of:
 * bootstrap.rb/MathUtil
 * 	def format_num(f, float=8, decimal=8)
 */
void format_numstr(char* numstr, int fraclen, int decilen, char* str) {
	int total_len = fraclen + decilen + 1;
	snprintf(str, total_len + 1, "%*s", total_len, numstr);
}
void format_num(double num, int fraclen, int decilen, char* str) {
	if (num == 0) {
		// Fill the string with spaces if the number is zero
		memset(str, ' ', fraclen + decilen + 1);
		str[fraclen + decilen + 1] = '\0'; // Null-terminate the string
		return;
	}

	// Use snprintf to format the number with specified width and precision
	snprintf(str, fraclen + decilen + 3, "%*.*f", fraclen + decilen + 1, fraclen, num);

	// Replace trailing zeros in the fraction with spaces
	char* dot = strchr(str, '.');
	if (dot == NULL) return;
	char* end = str + strlen(str) - 1;
	if ((int)num == num) {
		while (end >= dot) {
			*end = ' '; end--;
		}
		return;
	}
	while (end > dot && *end == '0') {
		if (end == dot + 1) break;
		*end = ' '; end--;
	}
	// If the last character after removing zeros is a dot, replace it with a space
	if (*end == '.') *end = ' ';
}
VALUE rb_format_num(int argc, VALUE *argv, VALUE klass) {
	VALUE v_num, v_fraclen, v_decilen;
	rb_scan_args(argc, argv, "12", &v_num, &v_fraclen, &v_decilen);

	int fraclen = NIL_P(v_fraclen) ? 8 : NUM2INT(v_fraclen);
	int decilen = NIL_P(v_decilen) ? 8 : NUM2INT(v_decilen);

	char str[fraclen + decilen + 3]; // 1 for sign, 1 for decimal point, 1 for null terminator
	if (NIL_P(v_num)) {
		format_numstr("", fraclen, decilen, str);
	} else if (RB_TYPE_P(v_num, T_STRING)) {
		char* numstr = RSTRING_PTR(v_num);
		format_numstr(numstr, fraclen, decilen, str);
	} else {
		double num = NUM2DBL(v_num);
		format_num(num, fraclen, decilen, str);
	}
	return rb_str_new_cstr(str);
}
