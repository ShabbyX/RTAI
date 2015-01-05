#ifndef _TMR_LXRTEXT_
#define _TMR_LXRTEXT_

#include <rtai_lxrt.h>

#define DISABLE_INTR
#define USE_EXT_WAIT

#ifdef DISABLE_INTR
#define CLI()  do { __asm__ __volatile__ ("cli"); } while (0)
#define STI()  do { __asm__ __volatile__ ("sti"); } while (0)
#else
#define CLI()
#define STI()
#endif

#define DIAG_FLAGS
#ifdef DIAG_FLAGS
#define CHECK_FLAGS() \
	do { \
		unsigned long flags; \
		__asm__ __volatile__("pushfl; popl %0": "=g" (flags)); \
		if (flags & (1 << 9)) rt_printk("<> BAD! ENABLED <>\n"); \
	} while (0);
#else
#define CHECK_FLAGS()
#endif

#define ALLOWED_CPUS  1

#define TIMER_IRQ  0

#define NUM_TIMERS  4
#define MY_TIMER    3

#define TMR_INDX  4

#define TMR_GET_SETUP      0
#define TMR_START          1
#define TMR_STOP           2
#define TMR_GET_ISR_COUNT  3
#define TMR_GET_COUNT      4
#define WAIT_ON_TIMER_EXT  5
#define WAIT_ON_TIMER      6

#ifndef __KERNEL__

static inline void tmr_get_setup(int timer, int *period, int *freq) 
{
	struct { int timer, *period, *freq; } arg = { timer, period, freq };
	rtai_lxrt(TMR_INDX, SIZARG, TMR_GET_SETUP, &arg);
}

static inline void tmr_start(int timer)
{
	struct { int timer; } arg = { timer };
	rtai_lxrt(TMR_INDX, SIZARG, TMR_START, &arg);
}

static inline void tmr_stop(int timer)
{
	struct { int timer; } arg = { timer };
	rtai_lxrt(TMR_INDX, SIZARG, TMR_STOP, &arg);
}

static inline int tmr_get_isr_count(int timer) 
{
	struct { int timer; } arg = { timer };
	return rtai_lxrt(TMR_INDX, SIZARG, TMR_GET_ISR_COUNT, &arg).i[LOW];
}

static inline int tmr_get_count(int timer, RTIME *cputime)
{
	struct { int timer; RTIME *cputime; } arg = { timer, cputime };
	return rtai_lxrt(TMR_INDX, SIZARG, TMR_GET_COUNT, &arg).i[LOW];
}

static inline int wait_on_timer_ext(int timer, int *count, RTIME *cputime)
{
	struct { int timer, *count; RTIME *cputime; } arg = { timer, count, cputime };
	return rtai_lxrt(TMR_INDX, SIZARG, WAIT_ON_TIMER_EXT, &arg ).i[LOW];
}

static inline int wait_on_timer(int timer)
{
	struct { int timer; } arg = { timer };
	return rtai_lxrt(TMR_INDX, SIZARG, WAIT_ON_TIMER, &arg ).i[LOW];
}

#endif /*  __KERNEL__ */

#endif /* _TMR_LXRTEXT_ */
