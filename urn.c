#include "urn.h"

#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

//////////////////////////////////////////
// Double tools
//////////////////////////////////////////
double urn_round(double f, int precision) {
	if (f == 0)
		return 0;
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

char   urn_global_log_buf[1024*1024]; // 64K was small for large log

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
// Function to trim leading and trailing spaces from a string
char *urn_s_trim(const char* str, char* new_s) {
	const char* end;

	// Trim leading space
	while(isspace((unsigned char)*str)) str++;

	if(*str == 0) { // All spaces?
		*new_s = 0;
		return new_s;
	}

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace((unsigned char)*end)) end--;

	// Set output string length
	end++;
	size_t len = end - str;

	strncpy(new_s, str, len);
	new_s[len] = '\0';
	return new_s;
}

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

/* multiply m into i */
void urn_inum_mul(urn_inum *i, double m) {
	if (m == 0) {
		i->intg = 0;
		i->frac_ext = 0;
		i->pstv = false;
		return;
	}

	double total_i = urn_inum_to_db(i);
	urn_inum_from_db(i, total_i * m);
}

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
int urn_tick_get_ts_e6(urn_ticks *ticks, int idx, unsigned long *ts_e6) {
	if (idx >= URN_TICK_LENTH) return ERANGE;
	unsigned short i = (ticks->latest_idx + URN_TICK_LENTH - idx) % URN_TICK_LENTH;
	if (i > ticks->latest_idx && i < ticks->oldest_idx) return ERANGE;
	if (ticks->tickts_e6[i] == 0) return ERANGE;
	*ts_e6 = ticks->tickts_e6[i];
	return 0;
}
int urn_sprintf_tick_json(urn_ticks *ticks, int idx, char *s) {
	if (idx >= URN_TICK_LENTH) return 0;
	unsigned short i = (ticks->latest_idx + URN_TICK_LENTH - idx) % URN_TICK_LENTH;
	if (i > ticks->latest_idx && i < ticks->oldest_idx) return 0;
	if (ticks->tickts_e6[i] == 0) return 0;
	int ct = 0;
	ct += sprintf(s+ct, "{\"p\":");
	ct += urn_inum_sprintf(&(ticks->tickp[i]), s+ct);
	ct += sprintf(s+ct, ",\"s\":");
	ct += urn_inum_sprintf(&(ticks->ticks[i]), s+ct);
	if (ticks->tickB[i])
		ct += sprintf(s+ct, ",\"T\":\"buy\"");
	else
		ct += sprintf(s+ct, ",\"T\":\"sell\"");
	ct += sprintf(s+ct, ",\"t\":%lu}", ticks->tickts_e6[i] / 1000);
	return ct;
}

const char *urn_shm_exchanges[] = { urn_shm_exch_list };
/* linearly find preset share memory index from constant urn_shm_exch_list, should cache result */
int urn_odbk_shm_i(char *exchange) {
	for (int i = 0; i < urn_shm_exch_num && urn_shm_exchanges[i][0] != '\0'; i++)
		if (strcasecmp(urn_shm_exchanges[i], exchange) == 0)
			return i;
	return -1;
}

/* Attach SHMEM, reset data, reset pairs index */
int urn_odbk_shm_init(bool writer, char *exchange, urn_odbk_mem **shmptr) {
	int rv = 0;
	key_t tmp_shmkey = 0;
	key_t *shmkey = &tmp_shmkey;
	int tmp_shmid = 0;
	int *shmid = &tmp_shmid;

	// SHMKEY starts from 0xA001, return -1 if not found.
	*shmkey = urn_odbk_shm_i(exchange);
	if (*shmkey < 0)
		URN_RET_ON_RV(EINVAL, "Unknown exchange in odbk_shm_init()");
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

int urn_odbk_shm_print_inline(urn_odbk_mem *shmp, int pairid, char *line) {
	URN_RET_IF((pairid >= URN_ODBK_MAX_PAIR), "pairid too big", ERANGE);
	urn_odbk *odbk = NULL;
	if (shmp->odbks[pairid][0].complete) {
		odbk = &(shmp->odbks[pairid][0]);
	} else if (shmp->odbks[pairid][1].complete) {
		odbk = &(shmp->odbks[pairid][1]);
	}
	if (odbk == NULL)
		return EEXIST;
	line += sprintf(line, "%lu,%lu,", odbk->w_ts_e6, odbk->mkt_ts_e6);
	char buffer[256];
	for (int i = 0; i < 5; i++)
		line += sprintf(line, "%s,", urn_s_trim(urn_inum_str(&(odbk->bidp[i])), buffer));
	for (int i = 0; i < 5; i++)
		line += sprintf(line, "%s,", urn_s_trim(urn_inum_str(&(odbk->askp[i])), buffer));
	for (int i = 0; i < 5; i++)
		line += sprintf(line, "%s,", urn_s_trim(urn_inum_str(&(odbk->bids[i])), buffer));
	for (int i = 0; i < 5; i++)
		line += sprintf(line, "%s,", urn_s_trim(urn_inum_str(&(odbk->asks[i])), buffer));
	urn_ticks *ticks = &(shmp->ticks[pairid]);
	bool buy = false;
	urn_inum p, s;
	unsigned long ts_e6 = 0;
	if (urn_tick_get(ticks, 0, &buy, &p, &s, &ts_e6) == 0) {
		line += sprintf(line, "%s,%s,", (buy ? "B" : "S"), urn_s_trim(urn_inum_str(&p), buffer));
		line += sprintf(line, "%s,", urn_s_trim(urn_inum_str(&s), buffer));
		line += sprintf(line, "%lu\n", ts_e6);
	} else {
		line += sprintf(line, ",,,\n");
	}
	return 0;
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
