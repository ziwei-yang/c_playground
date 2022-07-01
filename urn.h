#ifndef URANUS_COMMON
#define URANUS_COMMON

#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
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
#if __APPLE__
#define URN_PRINT_WRAP(pipe, pre, msg, tail) gettimeofday(&urn_global_tv, NULL), \
	fprintf (pipe, "%s%02lu:%02lu:%02lu.%06u %12.12s:%-4d: %s%s\n", \
			pre, \
			(urn_global_tv.tv_sec % 86400)/3600, \
			(urn_global_tv.tv_sec % 3600)/60, \
			(urn_global_tv.tv_sec % 60), \
			urn_global_tv.tv_usec, \
			__FILE__, __LINE__, msg, tail);
#elif __linux__
#define URN_PRINT_WRAP(pipe, pre, msg, tail) gettimeofday(&urn_global_tv, NULL), \
	fprintf (pipe, "%s%02lu:%02lu:%02lu.%06ld %12.12s:%-4d: %s%s\n", \
			pre, \
			(urn_global_tv.tv_sec % 86400)/3600, \
			(urn_global_tv.tv_sec % 3600)/60, \
			(urn_global_tv.tv_sec % 60), \
			urn_global_tv.tv_usec, \
			__FILE__, __LINE__, msg, tail);
