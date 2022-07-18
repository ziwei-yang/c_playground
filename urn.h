#ifndef URANUS_COMMON
#define URANUS_COMMON

#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

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
		stderr, "%s:%d %s(): ERROR %s return with %d\n", \
		__FILE__, __LINE__, __FUNCTION__, \
		msg, ret), ret;
#define URN_FATAL(msg, ret) perror(msg) , fprintf (\
		stderr, "%s:%d %s(): FATAL %s\nexit with %d\n", \
		__FILE__, __LINE__, __FUNCTION__, \
		msg, ret), \
		exit(ret), ret;
#define URN_FATAL_NNG(ret)  fprintf (\
		stderr, "%s:%d %s(): FATAL %s\nexit with %d\n", \
		__FILE__, __LINE__, __FUNCTION__, \
		nng_strerror(ret), ret), \
		exit(ret), ret;

#define URN_RET_ON_NULL(stmt, emsg, ret) if ((stmt) == NULL) return URN_ERRRET(emsg, ret);
#define URN_RET_ON_RV(stmt, emsg) if ((rv = (stmt)) != 0) return URN_ERRRET(emsg, rv);
#define URN_RET_IF(stmt, emsg, ret) if (stmt) return URN_ERRRET(emsg, ret);
#define URN_RET_UNLESS(stmt, emsg, ret) if (!(stmt)) return URN_ERRRET(emsg, ret);

#define URN_GO_FINAL_ON_NULL(stmt, emsg) if ((stmt) == NULL) { URN_LOG(emsg); goto final; }
#define URN_GO_FINAL_ON_RV(stmt, emsg) if ((rv = (stmt)) != 0) { URN_LOG(emsg); goto final; }
#define URN_GO_FINAL_IF(stmt, emsg) if (stmt) { URN_LOG(emsg); goto final; }

//////////////////////////////////////////
// Log, debug and colored printing.
//////////////////////////////////////////

struct timeval urn_global_tv;
char   urn_global_log_buf[1024*1024]; // 64K was small for large log
#define URN_TIMEZONE 28800 // change this if not in +0800 timezone
#if __APPLE__
#define URN_PRINT_WRAP(pipe, pre, msg, tail) gettimeofday(&urn_global_tv, NULL), \
	fprintf (pipe, "%s%02lu:%02lu:%02lu.%06u %12.12s:%-4d: %s%s\n", \
			pre, \
			((urn_global_tv.tv_sec+URN_TIMEZONE) % 86400)/3600, \
			(urn_global_tv.tv_sec % 3600)/60, \
			(urn_global_tv.tv_sec % 60), \
			urn_global_tv.tv_usec, \
			__FILE__, __LINE__, msg, tail);
#else
#define URN_PRINT_WRAP(pipe, pre, msg, tail) gettimeofday(&urn_global_tv, NULL), \
	fprintf (pipe, "%s%02lu:%02lu:%02lu.%06ld %12.12s:%-4d: %s%s\n", \
			pre, \
			((urn_global_tv.tv_sec+URN_TIMEZONE) % 86400)/3600, \
			(urn_global_tv.tv_sec % 3600)/60, \
			(urn_global_tv.tv_sec % 60), \
			urn_global_tv.tv_usec, \
			__FILE__, __LINE__, msg, tail);
#endif

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
#define URN_DEBUGF_C(color, ...)   sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_PRINT_WRAP(stdout, CLR_##color, urn_global_log_buf, CLR_RST)
#define URN_LOGF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_LOG(urn_global_log_buf);
#define URN_INFOF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_INFO(urn_global_log_buf);
#define URN_WARNF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_WARN(urn_global_log_buf);
#define URN_ERRF(...) sprintf(urn_global_log_buf, __VA_ARGS__), \
	URN_ERR(urn_global_log_buf);

// Turn off all URN_DEBUG macros, unless URN_MAIN_DEBUG is ON
#ifndef URN_MAIN_DEBUG
#undef URN_DEBUG
#define URN_DEBUG(...) ;
#undef URN_DEBUGF
#define URN_DEBUGF(...) ;
#undef URN_DEBUGF_C
#define URN_DEBUGF_C(...) ;
#endif

//////////////////////////////////////////
// MIN, MAX and others.
//////////////////////////////////////////

#define URN_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define URN_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define _URN_RGCAP(a, min, max) ((a<min) ? min : ((a>max) ? max : a)) // internal use
#define URN_INTCAP(a) (int)(_URN_RGCAP((long long)(a), (long long)INT_MIN, (long long)INT_MAX))

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
// Double tools
//////////////////////////////////////////
double urn_round(double f, int precision) {
	if (precision == 0)
		return (double)(round(f));
	char str[64];
	sprintf(str, "%.*lf", precision, f);
	double newf;
	sscanf(str, "%lf", &newf);
	return newf;
}

