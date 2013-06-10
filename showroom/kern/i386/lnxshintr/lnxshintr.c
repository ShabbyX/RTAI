#include <linux/module.h>

#include <asm/rtai.h>

int ETHIRQ = 11;
RTAI_MODULE_PARM(ETHIRQ, int);

static void handler(int irq, void *something)
{
	static int cnt;
	rt_pend_linux_irq(ETHIRQ);
	rt_printk("RTAI IRQ: %d %d %d %d - ", ++cnt, irq, ETHIRQ, rtai_cpuid());
}

static int linux_post_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	static int cnt;
	rt_enable_irq(ETHIRQ);
	rt_printk("LINUX IRQ: %d %d %d %d\n", ++cnt, irq, ETHIRQ, rtai_cpuid());
	return 1;
}

int init_module(void)
{
	rt_request_linux_irq(ETHIRQ, linux_post_handler, "LINUX_POST_HANDLER", linux_post_handler);
	rt_set_irq_ack(ETHIRQ, (void *)rt_disable_irq);
	rt_request_irq(ETHIRQ, (void *)handler, NULL, 0);
	return 0;
}

void cleanup_module(void)
{
	rt_release_irq(ETHIRQ);
	rt_free_linux_irq(ETHIRQ, linux_post_handler);
}
