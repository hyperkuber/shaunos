/*
 * time.c
 *
 *  Created on: Jan 7, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/time.h>
#include <asm/io.h>
#include <kernel/types.h>
#include <kernel/timer.h>
#include <kernel/kthread.h>


#define CMOS_ADDR	0x70
#define CMOS_DATA	0x71

#define MINUTE 60
#define HOUR ( 60 * MINUTE )
#define DAY  ( 24 * HOUR )
#define YEAR ( 365 * DAY )
//yeah, it is 2013...
//it has been one year long since i launched my project
#define CURRENT_YEAR	2013

struct timespec xtime;


static long time_zone = -8 * 60 * 60;


static int month [ 12 ] = {
    0,
    DAY * ( 31 ),
    DAY * ( 31 + 29 ),
    DAY * ( 31 + 29 + 31 ),
    DAY * ( 31 + 29 + 31 + 30 ),
    DAY * ( 31 + 29 + 31 + 30 + 31 ),
    DAY * ( 31 + 29 + 31 + 30 + 31 + 30 ),
    DAY * ( 31 + 29 + 31 + 30 + 31 + 30 + 31),
    DAY * ( 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),
    DAY * ( 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
    DAY * ( 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 ),
    DAY * ( 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 )
};

typedef struct rtctime {
	unsigned char second;
	unsigned char minute;
	unsigned char hour;
	unsigned char day;
	unsigned char month;
	unsigned int year;
} rtctime_t;

unsigned char rtc_read_reg(int reg)
{
	outb(CMOS_ADDR, (uchar_t)reg);
	return inb(CMOS_DATA);
}

int cmos_uip(void)
{
	outb(CMOS_ADDR, 0x0A);
	return (inb(CMOS_DATA) & 0x80);
}

void read_rtc(rtctime_t *time)
{

	unsigned char registerB;

	while (cmos_uip());
	do {
		time->second = rtc_read_reg(0x00);
		time->minute = rtc_read_reg(0x02);
		time->hour = rtc_read_reg(0x04);
		time->day = rtc_read_reg(0x07);
		time->month = rtc_read_reg(0x08);
		time->year = rtc_read_reg(0x09);
	} while (time->second != rtc_read_reg(0x00));



	registerB = rtc_read_reg(0x0B);

	if (!(registerB & 0x04)){
		time->second = ((time->second & 0x0F) + time->second / 16 * 10);
		time->minute = ((time->minute & 0x0F) + time->minute / 16 * 10);
		time->hour = ( (time->hour & 0x0F) + (((time->hour & 0x70) / 16) * 10) ) | (time->hour & 0x80);
		time->day = (time->day & 0x0F) + ((time->day / 16) * 10);
		time->month = (time->month & 0x0F) + ((time->month / 16) * 10);
		time->year = (time->year & 0x0F) + ((time->year / 16) * 10);
	}

	time->year += (CURRENT_YEAR / 100) * 100;
    if(time->year < CURRENT_YEAR)
    	time->year += 100;

}


time_t make_time(rtctime_t *time)
{
	time_t res;
	int year;
	if(time->year < 70)
		time->year += 100;
	year = time->year;

	res = YEAR * year + DAY * ((year + 1) / 4);
	res += month[time->month - 1];
	if(time->month - 1 > 1 && ((year + 2) % 4)) {
		res -= DAY;
	}

	res += DAY * (time->day - 1);
	res += HOUR * time->hour;
	res += MINUTE * time->minute;
	res += time->second;
	return res;
}

static inline unsigned long
mktime (unsigned int year, unsigned int mon,
	unsigned int day, unsigned int hour,
	unsigned int min, unsigned int sec)
{
	if (0 >= (int) (mon -= 2)) {	/* 1..12 -> 11,12,1..10 */
		mon += 12;		/* Puts Feb last since it has leap day */
		year -= 1;
	}

	return (((
		(unsigned long) (year/4 - year/100 + year/400 + 367*mon/12 + day) +
			year*365 - 719499
	    )*24 + hour /* now have hours */
	  )*60 + min /* now have minutes */
	)*60 + sec + time_zone; /* finally seconds */

}


unsigned long get_cmos_time()
{
	rtctime_t currtime = {0};
	read_rtc(&currtime);
	LOG_INFO("year:%d month:%d day:%d hour:%d minute:%d second:%d",
			currtime.year, currtime.month, currtime.day,
			currtime.hour, currtime.minute, currtime.second);
	return mktime(currtime.year, currtime.month, currtime.day, currtime.hour, currtime.minute,
			currtime.second);
}


unsigned long long sys_get_ticks(void)
{
	extern unsigned long long ticks;
	return ticks;
}

clock_t ktimes(struct tms *ts)
{
	unsigned long utime = current->utime;
	unsigned long stime = utime + current->ktime;
	struct tms t;
	t.tms_utime = utime;
	t.tms_stime = stime;
	t.tms_cutime = 0;	//all the child process's user time
	t.tms_cstime = 0;

	if (ts){
		copy_to_user((char *)ts,(char *)&t, sizeof(struct tms));
	}
	return (clock_t)sys_get_ticks();
}

int kgettimeofday(struct timeval *tv, void *tz)
{
	struct timeval tval;
	tval.tv_sec = xtime.ts_sec;
	tval.tv_usec = xtime.ts_nsec / 1000;

	if (tv){
		copy_to_user((char *)tv, (char *)&tval, sizeof(struct timeval));
	}

	if (tz){
		copy_to_user((char *)tz, (char *)&time_zone, sizeof(time_zone));
	}

	return 0;
}

int ksettimeofday(struct timeval *tv, void *tz)
{

	if (tv) {
		xtime.ts_sec = tv->tv_sec;
		xtime.ts_nsec = tv->tv_usec * 1000;
	}

	if (tz)
		time_zone = *(long *)tz;

	return 0;
}

int ti_get_timeval(struct timeval *tv)
{

	if (tv){
		tv->tv_sec = xtime.ts_sec;
		tv->tv_usec = xtime.ts_nsec / 1000;
		return 0;
	}
	return -EINVAL;
}


void time_init(void)
{
	xtime.ts_sec = get_cmos_time();
	xtime.ts_nsec = 0;
}