//////////////////////////////////////////
// Diff of timeval
//////////////////////////////////////////
long urn_usdiff(struct timeval t1, struct timeval t2) {
	return (long)(t2.tv_sec - t1.tv_sec) * 1000000 + (long) (t2.tv_usec - t1.tv_usec);
}
long urn_msdiff(struct timeval t1, struct timeval t2) {
	return (long)(t2.tv_sec - t1.tv_sec) * 1000 + (long) (t2.tv_usec - t1.tv_usec)/1000;
}

/*
 * 2022-06-18T09:14:56.470799Z - example:
 * s-------------------us----invalid_char
 * format should be '%Y-%m-%dT%H:%M:%S.'
 */
long parse_timestr_w_e6(struct tm *tmp, const char *s, const char* format) {
	URN_DEBUGF("parse_timestr_w_e6 strptime %s %s", s, format);
	char *end = strptime(s, format, tmp); // parsed as local time.
	// timezone global is defined in time.h, reset to GMT
	URN_DEBUGF("parse_timestr_w_e6 timegm");
	time_t epoch = timegm(tmp);
#ifdef __apple__
	epoch -= timezone;
#endif
	if (end == NULL) // no microsecond
		return epoch*1000000l;
	// parse microsecond
	char *invalid_char;
	URN_DEBUGF("parse_timestr_w_e6 strtol %s", end);
	long us = strtol(end, &invalid_char, 10);
	URN_DEBUGF("parse_timestr_w_e6 strtol us %ld", us);
	unsigned int mul[7] = {0, 100000, 10000, 1000, 100, 10, 1};
	if (invalid_char-end > 6)
		us = us % 1000000;
	else if (invalid_char-end == 0)
		us = mul[0];
	else
		us = us * mul[invalid_char-end];
	long ts_e6 = epoch*1000000l + us;
	return ts_e6;
}

//////////////////////////////////////////
// Enhanced String struct
//////////////////////////////////////////

void urn_s_upcase(char *s, int slen) {
	for (int i = 0; s[i] != '\0' && i < slen; i++)
		if(s[i] >= 'a' && s[i] <= 'z')
			s[i] = s[i] - 32;
}

void urn_s_downcase(char *s, int slen) {
	for (int i = 0; s[i] !='\0' && i < slen; i++)
		if(s[i] >= 'A' && s[i] <= 'Z')
			s[i] = s[i] + 32;
}

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


//////////////////////////////////////////
// Basic market structs
//////////////////////////////////////////

//////////////////////////////////////////
// Market number represent in integer.
//////////////////////////////////////////
#define URN_INUM_PRECISE 12
#define URN_INUM_FRACEXP 1000000000000
/* 64 bits fixed size of num, fixed size for share memory  */
typedef struct urn_inum {
	unsigned long intg;
	unsigned long frac_ext; // frac_ext = frac * 1e-URN_INUM_PRECISE
	bool   pstv; // when intg is zero, use this for sign.
	// description size padding to 64
	char   s[64-sizeof(long)-sizeof(size_t)-sizeof(bool)];
} urn_inum;

#define urn_inum_iszero(i) (((i)->intg == 0) && ((i)->frac_ext == 0))

// print inum into s (or internal description pointer)
char *urn_inum_str(urn_inum *i) {
	if (i == NULL) return NULL;
	char *s = i->s;
	if (s[0] != '\0') // generated already.
		return s;

	if (i->frac_ext == 0) {
		sprintf(s, "%ld%*s", i->intg, URN_INUM_PRECISE+1, "");
		return s;
	}

	int chars = 0;
	if (i->pstv)
		chars = sprintf(s, "%ld.%012zu", i->intg, i->frac_ext);
	else if (i->intg != 0)
		chars = sprintf(s, "%ld.%012zu", i->intg, i->frac_ext);
	else // intg=0 and negative.
		chars = sprintf(s, "-%ld.%012zu", i->intg, i->frac_ext);
	// Replace 0 with space at the tail of fraction
	// 1.230000 -> 1.23
	while (chars > 0) {
		if (s[chars-1] == '0') {
			s[chars-1] = ' ';
			chars--;
		} else
			break;
	}
	return s;
}

int urn_inum_sprintf(urn_inum *i, char *s) {
	// too many spaces in urn_inum_str()
	// return sprintf(s, "%s", urn_inum_str(i));
	if (i->frac_ext == 0) // integer only.
		return sprintf(s, "%ld", i->intg);
	else if (i->pstv)
		return sprintf(s, "%ld.%012zu", i->intg, i->frac_ext);
	else if (i->intg != 0)
		return sprintf(s, "%ld.%012zu", i->intg, i->frac_ext);
	else // intg=0 and negative.
		return sprintf(s, "-%ld.%012zu", i->intg, i->frac_ext);
}

