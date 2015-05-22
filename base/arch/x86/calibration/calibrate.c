/*
 * Copyright (C) 2015 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rtai_sem.h>
#include <rtai_lxrt.h>

static int task_period        = 300; /* us */
static int cal_time           = 1;   /* s */
static int cal_tol            = 50;  /* ns */
static int max_cal_iterations = 20;
static int ret_time_loops     = 1000;

static inline RTIME rt_get_time_in_usrspc(void)
{
#ifdef __i386__
        unsigned long long t;
        __asm__ __volatile__ ("rdtsc" : "=A" (t));
       return t;
#else
        union { unsigned int __ad[2]; RTIME t; } t;
        __asm__ __volatile__ ("rdtsc" : "=a" (t.__ad[0]), "=d" (t.__ad[1]));
        return t.t;
#endif
}

#define RTAI_CONFIG_PATH  "../../../../rtai_config.h"

static void latency_calibrated(void)
{
	FILE *si;
	char *line = NULL;
	size_t len = 0;

	si = fopen(RTAI_CONFIG_PATH, "r");

	while (getline(&line, &len, si) > 0) {
		if (!strncmp(line, "#define CONFIG_RTAI_SCHED_LATENCY", sizeof("#define CONFIG_RTAI_SCHED_LATENCY") - 1)) {
			int sched_latency = atoi(line + sizeof("#define CONFIG_RTAI_SCHED_LATENCY"));
			if (sched_latency > 1) {
				printf("* SCHED_LATENCY IS CALIBRATED: %d (ns). *\n",sched_latency); 
				exit(1);
			}
		}  
		free(line);
		line = NULL;
	}

	fclose(si);
	return;
}

#define DOT_RTAI_CONFIG_PATH  "../../../../.rtai_config"

static void set_calibrated_macros(int sched_latency, int ret_time, int argc)
{
if (argc == 1) {
	FILE *si, *so;
	char *line = NULL;
	size_t len = 0;

// brute force, rtai_config.h comes first .....

	si = fopen(RTAI_CONFIG_PATH, "r");
	so = fopen("tmp", "w+");

	while (getline(&line, &len, si) > 0) {
		if (!strncmp(line, "#define CONFIG_RTAI_SCHED_LATENCY", sizeof("#define CONFIG_RTAI_SCHED_LATENCY") - 1)) {
			fprintf(so, "#define CONFIG_RTAI_SCHED_LATENCY %d\n", sched_latency);
			goto cont1;
		}  
#if 0
		if (!strncmp(line, "#define CONFIG_RTAI_BUSY_TIME_ALIGN", sizeof("#define CONFIG_RTAI_BUSY_TIME_ALIGN") - 1)) {
			fprintf(so, "#define CONFIG_RTAI_BUSY_TIME_ALIGN 1\n");
			goto cont1;
		}
#endif
		if (!strncmp(line, "#define CONFIG_RTAI_KERN_BUSY_ALIGN_RET_DELAY", sizeof("#define CONFIG_RTAI_KERN_BUSY_ALIGN_RET_DELAY") - 1)) {
			fprintf(so, "#define CONFIG_RTAI_KERN_BUSY_ALIGN_RET_DELAY %d\n", ret_time/2);
			goto cont1;
		}  
		if (!strncmp(line, "#define CONFIG_RTAI_USER_BUSY_ALIGN_RET_DELAY", sizeof("#define CONFIG_RTAI_USER_BUSY_ALIGN_RET_DELAY") - 1)) {
			fprintf(so, "#define CONFIG_RTAI_USER_BUSY_ALIGN_RET_DELAY %d\n", -ret_time);
			goto cont1;
		}  
		fprintf(so, "%s", line);
cont1:
		free(line);
		line = NULL;
	}

	fclose(si);
	fclose(so);
	system("mv tmp "RTAI_CONFIG_PATH);

// ..... then .rtai_config.

	si = fopen(DOT_RTAI_CONFIG_PATH, "r");
	so = fopen("tmp", "w+");

	while (getline(&line, &len, si) > 0) {
		if (!strncmp(line, "CONFIG_RTAI_SCHED_LATENCY", sizeof("CONFIG_RTAI_SCHED_LATENCY") - 1)) {
			fprintf(so, "CONFIG_RTAI_SCHED_LATENCY=\"%d\"\n", sched_latency);
			goto cont2;
		}  
#if 0
//		if (!strncmp(line, "CONFIG_RTAI_BUSY_TIME_ALIGN", sizeof("CONFIG_RTAI_BUSY_TIME_ALIGN") - 1)) {
		if (strstr(line, "CONFIG_RTAI_BUSY_TIME_ALIGN")) {
			fprintf(so, "CONFIG_RTAI_BUSY_TIME_ALIGN=y\n");
			goto cont2;
		}  
#endif
		if (!strncmp(line, "CONFIG_RTAI_KERN_BUSY_ALIGN_RET_DELAY", sizeof("CONFIG_RTAI_KERN_BUSY_ALIGN_RET_DELAY") - 1)) {
			fprintf(so, "CONFIG_RTAI_KERN_BUSY_ALIGN_RET_DELAY=\"%d\"\n", ret_time/2);
			goto cont2;
		}  
		if (!strncmp(line, "CONFIG_RTAI_USER_BUSY_ALIGN_RET_DELAY", sizeof("CONFIG_RTAI_USER_BUSY_ALIGN_RET_DELAY") - 1)) {
			fprintf(so, "CONFIG_RTAI_USER_BUSY_ALIGN_RET_DELAY=\"%d\"\n", -ret_time);
			goto cont2;
		}  
		fprintf(so, "%s", line);
cont2:
		free(line);
		line = NULL;
	}

	fclose(si);
	fclose(so);
	system("mv tmp "DOT_RTAI_CONFIG_PATH);
} else {
	printf("SUMMARY %d %d %d\n", sched_latency, ret_time/2, -ret_time);
}

	return;
}

