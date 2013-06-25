
#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/io.h>

#include <rtai_nam2num.h>
#include <rtai_sched.h>
#include <rtai_scb.h>

#include "pcsp_tables.h"

MODULE_LICENSE("GPL");

//#define CONFIG_X86_64

#define SCBSUPRT  USE_GFP_ATOMIC
#define SCBSIZ    2000

#define TICK_PERIOD 25000	/* 40 khz */
#define DIVISOR 5

#define STACK_SIZE 4000

/* You can make this bigger, but then you start to get
 * clipping, which sounds bad.  29 is good.
 */
#define VOLUME  30

static RT_TASK thread;

static int cpu_used[NR_RT_CPUS];

static unsigned char vl_tab[256];

static int port61;

static void *scb;

static volatile int end;

#define PORT_ADR 0x61

static int filter(int x)
{
	static int oldx;
	int ret;

	if (x & 0x80) {
		x = 382 - x;
	}
	ret = x > oldx;
	oldx = x;
	return ret;
}

static void intr_handler(long t)
{
	char data, temp;
	int go = 0;
	int divisor = DIVISOR;

	while (!end) {
		if (!(--divisor)) {
			divisor = DIVISOR;
			cpu_used[hard_cpu_id()]++;
			go = !rt_scb_get(scb, &data, 1);
		} else {
			go = 0;
		}
		if (go) {
#ifdef CONFIG_X86_64
			data = filter(data);
			temp = inb(PORT_ADR);
			temp &= 0xfd;
			temp |= (data & 1) << 1;
			outb(temp, PORT_ADR);
#else
			outb(port61, 0x61);
			outb(port61^1, 0x61);
			outb(vl_tab[((unsigned int)data)&0xff], 0x42);
#endif
		}
		rt_task_wait_period();
	}
}

int init_module(void)
{
	int i;

	outb_p(0x92, 0x43);  /* binary, mode1, LSB only, ch 2 */
	for (i = 0; i < 256; vl_tab[i] = 1 + ((VOLUME*ulaw[i]) >> 8), i++);
	port61 = inb(0x61) | 0x3;

	scb = rt_scb_init(nam2num("SCB"), SCBSIZ, SCBSUPRT);
	rt_task_init(&thread, intr_handler, 0, STACK_SIZE, 0, 0, 0);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	rt_task_make_periodic_relative_ns(&thread, 10000000, TICK_PERIOD);

	return 0;
}


void cleanup_module(void)
{
	int cpuid;
	end = 1;
	stop_rt_timer();
	rt_task_delete(&thread);
	rt_scb_delete(nam2num("SCB"));
	printk("\n\nCPU USE SUMMARY\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d\n", cpuid, cpu_used[cpuid]);
	}
	printk("END OF CPU USE SUMMARY\n\n");
}