// Every inum should be initialized here.
int urn_inum_parse(urn_inum *i, const char *s) {
	int rv = 0;
	// -0.00456 -> intg=0 intg_s='-0'  frac_s='00456'
	char intg_s[99];
	sscanf(s, "%s.", intg_s);
	i->pstv = (intg_s[0] == '-') ? 0 : 1;

	char frac_s[99];
	rv = sscanf(s, "%ld.%s", &(i->intg), frac_s);
	if (rv == 1) {
		i->frac_ext = 0;
		(i->s)[0] = '\0'; // clear desc
		return 0;
	} else if (rv != 2) {
		(i->s)[0] = '\0'; // clear desc
		return EINVAL;
	}

	// right pading '00456'
	// -> rpad(URN_INUM_PRECISE) '00456000000000'
	// -> to unsigned long
	unsigned int padto = URN_INUM_PRECISE;
	char frac_padded[padto+1];
	unsigned int cp_ct = URN_MIN(padto, strlen(frac_s));
	for (unsigned int p = 0; p < padto; p++)
		frac_padded[p] = (p<cp_ct) ? frac_s[p] : '0';
	frac_padded[padto] = '\0';
	i->frac_ext = (size_t)(atol(frac_padded));

	(i->s)[0] = '\0'; // clear desc
	return 0;
}

int urn_inum_cmp(urn_inum *i1, urn_inum *i2) {
	if (i1 == NULL && i2 == NULL) return 0;
	if (i1 != NULL && i2 == NULL) return INT_MAX;
	if (i1 == NULL && i2 != NULL) return INT_MIN;
//	URN_DEBUGF("Sorting inum %20s and %20s", urn_inum_str(i1), urn_inum_str(i2));
	if (i1->pstv && i2->pstv) {
		if (i1->intg != i2->intg)
			return (i1->intg > i2->intg) ? 1 : -1;
		if (i1->frac_ext != i2->frac_ext)
			return (i1->frac_ext > i2->frac_ext) ? 1 : -1;
		return 0;
	} else if (i1->pstv && !(i2->pstv)) {
		return 1;
	} else if (!(i1->pstv) && i2->pstv) {
		return -1;
	} else if (!(i1->pstv) && !(i2->pstv)) {
		if (i1->intg != i2->intg)
			return (i1->intg > i2->intg) ? 1 : -1;
		// when intg is same, fraction bigger means number smaller.
		if (i1->frac_ext != i2->frac_ext)
			return (i1->frac_ext < i2->frac_ext) ? 1 : -1;
		return 0;
	}
	return 0;
}

double urn_inum_to_db(urn_inum *i) {
	if (i->intg == 0 && !(i->pstv))
		return 0 - (double)(i->frac_ext)/URN_INUM_FRACEXP;
	else
		return (double)(i->intg) + (double)(i->frac_ext)/URN_INUM_FRACEXP;
}

int urn_inum_from_db(urn_inum *i, double d) {
	sprintf(i->s, "%lf", d);
	int rv = urn_inum_parse(i, i->s);
	(i->s)[0] = '\0'; // clear desc
	return rv;
}

int urn_inum_alloc(urn_inum **i, const char *s) {
	URN_RET_ON_NULL(*i = malloc(sizeof(urn_inum)), "Not enough memory", ENOMEM);
	return urn_inum_parse(*i, s);
}

/* add i2 to i1 */
int urn_inum_add(urn_inum *i1, urn_inum *i2) {
	if (i1 == NULL && i2 == NULL) return EINVAL;
	if (i1 != NULL && i2 == NULL) return EINVAL;
	if (i1 == NULL && i2 != NULL) return EINVAL;
//	URN_DEBUGF("Add inum %20s to %20s", urn_inum_str(i2), urn_inum_str(i1));
	if (i1->pstv == i2->pstv) {
		// Both pos or neg
		i1->intg += i2->intg;
		i1->frac_ext += i2->frac_ext;
		if (i1->frac_ext > URN_INUM_FRACEXP) {
			i1->intg += 1;
			i1->frac_ext -= URN_INUM_FRACEXP;
		}
	} else {
		// +i1-i2 OR -i1+i2, does not matter, just reverse pstv
		if (i1->intg > i2->intg) {
			i1->intg -= i2->intg;
			i1->intg --;
			i1->frac_ext += URN_INUM_FRACEXP;
			i1->frac_ext -= i2->frac_ext;
			if (i1->frac_ext > URN_INUM_FRACEXP) {
				i1->intg += 1;
				i1->frac_ext -= URN_INUM_FRACEXP;
			}
		} else if (i1->intg == i2->intg) {
			i1->intg = 0;
			if (i1->intg >= i2->intg) {
				i1->frac_ext -= i2->frac_ext;
			} else {
				i1->pstv = !(i1->pstv);
				i1->frac_ext = i2->frac_ext - i1->frac_ext;
			}
		} else if (i1->intg < i2->intg) {
			i1->pstv = !(i1->pstv);
			i1->intg = i2->intg - i1->intg - 1;
			i1->frac_ext = i2->frac_ext - i1->frac_ext + URN_INUM_PRECISE;
			if (i1->frac_ext > URN_INUM_FRACEXP) {
				i1->intg += 1;
				i1->frac_ext -= URN_INUM_FRACEXP;
			}
		}
	}
	return 0;
}