#else
#define URN_PRINT_WRAP(pipe, pre, msg, tail) gettimeofday(&urn_global_tv, NULL), \
	fprintf (pipe, "%s%02lu:%02lu:%02lu.%06u %12.12s:%-4d: %s%s\n", \
			pre, \
			(urn_global_tv.tv_sec % 86400)/3600, \
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
// Diff of timeval
//////////////////////////////////////////
long urn_usdiff(struct timeval t1, struct timeval t2) {
	return (long)(t2.tv_sec - t1.tv_sec) * 1000000 + (long) (t2.tv_usec - t1.tv_usec);
}
long urn_msdiff(struct timeval t1, struct timeval t2) {
	return (long)(t2.tv_sec - t1.tv_sec) * 1000 + (long) (t2.tv_usec - t1.tv_usec)/1000;
}

//////////////////////////////////////////
// Enhanced String struct
//////////////////////////////////////////

void urn_s_upcase(char *s, int slen) {
	for (int i = 0; s[i]!='\0' && i<slen; i++) {
		if(s[i] >= 'a' && s[i] <= 'z')
			s[i] = s[i] -32;
	}
}

void urn_s_downcase(char *s, int slen) {
	for (int i = 0; s[i]!='\0' && i<slen; i++) {
		if(s[i] >= 'A' && s[i] <= 'Z')
			s[i] = s[i] +32;
	}
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
	long   intg;
	size_t frac_ext; // frac_ext = frac * 1e-URN_INUM_PRECISE
	_Bool  pstv; // when intg is zero, use this for sign.
	// description size padding to 64
	char   s[64-sizeof(long)-sizeof(size_t)-sizeof(_Bool)];
} urn_inum;

bool urn_inum_iszero(urn_inum *i) {
	if (i->intg == 0 && i->frac_ext == 0)
		return true;
	return false;
}

// print inum into s (or internal description pointer)
char *urn_inum_str(urn_inum *i) {
	if (i == NULL) return NULL;
	char *s = i->s;
	if (s[0] != '\0') // cached already.
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
	// Replace 0s in fraction at end with space.
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
	(i->s)[0] = '\0'; // clear desc
	int rv = 0;
	// -0.00456 -> intg=0 intg_s='-0'  frac_s='00456'
	char intg_s[99];
	sscanf(s, "%s.", intg_s);
	i->pstv = (intg_s[0] == '-') ? 0 : 1;

	char frac_s[99];
	rv = sscanf(s, "%ld.%s", &(i->intg), frac_s);
	if (rv == 1) {
		i->frac_ext = 0;
		return 0;
	} else if (rv != 2)
		return EINVAL;

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

int urn_inum_alloc(urn_inum **i, const char *s) {
	URN_RET_ON_NULL(*i = malloc(sizeof(urn_inum)), "Not enough memory", ENOMEM);
	return urn_inum_parse(*i, s);
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

// First data is a 512 pair index
// For each pair, get a swap writing 2 odbk cache [full, dirty] -> [dirty, full]
// on macos the sizeof(urn_odbk_mem) is 2695168, enough to fit in its SHMMAX 4MB
// 	$sysctl kern.sysv.shmmax
// 	kern.sysv.shmmax: 4194304
// 	OR
// 	$sysctl -a | grep shm
// To change SHMMAX on macos, see:
// https://dansketcher.com/2021/03/30/shmmax-error-on-big-sur/
#define urn_odbk_mem_cap 512
typedef struct urn_odbk_mem {
	char pairs[urn_odbk_mem_cap][16];
	urn_odbk odbks[urn_odbk_mem_cap][2];
} urn_odbk_mem;
#define urn_odbk_pid_cap 8
typedef struct urn_odbk_clients {
	char pids_desc[urn_odbk_mem_cap][urn_odbk_pid_cap][64];
	pid_t pids[urn_odbk_mem_cap][urn_odbk_pid_cap];
} urn_odbk_clients;
#define urn_shm_exch_num 9
const char *urn_shm_exchanges[] = {
	"Binance", "BNCM", "BNUM", "Bybit",
	"BybitU", "Coinbase", "FTX", "Kraken",
	"Bittrex",
	"\0"};

int urn_odbk_shm_i(char *exchange) {
	int i = 0;
	while(1) {
		if (urn_shm_exchanges[i][0] == '\0') return -1;
		if (strcasecmp(urn_shm_exchanges[i], exchange) == 0)
			return i;
		i++;
	}
	return -1;
}

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
	}
	if (*shmid == -1)
		URN_RET_ON_RV(errno, "shmget with shmid failed in odbk_shm_init()");

	*shmptr = shmat(*shmid, NULL, 0);
	if (*shmptr == (void *) -1)
		URN_RET_ON_RV(errno, "Unable to attach share memory in odbk_shm_init()");

	if (created) {
		URN_INFO("Cleaning new created urn_odbk_mem");
		// set pair name NULL, mark all odbk dirty.
		for (int i = 0; i < urn_odbk_mem_cap; i++) {
			(*shmptr)->pairs[i][0] = '\0';
			(*shmptr)->odbks[i][0].complete = false;
			(*shmptr)->odbks[i][1].complete = false;
		}
	}
	URN_INFOF("odbk_shm_init() done at key %#08x id %d size %lu ptr %p %c",
		*shmkey, *shmid, desired_sz, *shmptr, writer ? 'W' : 'R');
	return rv;
}

int urn_odbk_shm_write_index(urn_odbk_mem *shmp, char **pair_arr, int len) {
	if (len > urn_odbk_mem_cap && len <= 1) {
		// pair_arr[0] should always be pair name of NULL(no such pair)
		URN_WARNF("odbk_shm_write_index() with illegal len %d", len);
		return ERANGE;
	}
	// mark [0] as useless
	strcpy(shmp->pairs[0], "USELESS");
	for (int i = 1; i <= len; i++)
		strcpy(shmp->pairs[i], pair_arr[i]);
	// set unused pair name NULL
	for (int i = len+1; i < urn_odbk_mem_cap; i++)
		shmp->pairs[i][0] = '\0';
	// Mark all odbk dirty.
	for (int i = 0; i < urn_odbk_mem_cap; i++) {
		shmp->odbks[i][0].complete = false;
		shmp->odbks[i][1].complete = false;
	}
	return 0;
}

int urn_odbk_shm_print(urn_odbk_mem *shmp, int pairid) {
	URN_RET_IF((pairid >= urn_odbk_mem_cap), "pairid too big", ERANGE);
	printf("odbk_shm_init %d [%s] complete: [%d, %d]\n", pairid, shmp->pairs[pairid],
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

	printf("mkt %02lu:%02lu:%02lu.%06ld shm_w +%8.3f ms shm_r +%6.3f ms\n",
			((mkt_ts_e6/1000000) % 86400)/3600,
			((mkt_ts_e6/1000000) % 3600)/60,
			((mkt_ts_e6/1000000) % 60),
			mkt_ts_e6 % 1000000, latency_e3,
			((float)shm_latency_e6)/1000);

	for (int i = 0; i < urn_odbk_mem_cap; i++) {
		if (urn_inum_iszero(&(odbk->bidp[i])) &&
			urn_inum_iszero(&(odbk->bids[i])) &&
			urn_inum_iszero(&(odbk->askp[i])) &&
			urn_inum_iszero(&(odbk->asks[i]))) {
			break;
		}
		printf("%d %24s %24s - %24s %24s\n", i,
			urn_inum_str(&(odbk->bidp[i])),
			urn_inum_str(&(odbk->bids[i])),
			urn_inum_str(&(odbk->askp[i])),
			urn_inum_str(&(odbk->asks[i])));
	}
	printf("odbk_shm_init %d [%s] -- end --\n", pairid, shmp->pairs[pairid]);
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
		for (int i = 0; i < urn_odbk_mem_cap; i++) {
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

int urn_odbk_clients_reg(urn_odbk_clients *shmp, int pairid) {
	pid_t p = getpid();
	for (int j = 0; j < urn_odbk_pid_cap; j++) {
		if (shmp->pids[pairid][j] > 0)
			continue;
		URN_LOGF("Register urn odbk client pairid %d PID %d", pairid, p);
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
