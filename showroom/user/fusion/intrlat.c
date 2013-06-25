#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");

#define IS_RTAI

#ifdef IS_RTAI

#include <asm/rtai_hal.h>

#define ENABLE_IRQ  1
#define PEND_IRQ    2

struct intr_s { int irq; void *isr; unsigned long cnt; void *cookie; };
typedef struct intr_s xnintr_t;

int irq_handler(int, xnintr_t *);

static inline void xnintr_init(xnintr_t *intr, int irq, void *isr, void *iack, unsigned long flags)
{
	*intr = (xnintr_t){ irq, isr, 0, 0 };
}

static inline void xnintr_attach(xnintr_t *intr, void *cookie)
{
	intr->cookie = cookie;
	rt_request_irq(intr->irq, (void *)irq_handler, intr, 0);
}

static inline void xnintr_destroy(xnintr_t *intr)
{
	rt_release_irq(intr->irq);
	printk("INFO > IRQ: %d, INTRCNT: %lu\n", intr->irq, intr->cnt);
}

int irq_handler(int irq, xnintr_t *intr)
{
	int r = ((int (*)(void *))intr->isr)(intr);
	if (r & ENABLE_IRQ) {
		rt_enable_irq((intr)->irq);
	}
	if (r & PEND_IRQ) {
		rt_pend_linux_irq(intr->irq);
	}
	++intr->cnt;
	return 0;
}

#else

#include <nucleus/intr.h>

#endif

xnintr_t intr;
static volatile int tmrcnt = LATCH;

#define ECHO_PERIOD 1
static struct timer_list timer;

static void timer_fun(unsigned long none)
{
	static int echo;
	printk("%d MAX INTERRUPT LATENCY: %d (us)\n", echo += ECHO_PERIOD, ((LATCH - tmrcnt)*1000000 + CLOCK_TICK_RATE/2)/CLOCK_TICK_RATE);
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ);
}

static int isr(xnintr_t *intr)
{
	int cnt;
	outb(0x00, 0x43);
	cnt = inb(0x40);
	cnt |= (inb(0x40) << 8);
	if (cnt < tmrcnt) {
		tmrcnt = cnt;
	}
	return (ENABLE_IRQ | PEND_IRQ);
}

int __init init_module(void)
{
	init_timer(&timer);
	timer.function = timer_fun;
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ);
	xnintr_init(&intr, 0, isr, NULL, 0);
	xnintr_attach(&intr, &intr);
	return 0;
}

void cleanup_module(void)
{
	del_timer(&timer);
	xnintr_destroy(&intr);
	printk("FINAL MAX INTERRUPT LATENCY: %d (us)\n", ((LATCH - tmrcnt)*1000000 + CLOCK_TICK_RATE/2)/CLOCK_TICK_RATE);
}