//////////////////////////////////////////
// orderbook data read/write in share memory
//////////////////////////////////////////

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

/* whole orderbook in fixed size to be put in share memory */
/* each urn_inum has a fixed size 8 bytes */
#define URN_ODBK_DEPTH 10
typedef struct urn_odbk {
	char desc[64-sizeof(_Bool)-2*sizeof(unsigned long)]; // 64bit padding
	_Bool complete; // current data is ready to be use.
	unsigned long mkt_ts_e6;
	unsigned long w_ts_e6;
	urn_inum bidp[URN_ODBK_DEPTH];
	urn_inum bids[URN_ODBK_DEPTH];
	urn_inum askp[URN_ODBK_DEPTH];
	urn_inum asks[URN_ODBK_DEPTH];
} urn_odbk;

/* store latest LEN history trades in cycle
 * Init:
 * 	latest_idx = 0; oldest_idx = 1; memset data all NULL;
 * When writing new ticker:
 * 	idx = (latest_idx + 1) % LEN;
 * 	oldest_idx = (oldest_idx + 1) % LEN;
 * 	write at idx; // idx not accessible now.
 * 	latest_idx = (latest_idx + 1) % LEN; // idx accessible
 * When reading, idx = latest_idx, loop;
 * 	break if data == NULL;
 * 	break if idx == oldest_idx;
 * 	read at idx;
 * 	decr idx below: // from latest to oldest
 * 	if idx > 0
 * 		idx = (idx - 1);
 * 	else
 * 		idx = LEN - 1;
 */
#define URN_TICK_LENTH   16 // must be less than ushort/2
#define URN_TICK_MERGE   1000000 // merge tick at same price in 1000000us(1s)
#define URN_TICK_MERGE_M 100000// merge tick at diff price in 100000us(0.1s)
typedef struct urn_ticks {
	char           desc[64-sizeof(unsigned long)-2*sizeof(unsigned short)];
	unsigned long  latest_t_e6; // 0:no tick, >0:last tick ts_e6
	unsigned short latest_idx;
	unsigned short oldest_idx;
	bool           tickB[URN_TICK_LENTH]; // side is buy
	urn_inum       tickp[URN_TICK_LENTH];
	urn_inum       ticks[URN_TICK_LENTH];
	unsigned long  tickts_e6[URN_TICK_LENTH]; // 0:no data
} urn_ticks;
urn_ticks *urn_tick_alloc() {
	return calloc(1, sizeof(urn_ticks));
}
void urn_tick_init(urn_ticks *ticks) {
	memset(ticks, 0, sizeof(urn_ticks));
}
int urn_tick_append(urn_ticks *ticks, bool buy, urn_inum *p, urn_inum *s, unsigned long ts_e6) {
	if (ts_e6 <= 0) {
		URN_WARNF("write to urn_ticks %p with zero ts_e6", ticks);
		return EINVAL;
	}
	if (ticks->latest_t_e6 == 0) { // init first data.
		// URN_LOGF("write to urn_ticks init 0");
		ticks->latest_idx = 0;
		ticks->oldest_idx = 1;
		memcpy(&(ticks->tickp[0]), p, sizeof(urn_inum));
		memcpy(&(ticks->ticks[0]), s, sizeof(urn_inum));
		ticks->tickts_e6[0] = ts_e6;
		ticks->tickB[0] = buy;
		ticks->latest_t_e6 = ts_e6;
		return 0;
	}
	unsigned short idx;
#ifdef URN_TICK_MERGE
	idx = ticks->latest_idx;
	if ((ticks->tickts_e6[idx] + URN_TICK_MERGE > ts_e6) &&
			(ticks->tickB[idx] == buy) &&
			(urn_inum_cmp(p, &(ticks->tickp[idx])) == 0)) {
		// Time close, p same, buy same -> merge size
		// Also update time for next merge.
		urn_inum_add(&(ticks->ticks[idx]), s);
		ticks->tickts_e6[idx] = ts_e6;
		ticks->latest_t_e6 = ts_e6;
		return 0;
	} else if ((ticks->tickts_e6[idx] + URN_TICK_MERGE_M > ts_e6) &&
			(ticks->tickB[idx] == buy)) {
		// Time very close, buy same -> merge size
		// Update price
		// Also update time for next merge.
		urn_inum_add(&(ticks->ticks[idx]), s);
		memcpy(&(ticks->tickp[idx]), p, sizeof(urn_inum));
		ticks->tickts_e6[idx] = ts_e6;
		ticks->latest_t_e6 = ts_e6;
		return 0;
	}
#endif
	idx = (ticks->latest_idx + 1) % URN_TICK_LENTH;
	ticks->oldest_idx = (ticks->oldest_idx + 1) % URN_TICK_LENTH;
	memcpy(&(ticks->tickp[idx]), p, sizeof(urn_inum));
	memcpy(&(ticks->ticks[idx]), s, sizeof(urn_inum));
	ticks->tickB[idx] = buy;
	ticks->tickts_e6[idx] = ts_e6;
	ticks->latest_t_e6 = ts_e6;
	ticks->latest_idx = idx;
	return 0;
}
int urn_tick_get(urn_ticks *ticks, int idx, bool *buy, urn_inum *p, urn_inum *s, unsigned long *ts_e6) {
	if (idx >= URN_TICK_LENTH) return ERANGE;
	unsigned short i = (ticks->latest_idx + URN_TICK_LENTH - idx) % URN_TICK_LENTH;
	if (i > ticks->latest_idx && i < ticks->oldest_idx) return ERANGE;
	if (ticks->tickts_e6[i] == 0) return ERANGE;
	memcpy(p, &(ticks->tickp[i]), sizeof(urn_inum));
	memcpy(s, &(ticks->ticks[i]), sizeof(urn_inum));
	*buy = ticks->tickB[i];
	*ts_e6 = ticks->tickts_e6[i];
	// URN_LOGF("read urn_ticks %hu %hhu %lu", i, *buy, *ts_e6);
	return 0;
}

