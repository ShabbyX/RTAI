#include <linux/module.h>

#include <asm/rtai.h>

#define MAX_NIRQ 10
static int nirq;
static int irq[] = {1, 12, 19};
static int level[] = {0, 0, 0};
static int cnt[] = {0, 0, 0};
static char post_handler[MAX_NIRQ][20];
static void *dev_id[MAX_NIRQ];

static void rtai_handler(int irqa, void *idx)
{
	int i;
	i = (int)idx;
	cnt[i]++;	
	rt_pend_linux_irq(irq[i]);
}

static int linux_post_handler(int irqa, void *dev_id, struct pt_regs *regs)
{
	int i = (int)dev_id;
	if (level[i-1]) rt_enable_irq(irqa);
	rt_printk("LINUX IRQ %d: %d %d %d\n", i, cnt[i-1], irqa, irq[i-1]);
	return 1;
}

int init_module(void)
{
	int i;
	nirq = sizeof(irq)/sizeof(int);
	if (nirq >= MAX_NIRQ) {
		printk("SET MAX_NIRQ MACRO AT LEAST TO %d.\n", nirq);
		return 1;
	}
	for (i = 0; i < nirq; i++) {
		snprintf(post_handler[i], sizeof(post_handler[i]), "POST_HANDLER%d", i + 1);
		dev_id[i] = (void *)(i+1);
		rt_request_linux_irq(irq[i], linux_post_handler, post_handler[i], dev_id[i]);
		if (level[i]) rt_set_irq_ack(irq[i], (void *)rt_disable_irq);
		rt_request_irq(irq[i], (void *)rtai_handler, (void *)i, 0);
	}
	return 0;
}

void cleanup_module(void)
{
	int i;
	for (i = 0; i < nirq; i++) {
		rt_release_irq(irq[i]);
		rt_free_linux_irq(irq[i], dev_id[i]);
	}
}

