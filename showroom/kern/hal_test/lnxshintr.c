#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <asm/rtai.h>
#include <rtai_wrappers.h>

#define NIRQ 10
static int nirq;
#ifdef CONFIG_SMP
// on one of my computers: timer (8254), keyboard, mouse, ethernet
static int irq[] = {0, 1, 12, 19};
#else
// on one of my computers: timer (8254), keyboard, ethernet, mouse
static int irq[] = {0, 1, 10, 12};
#endif
static int cnt[] = {0, 0, 0, 0};
static int diag[] = {0, 0, 0, 0};
static char post_handler[NIRQ][20];
static void *dev_id[NIRQ];

static void rtai_handler(int irqa, void *idx)
{
	int i;
	i = (long)idx;
	cnt[i]++;	
	rt_pend_linux_irq(irq[i]);
}

static int linux_post_handler(int irqa, void *dev_id, struct pt_regs *regs)
{
	int i;
	i = (long)dev_id;
	if (diag[i-1]) {
		rt_printk("# %d: LINUX IRQ %d/%d: COUNT: %d.\n", i, irqa, irq[i-1], cnt[i-1]);
	}
	return 1;
}

int init_module(void)
{
	int i;
	nirq = sizeof(irq)/sizeof(int);
	if (nirq >= NIRQ) {
		return 1;
	}
	for (i = 0; i < nirq; i++) {
		snprintf(post_handler[i], sizeof(post_handler[i]), "POST_HANDLER%d", i + 1);
		dev_id[i] = (void *)(long)(i+1);
		rt_request_linux_irq(irq[i], linux_post_handler, post_handler[i], dev_id[i]);
		rt_request_irq(irq[i], (void *)rtai_handler, (void *)(long)i, 0);
	}
	return 0;
}

void cleanup_module(void)
{
	int i;
	for (i = 0; i < nirq; i++) {
		rt_release_irq(irq[i]);
		rt_free_linux_irq(irq[i], dev_id[i]);
		rt_printk("LINUX IRQ %d: COUNT: %d.\n", irq[i], cnt[i]);
	}
}