// First data is a 512 pair name index
// For each pair, repeatly swap writing in 2 caches [ready, dirty] <-> [dirty, ready]
// on macos the sizeof(urn_odbk_mem) is 4976640, can't fit in default 4MB
// To change SHMMAX on macos, see:
// 	https://dansketcher.com/2021/03/30/shmmax-error-on-big-sur/
// OR:
// 	macos_chg_shmmax.sh
#define URN_ODBK_MAX_PAIR 512
typedef struct urn_odbk_mem {
	char      pairs[URN_ODBK_MAX_PAIR][16];
	urn_odbk  odbks[URN_ODBK_MAX_PAIR][2];
	/* ticks history appened */
	urn_ticks ticks[URN_ODBK_MAX_PAIR];
} urn_odbk_mem;

#define urn_odbk_pid_cap 8
typedef struct urn_odbk_clients {
	char pids_desc[URN_ODBK_MAX_PAIR][urn_odbk_pid_cap][64];
	pid_t pids[URN_ODBK_MAX_PAIR][urn_odbk_pid_cap];
} urn_odbk_clients;
typedef urn_odbk_clients urn_tick_clients;
#define urn_shm_exch_num 12
#define urn_shm_exch_list \
	"Binance","BNCM","BNUM","Bybit", \
	"BybitU","Coinbase","FTX","Kraken", \
	"Bittrex", "Gemini", "Bitstamp", "BybitS" \
	"\0" // SHMEM_KEY depends on list order
const char *urn_shm_exchanges[] = { urn_shm_exch_list };

/* linear find index from constant urn_shm_exch_list, please cache result */
int urn_odbk_shm_i(char *exchange) {
	int i = 0;
	while(i < urn_shm_exch_num) {
		if (urn_shm_exchanges[i][0] == '\0') return -1;
		if (strcasecmp(urn_shm_exchanges[i], exchange) == 0)
			return i;
		i++;
	}
	return -1;
}

