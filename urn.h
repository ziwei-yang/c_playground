#ifndef URANUS_COMMON
#define URANUS_COMMON

#include <time.h>
#include <sys/time.h>

//////////////////////////////////////////
// Terminal color control. ///////////////
//////////////////////////////////////////

#define CLR_RST           "\033[0m"
#define CLR_BOLD          "\033[01m"
#define CLR_DISABLE       "\033[02m"
#define CLR_UNDERLINE     "\033[04m"
#define CLR_BLINK         "\033[05m"
#define CLR_REVERSE       "\033[07m"
#define CLR_STRIKETHROUGH "\033[09m"

#define CLR_BLACK   "\033[30m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"
#define CLR_WHITE   "\033[37m"

#define CLR_LBLACK   "\033[90m"
#define CLR_LRED     "\033[91m"
#define CLR_LGREEN   "\033[92m"
#define CLR_LYELLOW  "\033[93m"
#define CLR_LBLUE    "\033[94m"
#define CLR_LMAGENTA "\033[95m"
#define CLR_LCYAN    "\033[96m"
#define CLR_LWHITE   "\033[97m"

#define CLR_BGBLACK   "\033[40m"
#define CLR_BGRED     "\033[41m"
#define CLR_BGGREEN   "\033[42m"
#define CLR_BGYELLOW  "\033[43m"
#define CLR_BGBLUE    "\033[44m"
#define CLR_BGMAGENTA "\033[45m"
#define CLR_BGCYAN    "\033[46m"
#define CLR_BGWHITE   "\033[47m"

// s must be literal string
#define URN_CLR(color,s) CLR_##color s CLR_RST
#define URN_BLUE(s)   CLR_BLUE  s CLR_RST
#define URN_RED(s)    CLR_RED   s CLR_RST
#define URN_GREEN(s)  CLR_GREEN s CLR_RST

//////////////////////////////////////////
// Fatal printing and exit.
//////////////////////////////////////////

#define URN_ERRRET(msg, ret) fprintf (\
		stderr, "%s:%d %s(): ERROR %s code %d\n", \
		__FILE__, __LINE__, __FUNCTION__, \
		msg, ret), ret;
#define URN_FATAL(msg, ret) fprintf (\
		stderr, "%s:%d %s(): FATAL %s\nexit %d\n", \
		__FILE__, __LINE__, __FUNCTION__, \
		msg, ret), \
		exit(ret), ret;
#define URN_FATAL_NNG(ret)  fprintf (\
		stderr, "%s:%d %s(): FATAL %s\nexit %d\n", \
		__FILE__, __LINE__, __FUNCTION__, \
		nng_strerror(ret), ret), \
		exit(ret), ret;

//////////////////////////////////////////
// Log, debug and colored printing.
//////////////////////////////////////////

struct timeval urn_global_tv;
char   urn_global_log_buf[65536];

#define URN_PRINT_WRAP(pipe, pre, msg, tail) gettimeofday(&urn_global_tv, NULL), \
	fprintf (pipe, "%s%02lu:%02lu:%02lu.%06u %12.12s:%-4d: %s%s\n", \
			pre, \
			(urn_global_tv.tv_sec % 86400)/3600, \
			(urn_global_tv.tv_sec % 3600)/60, \
			(urn_global_tv.tv_sec % 60), \
			urn_global_tv.tv_usec, \
			__FILE__, __LINE__, msg, tail);

#define URN_DEBUG(msg) URN_PRINT_WRAP(stdout, "", msg, "")
#define URN_LOG(msg) URN_PRINT_WRAP(stdout, "", msg, "")
#define URN_INFO(msg) URN_PRINT_WRAP(stdout, CLR_BLUE, msg, CLR_RST)
#define URN_WARN(msg) URN_PRINT_WRAP(stdout, CLR_RED, msg, CLR_RST)
#define URN_ERR(msg) URN_PRINT_WRAP(stderr, "", msg, "")

#define URN_LOG_UNDERLINE(msg)      URN_PRINT_WRAP(stdout, "\033[04m", msg, "\033[0m")
#define URN_LOG_REVERSECOLOR(msg)   URN_PRINT_WRAP(stdout, "\033[07m", msg, "\033[0m")

#define URN_LOG_C(color, msg)    URN_PRINT_WRAP(stdout, CLR_##color, msg, CLR_RST)
#define URN_LOGF_C(color, ...)   sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_PRINT_WRAP(stdout, CLR_##color, urn_global_log_buf, CLR_RST)

#define URN_DEBUGF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_DEBUG(urn_global_log_buf);
#define URN_LOGF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_LOG(urn_global_log_buf);
#define URN_INFOF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_INFO(urn_global_log_buf);
#define URN_WARNF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_WARN(urn_global_log_buf);
#define URN_ERRF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_ERR(urn_global_log_buf);

//////////////////////////////////////////
// MIN, MAX and others.
//////////////////////////////////////////

#define URN_MIN(a, b) (((a) < (b)) ? (a) : (b))

//////////////////////////////////////////
// Common function option
//////////////////////////////////////////
typedef struct urn_func_opt {
	_Bool allow_fail; /* okay to fail */
	_Bool verbose; /* more verbosed output */
	_Bool silent; /* keep no output */
} urn_func_opt;

typedef void (*urn_ptr_cb)(void *p);

//////////////////////////////////////////
// Enhanced String struct
//////////////////////////////////////////

//// substring of origin
//typedef struct urn_s {
//	// char   *origin; /* known of original string address, could be same as s */
//	char   *s; /* string start address */
//	size_t l;  /* length without \0 */
//} urn_s;
//
//void urn_s_cp(char *dest, urn_s s) {
//	memcpy(dest, s.s, s.l);
//	*(dest+s.l) = '\0';
//}
//
//#define urn_substr(orig, from, to) {.s=(orig+from), .l=(to-from)}
//int urn_substr_alloc(urn_s **sp, char *orig, size_t from, size_t to) {
//	*sp = malloc(sizeof(urn_s));
//	if ((*sp) == NULL) return ENOMEM;
//	(*sp)->s = orig+from;
//	(*sp)->l = to-from;
//	return 0;
//}

#endif
