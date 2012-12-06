#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>

#include <asm/io.h>

#include <rtai_nam2num.h>
#include <rtai_sched.h>
#include <rtai_scb.h>
#include "params.h"

MODULE_LICENSE("GPL");

#define SCBSUPRT  USE_GFP_ATOMIC
#define SCBSIZ    (2*BUFSIZE*sizeof(int))

#define STACK_SIZE 16*1024

static RT_TASK thread;

static void *scb;

static void intr_handler(long t)
{
	unsigned int i, cnt, n = 0;
	unsigned int data[BUFSIZE];

	while (1) {
		if ((cnt = randu(BUFSIZE)) > 0) {
			while (rt_scb_evdrp(scb, data, cnt*sizeof(int))) {
				rt_sleep(nano2count(SLEEP_TIME));
			}
			rt_scb_get(scb, data, cnt*sizeof(int));
			for (i = 0; i < cnt; i++) {
				if (data[i] != ++n) {
					rt_printk("K >>> %u\n", n);
				}
			}
		}
	}
}

int init_module(void)
{
	unsigned cnt;
	while (!(cnt = randu(BUFSIZE)));
	while (--cnt) {
		randu(BUFSIZE);
	}
	scb = rt_scb_init(nam2num("SCB"), SCBSIZ, SCBSUPRT);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	rt_task_init(&thread, intr_handler, 0, STACK_SIZE, 0, 1, 0);
	rt_task_resume(&thread);

	return 0;
}


void cleanup_module(void)
{
	stop_rt_timer();
	rt_task_delete(&thread);
	rt_scb_delete(nam2num("SCB"));
}