/* Attach SHMEM, reset data, reset pairs index */
int urn_odbk_shm_init(bool writer, char *exchange, urn_odbk_mem **shmptr) {
	int rv = 0;
	key_t tmp_shmkey = 0;
	key_t *shmkey = &tmp_shmkey;
	int tmp_shmid = 0;
	int *shmid = &tmp_shmid;

	*shmkey = urn_odbk_shm_i(exchange);
	if (*shmkey < 0)
		URN_RET_ON_RV(EINVAL, "Unknown exchange in odbk_shm_init()");
	// SHMKEY starts from 0xA001, return -1 if not found.
	*shmkey += 0xA001;

	int shmflg = 0644;
	bool created = false;
	unsigned long desired_sz = sizeof(urn_odbk_mem);

	URN_LOGF("odbk_shm_init() get  at key %#08x size %lu %c",
		*shmkey, desired_sz, writer ? 'W' : 'R');
	*shmid = shmget(*shmkey, desired_sz, shmflg);
	if ((*shmid == -1) && (errno == ENOENT) && writer) {
		URN_LOGF("odbk_shm_init() new  at key %#08x size %lu %c",
			*shmkey, desired_sz, writer ? 'W' : 'R');
		*shmid = shmget(*shmkey, desired_sz, shmflg | IPC_CREAT);
		created = true;
	} else if ((*shmid == -1) && (errno == EINVAL)) {
		// If is reader, and memory size is larger than desired_sz.
		// Okay because of writer has newer but comaptiable struct
		URN_WARNF("odbk_shm_init() EINVAL at key %#08x size %lu %c",
			*shmkey, desired_sz, writer ? 'W' : 'R');
		URN_WARN("Checking share memory stat");
		*shmid = shmget(*shmkey, 0, 0);
		struct shmid_ds ds;
		rv = shmctl(*shmid, IPC_STAT, &ds);
		if (rv != 0) {
			perror("shmctl IPC_STAT error");
			URN_RET_ON_RV(errno, "shmget with shmid failed in odbk_shm_init()");
		}
		URN_WARNF("\tIPC_STAT rv: %d", rv);
		URN_WARNF("\tshm_segsz:   %zu", ds.shm_segsz);
		URN_WARNF("\tlast op pid: %d", ds.shm_lpid);
		URN_WARNF("\tcreator pid: %d", ds.shm_cpid);
		URN_WARNF("\tnum attach:  %lu", (unsigned long)(ds.shm_nattch));
		URN_WARNF("desired_sz %lu", desired_sz);
		if (writer) {
			URN_WARN("Try to remove it as writer");
			URN_RET_ON_RV(shmctl(*shmid, IPC_RMID, NULL),
				"shmctl with IPC_REID failed in odbk_shm_init()");
			created = true;
		} else if (ds.shm_segsz <= desired_sz) {
			// as reader, refuse smaller shm_segsz
			URN_WARN("Reader only accepts larger shm_segsz but not smaller");
			URN_RET_ON_RV(EINVAL, "shmget with shmid failed in odbk_shm_init()");
		} else {
			// as reader, accept larger shm_segsz only.
			URN_WARN("Try shmget() as reader with larger size");
			desired_sz = ds.shm_segsz;
		}
		URN_LOGF("odbk_shm_init() new  at key %#08x size %lu %c",
			*shmkey, desired_sz, writer ? 'W' : 'R');
		*shmid = shmget(*shmkey, desired_sz, shmflg | IPC_CREAT);
	}
	if (*shmid == -1)
		URN_RET_ON_RV(errno, "shmget with shmid failed in odbk_shm_init()");

	*shmptr = shmat(*shmid, NULL, 0);
	if (*shmptr == (void *) -1)
		URN_RET_ON_RV(errno, "Unable to attach share memory in odbk_shm_init()");

	if (created) {
		URN_INFO("Cleaning new created urn_odbk_mem");
		// set pair name NULL, mark all odbk dirty.
		for (int i = 0; i < URN_ODBK_MAX_PAIR; i++) {
			(*shmptr)->pairs[i][0] = '\0';
			(*shmptr)->odbks[i][0].complete = false;
			(*shmptr)->odbks[i][1].complete = false;
			// set all zero.
			urn_tick_init(&((*shmptr)->ticks[i]));
		}
	}
	URN_INFOF("odbk_shm_init() done at key %#08x id %d size %lu ptr %p %c",
		*shmkey, *shmid, desired_sz, *shmptr, writer ? 'W' : 'R');
	return rv;
}

/* write pair index to SHMEM */
int urn_odbk_shm_write_index(urn_odbk_mem *shmp, char **pair_arr, int len) {
	if (len > URN_ODBK_MAX_PAIR && len <= 1) {
		// pair_arr[0] should always be pair name of NULL(no such pair)
		URN_WARNF("odbk_shm_write_index() with illegal len %d", len);
		return ERANGE;
	}
	// mark [0] as useless
	strcpy(shmp->pairs[0], "USELESS");
	for (int i = 1; i < URN_ODBK_MAX_PAIR; i++) {
		if (i <= len)
			strcpy(shmp->pairs[i], pair_arr[i]);
		else
			shmp->pairs[i][0] = '\0';
	}
	// Mark all odbk dirty.
	for (int i = 0; i < URN_ODBK_MAX_PAIR; i++) {
		shmp->odbks[i][0].complete = false;
		shmp->odbks[i][1].complete = false;
	}
	return 0;
}

int urn_odbk_shm_pair_index(urn_odbk_mem *shmp, char *pair) {
	int pairid = -1;
	for (int i=0; i<URN_ODBK_MAX_PAIR; i++) {
		if (shmp->pairs[i][0] == '\0')
			break;
		if (strcasecmp(pair, shmp->pairs[i]) == 0) {
			pairid = i;
			break;
		}
	}
	return pairid;
}

