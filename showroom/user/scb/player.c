#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/mman.h>

#include <rtai_nam2num.h>
#include <rtai_lxrt.h>
#include <rtai_scb.h>
#include "params.h"

static volatile int end;
static void endme(int dummy) { end = 1; }

#define LCBSIZ  (2*BUFSIZE*sizeof(int) + HDRSIZ)

#if 0
/* another way of sharing a preassigned local circular buffer */
static unsigned int lcbuf[LCBSIZ];
static void *lcb;
#endif

static void *thread_fun(void *lcb)
{
	unsigned int i, cnt, n = 0;
	unsigned int data[BUFSIZE];

	rt_thread_init(nam2num("THREAD"), 0, 0, SCHED_FIFO, 0xF);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	while (!end) {
		if ((cnt = randu(BUFSIZE)) > 0) {
			while (rt_scb_evdrp(lcb, data, cnt*sizeof(int))) {
				rt_sleep(nano2count(SLEEP_TIME));
			}
			rt_scb_get(lcb, data, cnt*sizeof(int));
			for (i = 0; i < cnt; i++) {
				if (data[i] != ++n) {
					rt_printk("U >>> %u\n", n);
				}
			}
		}
	}

	rt_make_soft_real_time();
	rt_task_delete(NULL);

	return 0;
}

static pthread_t thread;

int main(void)
{
	unsigned int lcbuf[LCBSIZ];
	unsigned int i, cnt, n = 0;
	unsigned int data[BUFSIZE];
	void *scb, *lcb;

	signal(SIGINT,  endme);
	signal(SIGTERM, endme);

	rt_thread_init(nam2num("MAIN"), 1, 0, SCHED_FIFO, 0xF);
	scb = rt_scb_init(nam2num("SCB"), 0, 0);
	lcb = rt_scb_init(nam2num("LCB"), LCBSIZ, (unsigned long)lcbuf);
	thread = rt_thread_create(thread_fun, lcb, 0);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	while (!end) {
		if ((cnt = randu(BUFSIZE)) > 0) {
			for (i = 0; i < cnt; i++) {
				data[i] = ++n;
			}
			printf("TOT = %u, UCNT = %d\n", n, cnt);
			while (rt_scb_put(scb, data, cnt*sizeof(int))) {
				rt_sleep(nano2count(SLEEP_TIME));
			}
			while (rt_scb_put(lcb, data, cnt*sizeof(int))) {
				rt_sleep(nano2count(SLEEP_TIME));
			}
		}
	}

	rt_make_soft_real_time();
	rt_task_delete(NULL);
	rt_thread_join(thread);
	rt_scb_delete(nam2num("SCB"));
	rt_scb_delete(nam2num("LCB"));

	return 0;
}
