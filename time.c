#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

char *time_stamp(){
	char *timestamp = (char *)malloc(sizeof(char) * 64);
	time_t ltime;
	ltime=time(NULL);
	struct tm *tm;
	tm=localtime(&ltime);

	sprintf(timestamp, "%04d-%02d-%02d %02d:%02d:%02d",
			tm->tm_year+1900, tm->tm_mon, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	return timestamp;
}

struct timeval global_log_tv;
#define DEBUG(msg) gettimeofday(&global_log_tv, NULL), \
	fprintf (stdout, "DEBUG %5lu.%6u %s:%d %s(): %s\n", \
			global_log_tv.tv_sec % 100000, \
			global_log_tv.tv_usec, \
			__FILE__, __LINE__, __FUNCTION__,  msg);

int main(){
	printf(" Timestamp:[%s]\n",time_stamp());

	DEBUG("STEP0")
	struct timeval *tp;
	printf(" sizeof(sec)  : %lu\n", sizeof(tp->tv_sec));
	printf(" sizeof(usec) : %ld\n", sizeof(tp->tv_usec));
	long unsigned sizeoftp = sizeof(*tp);
	printf(" malloc   : %lu\n", sizeoftp);
	tp = malloc(sizeoftp);
	gettimeofday(tp, NULL);
	printf(" timeval  : %ld.%6d\n", tp->tv_sec, tp->tv_usec);
	free(tp);

	struct timespec *ts;
	printf(" sizeof(sec)  : %lu\n", sizeof(ts->tv_sec));
	printf(" sizeof(nsec) : %ld\n", sizeof(ts->tv_nsec));
	long unsigned sizeofts = sizeof(*ts);
	printf(" malloc   : %lu\n", sizeofts);
	ts = malloc(sizeofts);
	// clock_gettime(CLOCK_REALTIME, ts); // same as gettimeofday()
	clock_gettime(CLOCK_MONOTONIC_RAW, ts);
	printf(" timesepc : %ld.%9ld\n", ts->tv_sec, ts->tv_nsec);
	free(ts);
	return 0;
}