void urn_odbk_shm_print_index(urn_odbk_mem *shmp) {
	for (int i=0; i<URN_ODBK_MAX_PAIR; i++) {
		if (shmp->pairs[i][0] == '\0')
			break;
		URN_LOGF("urn_odbk_mem %p pair %d %s", shmp, i, shmp->pairs[i]);
	}
}

int urn_odbk_shm_print(urn_odbk_mem *shmp, int pairid) {
	int timezone = URN_TIMEZONE;
	URN_RET_IF((pairid >= URN_ODBK_MAX_PAIR), "pairid too big", ERANGE);
	printf("odbk_shm_print %d [%s] complete: [%d, %d] ", pairid, shmp->pairs[pairid],
		shmp->odbks[pairid][0].complete, shmp->odbks[pairid][1].complete);

	urn_odbk *odbk = NULL;
	if (shmp->odbks[pairid][0].complete) {
		odbk = &(shmp->odbks[pairid][0]);
	} else if (shmp->odbks[pairid][1].complete) {
		odbk = &(shmp->odbks[pairid][1]);
	}
	if (odbk == NULL) {
		printf("odbk_shm_init %d [%s] -- N/A --\n", pairid, shmp->pairs[pairid]);
		return 0;
	}

	long mkt_ts_e6 = odbk->mkt_ts_e6;
	long w_ts_e6 = odbk->w_ts_e6;
	double latency_e3 = (w_ts_e6 - mkt_ts_e6)/1000.0;

	struct timeval now_t;
	gettimeofday(&now_t, NULL);

	long shm_latency_e6 = now_t.tv_sec*1000000 + now_t.tv_usec - w_ts_e6;

	mkt_ts_e6 += (timezone*1000000l);
	printf("mkt %02lu:%02lu:%02lu.%06ld shm_w +%8.3f ms shm_r +%6.3f ms\n",
			((mkt_ts_e6/1000000) % 86400)/3600,
			((mkt_ts_e6/1000000) % 3600)/60,
			((mkt_ts_e6/1000000) % 60),
			mkt_ts_e6 % 1000000, latency_e3,
			((float)shm_latency_e6)/1000);
	mkt_ts_e6 -= (timezone*1000000l);

	for (int i = 0; i < URN_ODBK_DEPTH; i++) {
		if (urn_inum_iszero(&(odbk->bidp[i])) &&
			urn_inum_iszero(&(odbk->bids[i])) &&
			urn_inum_iszero(&(odbk->askp[i])) &&
			urn_inum_iszero(&(odbk->asks[i]))) {
			break;
		}
		printf("%d %18s %18s - %18s %18s\n", i,
			urn_inum_str(&(odbk->bidp[i])),
			urn_inum_str(&(odbk->bids[i])),
			urn_inum_str(&(odbk->askp[i])),
			urn_inum_str(&(odbk->asks[i])));
	}

	// Print latest 6 trades.
	urn_ticks *ticks = &(shmp->ticks[pairid]);
	bool buy = false;
	urn_inum p, s;
	unsigned long ts_e6 = 0, ts = 0;
	printf("Trades %p idx old %hu -> %hu latest_t_e6 %lu timezone %d\n",
		ticks, ticks->oldest_idx, ticks->latest_idx,
		ticks->latest_t_e6, timezone);
	for (int i = 0; i < 6; i++) {
		int rv = urn_tick_get(ticks, i, &buy, &p, &s, &ts_e6);
		ts = ts_e6 / 1000000;
		if (rv == ERANGE) {
			printf(CLR_YELLOW "ERANGE" CLR_RST "\n");
			break;
		}
		ts += timezone;
		printf("%s  %24s %24s %02lu:%02lu:%02lu.%06lu%s\n",
			(buy ? CLR_GREEN : CLR_RED),
			urn_inum_str(&p), urn_inum_str(&s),
			(ts % 86400)/3600, (ts % 3600)/60,
			(ts % 60), ts_e6 % 1000000, CLR_RST);
	}
	return 0;
}

