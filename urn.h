#ifndef URANUS_COMMON
#define URANUS_COMMON

#include <stdio.h>
#include <ctype.h>

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

static struct timeval urn_global_tv;
extern char   urn_global_log_buf[];

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
#define URN_ABS(a) ((a) > 0 ? (a) : (0-a))
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

extern double urn_round(double f, int precision);
extern long urn_usdiff(struct timeval t1, struct timeval t2);
extern long urn_msdiff(struct timeval t1, struct timeval t2);
extern long parse_timestr_w_e6(struct tm *tmp, const char *s, const char* format);
extern void urn_s_upcase(char *s, int slen);
extern void urn_s_downcase(char *s, int slen);
extern void urn_s_trim(const char* str, char* new_s);


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
	long   intg;
	unsigned long frac_ext; // frac_ext = frac * 1e-URN_INUM_PRECISE
	bool   pstv; // when intg is zero, use this for sign.
	// description size padding to 64
	char   s[64-sizeof(long)-sizeof(size_t)-sizeof(bool)];
} urn_inum;

#define urn_inum_iszero(i) (((i)->intg == 0) && ((i)->frac_ext == 0))

extern char *urn_inum_str(urn_inum *i);
extern int urn_inum_sprintf(urn_inum *i, char *s);
extern int urn_inum_parse(urn_inum *i, const char *s);
extern int urn_inum_cmp(urn_inum *i1, urn_inum *i2);
extern double urn_inum_to_db(urn_inum *i);
extern int urn_inum_from_db(urn_inum *i, double d);
extern int urn_inum_alloc(urn_inum **i, const char *s);
extern int urn_inum_add(urn_inum *i1, urn_inum *i2);
extern void urn_inum_mul(urn_inum *i, double m);

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
extern urn_ticks *urn_tick_alloc();
extern void urn_tick_init(urn_ticks *ticks);
extern int urn_tick_append(urn_ticks *ticks, bool buy, urn_inum *p, urn_inum *s, unsigned long ts_e6);
extern int urn_tick_get(urn_ticks *ticks, int idx, bool *buy, urn_inum *p, urn_inum *s, unsigned long *ts_e6);
extern int urn_tick_get_ts_e6(urn_ticks *ticks, int idx, unsigned long *ts_e6);
extern int urn_sprintf_tick_json(urn_ticks *ticks, int idx, char *s);

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
#define urn_shm_exch_num 14 // Always match len(urn_shm_exch_list)
#define urn_shm_exch_list \
	"Binance","BNCM","BNUM","Bybit", \
	"BybitL","Coinbase","FTX","Kraken", \
	"HashkeyG","Gemini","Bitstamp","BybitI", \
	"OKX","Hashkey","\0" // preset index for exchanges, SHMEM_KEY relies on this, rename and append only.

extern const char *urn_shm_exchanges[];

extern int urn_odbk_shm_i(char *exchange);
extern int urn_odbk_shm_init(bool writer, char *exchange, urn_odbk_mem **shmptr);
extern int urn_odbk_shm_write_index(urn_odbk_mem *shmp, char **pair_arr, int len);
extern int urn_odbk_shm_pair_index(urn_odbk_mem *shmp, char *pair);
extern void urn_odbk_shm_print_index(urn_odbk_mem *shmp);
extern int urn_odbk_shm_print(urn_odbk_mem *shmp, int pairid);
extern int urn_odbk_clients_init(char *exchange, urn_odbk_clients **shmptr);
extern int urn_odbk_clients_clear(urn_odbk_clients *shmp);
extern int urn_odbk_clients_dereg(urn_odbk_clients *shmp, int pairid);
extern int urn_odbk_clients_reg(urn_odbk_clients *shmp, int pairid);
extern int urn_odbk_clients_notify(urn_odbk_mem *odbk_shmp, urn_odbk_clients *shmp, int pairid);
extern int urn_odbk_clients_print(urn_odbk_clients *shmp, int pairid);
#endif