static int user_latency;
static RT_TASK *calmng;

static int user_calibrator(long loops)
{
	RT_TASK *task;
	RTIME resume_time;

 	if (!(task = rt_thread_init(nam2num("USRCAL"), 0, 0, SCHED_FIFO, 0xF))) {
		printf("*** CANNOT INIT USER LATENCY CALIBRATOR TASK ***\n");
		return 1;
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	resume_time = rt_get_time_in_usrspc() + 10*task_period;
	rt_task_make_periodic(task, resume_time, task_period);
	while (loops--) {
		resume_time += task_period;
		rt_task_wait_period();
		user_latency += rt_get_time_in_usrspc() - resume_time;
	}

	rt_task_resume(calmng);
	rt_make_soft_real_time();
	rt_thread_delete(task);
	return 0;
}

static inline int sign(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

int main(int argc, char *argv[])
{
	int loops, UserLatency = cal_tol;
	RTIME tret, t = 0;

	if (argc > 1) {
		int len;
		char *cmd;
		const char *module_path = argv[1];
		// struct stat st;
		// stat(argv[1], &st);
		// by now, we assume it's alright

		len = sizeof("/sbin/insmod /rtai_hal" HAL_SCHED_MODEXT " >/dev/null 2>&1") + strlen(module_path);
		cmd = malloc(len + 1);
		snprintf(cmd, len + 1, "/sbin/insmod %s/rtai_hal" HAL_SCHED_MODEXT " >/dev/null 2>&1", module_path);
		system(cmd);
		free(cmd);
		system("/sbin/insmod ./rtai_tmpsched" HAL_SCHED_MODEXT " >/dev/null 2>&1");

	} else {
		latency_calibrated();

		system("/sbin/insmod \"" HAL_SCHED_PATH "\"/rtai_hal" HAL_SCHED_MODEXT " >/dev/null 2>&1");
		system("/sbin/insmod \"" HAL_SCHED_PATH "\"/rtai_sched" HAL_SCHED_MODEXT " >/dev/null 2>&1");
	}

 	if (!(calmng = rt_thread_init(nam2num("CALMNG"), 10, 0, SCHED_FIFO, 0xF)) ) {
		printf("*** CANNOT INIT CALIBRATION TASK ***\n");
		return 1;
	}
	printf("\n* CALIBRATING SCHEDULING LATENCIES FOR %d (s), PERIOD %d (us). *\n", cal_time, task_period);
	loops = (cal_time*1000000 + task_period/2)/task_period;

	start_rt_timer(0);
	task_period = nano2count(1000*task_period);

	do {
		kernel_calibrator(task_period, loops, -UserLatency);
		rt_thread_create((void *)user_calibrator, (void *)loops, 0);
		rt_task_suspend(calmng);

		user_latency = (user_latency + loops/2)/loops;
		user_latency = sign(user_latency)*count2nano(abs(user_latency));
		UserLatency += user_latency;
		printf("* %d <> SCHED LATENCY - VARIATION: %d (ns), TOTAL: %d (ns). *\n", max_cal_iterations, user_latency, UserLatency);
	} while (abs(user_latency) > cal_tol && --max_cal_iterations > 0);

	UserLatency += 100;
	UserLatency -= UserLatency%100;
	printf("* SCHED LATENCY - USED: %d (ns). *\n", UserLatency);

	tret = rt_get_time_in_usrspc();
	for (loops = 0; loops < ret_time_loops; loops++) {
		t += rt_get_time();
	}
	tret = rt_get_time_in_usrspc() - tret;
	tret = count2nano(tret/ret_time_loops);

	printf("\n* CALIBRATED RETURN TIME FOR %d CYCLES: %lld (ns). *\n", ret_time_loops, tret);
	tret += 100;
	tret -= tret%100;
	printf("* CALIBRATED RETURN TIME - USED: %lld (ns). *\n", tret);

	stop_rt_timer();
	rt_thread_delete(NULL);
	printf("\n");

	if (argc > 1) {
		system("/sbin/rmmod rtai_tmpsched >/dev/null 2>&1");
	} else {
		system("/sbin/rmmod rtai_sched >/dev/null 2>&1");
	}
	system("/sbin/rmmod rtai_hal >/dev/null 2>&1");

	if (max_cal_iterations > 1) {
		set_calibrated_macros(UserLatency, tret, argc);
	} else {
		exit(1);
	}

	return 0;
}