int urn_odbk_clients_init(char *exchange, urn_odbk_clients **shmptr) {
	int rv = 0;
	key_t tmp_shmkey = 0;
	key_t *shmkey = &tmp_shmkey;
	int tmp_shmid = 0;
	int *shmid = &tmp_shmid;
	bool writer = true; // Always true in urn_odbk_clients operation.

	*shmkey = urn_odbk_shm_i(exchange);
	if (*shmkey < 0)
		URN_RET_ON_RV(EINVAL, "Unknown exchange in odbk_shm_init()");
	// clients SHMKEY starts from 0xA0A1, return -1 if not found.
	*shmkey += 0xA0A1;

	int shmflg = 0644;
	bool created = false;
	unsigned long desired_sz = sizeof(urn_odbk_clients);

	URN_LOGF("odbk_shm_init() get  at key %#08x size %lu %c",
		*shmkey, desired_sz, writer ? 'W' : 'R');
	*shmid = shmget(*shmkey, desired_sz, shmflg);
	if ((*shmid == -1) && (errno == ENOENT) && writer) {
		URN_LOGF("odbk_shm_init() new  at key %#08x size %lu %c",
			*shmkey, desired_sz, writer ? 'W' : 'R');
		*shmid = shmget(*shmkey, desired_sz, shmflg | IPC_CREAT);
		created = true;
	}
	if (*shmid == -1) {
		URN_RET_ON_RV(errno, "shmget with shmid failed in odbk_shm_init()");
	}

	*shmptr = shmat(*shmid, NULL, 0);
	if (*shmptr == (void *) -1)
		URN_RET_ON_RV(errno, "Unable to attach share memory in odbk_shm_init()");

	if (created) {
		URN_INFO("Cleaning new created urn_odbk_clients");
		// set pid as zero, desc NULL
		for (int i = 0; i < URN_ODBK_MAX_PAIR; i++) {
			for (int j = 0; j < urn_odbk_pid_cap; j++) {
				(*shmptr)->pids[i][j] = 0;
				(*shmptr)->pids_desc[i][j][0] = '\0';
			}
		}
	}
	URN_INFOF("odbk_shm_init() done at key %#08x id %d size %lu ptr %p %c",
		*shmkey, *shmid, desired_sz, *shmptr, writer ? 'W' : 'R');
	return rv;
}

int urn_odbk_clients_clear(urn_odbk_clients *shmp) {
	pid_t p = getpid();
	for (int i = 0; i < URN_ODBK_MAX_PAIR; i++) {
		for (int j = 0; j < urn_odbk_pid_cap; j++) {
			pid_t p2 = shmp->pids[i][j];
			if (p == p2)
				shmp->pids[i][j] = 0;
		}
	}
	URN_LOGF("Clear client PID %d from share mem %p", p, shmp);
	return 0;
}

/* remove self PID from single pairid area */
int urn_odbk_clients_dereg(urn_odbk_clients *shmp, int pairid) {
	pid_t p = getpid();
	for (int j = 0; j < urn_odbk_pid_cap; j++) {
		pid_t p2 = shmp->pids[pairid][j];
		if (p != p2) continue;
		shmp->pids[pairid][j] = 0;
		URN_LOGF("De-register client %p pairid %d PID %d", shmp, pairid, p);
	}
	return 0;
}

/* Add self PID into clients area, return ENOMEM if no empty slot */
int urn_odbk_clients_reg(urn_odbk_clients *shmp, int pairid) {
	pid_t p = getpid();
	for (int j = 0; j < urn_odbk_pid_cap; j++) {
		pid_t p2 = shmp->pids[pairid][j];
		if (p == p2) {
			URN_LOGF("Register urn odbk client pairid %d PID %d, exists", pairid, p);
			return 0;
		} else if (p2 > 0) {
			// replace slot if PID p2 does not exist.
			if (kill(p2, 0) == 0) continue;
			URN_LOGF("Register urn odbk client pairid %d PID %d, replace %d", pairid, p, p2);
		} else // empty slot
			URN_LOGF("Register urn odbk client pairid %d PID %d, new", pairid, p);
		shmp->pids[pairid][j] = p;
		return 0;
	}
	URN_WARNF("Failed to register urn odbk client pairid %d PID %d", pairid, p);
	return ENOMEM;
}

int urn_odbk_clients_notify(urn_odbk_mem *odbk_shmp, urn_odbk_clients *shmp, int pairid) {
	pid_t p = 0;
	int rv = 0;
	for (int j = 0; j < urn_odbk_pid_cap; j++) {
		p = shmp->pids[pairid][j];
		if (p <= 0) continue;
		rv = kill(p, SIGUSR1);
		if (rv == 0) continue;
		perror("Error in kill SIGUSR1");
		URN_WARNF("rm shm odbk client %d in pair %d %s",
			p, pairid, odbk_shmp->pairs[pairid]);
		shmp->pids[pairid][j] = 0;
	}
	return rv;
}

int urn_odbk_clients_print(urn_odbk_clients *shmp, int pairid) {
	pid_t p = getpid();
	printf("clients for pairid %d [", pairid);
	for (int j = 0; j < urn_odbk_pid_cap; j++) {
		p = shmp->pids[pairid][j];
		if (p > 0) printf(" %d", p);
	}
	printf("]\n");
	return 0;
}
#endif
