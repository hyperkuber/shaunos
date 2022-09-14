/*
 * time.h
 *
 *  Created on: Jan 8, 2013
 *      Author: root
 */

#ifndef SHAUN_TIME_H_
#define SHAUN_TIME_H_

#include <kernel/types.h>


struct timespec {
	time_t	ts_sec;		/* seconds */
	long	ts_nsec;	/* nanoseconds */
};


struct timeval {
	time_t		tv_sec;		/* seconds */
	long	tv_usec;	/* microseconds */
};

struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};


struct tms {
    clock_t tms_utime;  /* user time */
    clock_t tms_stime;  /* system time */
    clock_t tms_cutime; /* user time of children */
    clock_t tms_cstime; /* system time of children */
};

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2

struct	itimerval {
	struct	timeval it_interval;	/* timer interval */
	struct	timeval it_value;	/* current value */
};

#ifndef USEC_PER_SEC
#define USEC_PER_SEC (1000000L)
#endif

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC (1000000000L)
#endif

#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC (1000L)
#endif


void time_init(void);
unsigned long get_cmos_time();

extern struct timespec xtime;

unsigned long long sys_get_ticks(void);
clock_t ktimes(struct tms *ts);
extern int kgettimeofday(struct timeval *tv, void *tz);
extern int ti_get_timeval(struct timeval *tv);
extern int ksetitimer(int which, struct itimerval *newval, struct itimerval *oldval);
extern int ksettimeofday(struct timeval *tv, void *tz);
#endif /* SHAUN_TIME_H_ */


