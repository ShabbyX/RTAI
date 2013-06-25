#include <linux/module.h>
#include <asm/uaccess.h>

MODULE_LICENSE("GPL");

#include <asm/rtai_hal.h>

#define ALLOWED_CPUS  1

static volatile int tmrcnt;

static void tmrisr(int irq)
{
	int cnt;
	outb(0x00, 0x43);
	cnt = inb(0x40);
	cnt |= (inb(0x40) << 8);
	if (cnt > tmrcnt) {
		tmrcnt = cnt;
	}
	rt_pend_linux_irq(irq);
}

int init_rtai_proxy( void )
{
	rt_assign_irq_to_cpu(TIMER_IRQ, ALLOWED_CPUS);
	rt_request_global_irq(TIMER_IRQ, (void *)tmrisr);
	return 0;
}

void cleanup_rtai_proxy( void )
{
	rt_free_global_irq(TIMER_IRQ);
	rt_reset_irq_to_sym_mode(TIMER_IRQ);
	printk("MAX INTERRUPT LATENCY: %d\n", tmrcnt);
}

module_init(init_rtai_proxy);
module_exit(cleanup_rtai_proxy);
