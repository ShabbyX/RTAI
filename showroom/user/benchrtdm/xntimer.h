/**
 *
 * Copyright: 2006 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTAI_XNTIMER_H
#define _RTAI_XNTIMER_H

#ifndef _RTAI_TASKLETS_H
#define _RTAI_TASKLETS_H

struct rt_task_struct;

struct rt_tasklet_struct {
	struct rt_tasklet_struct *next, *prev;
	int priority, uses_fpu, cpuid;
	RTIME firing_time, period;
	void (*handler)(unsigned long);
	unsigned long data, id;
	int thread;
	struct rt_task_struct *task;
	struct rt_tasklet_struct *usptasklet;
#ifdef  CONFIG_RTAI_LONG_TIMED_LIST
	rb_root_t rbr;
	rb_node_t rbn;
#endif
};

RTIME rt_get_time(void);

int rt_insert_timer(struct rt_tasklet_struct *timer,
		    int priority,
		    RTIME firing_time,
		    RTIME period,
		    void (*handler)(unsigned long),
		    unsigned long data,
		    int pid);

void rt_remove_timer(struct rt_tasklet_struct *timer);

#endif /* !_RTAI_TASKLETS_H */

#define XN_INFINITE           0
#define RTDM_CLASS_BENCHMARK  6

#define xnpod_ns2ticks nano2count

typedef struct rt_tasklet_struct xntimer_t;

static inline void xntimer_init(xntimer_t *timer, void (*handler)(xntimer_t *))
{
	memset(timer, 0, sizeof(struct rt_tasklet_struct));
	timer->handler = (void *)handler;
	timer->data    = (unsigned long)timer;
}

static inline void xntimer_start(xntimer_t *timer, xnticks_t value, xnticks_t interval)
{
	/* Waltzing Matilda zaniness here, they subtract we add again! */
	rt_insert_timer(timer, 0, rt_get_time() + value, interval, timer->handler, (unsigned long)timer, 0);
}

static inline void xntimer_destroy(xntimer_t *timer)
{
	rt_remove_timer(timer);
}

#include <asm/div64.h>  // for do_div below

static inline unsigned long long xnarch_ulldiv(unsigned long long ull, unsigned long uld, unsigned long *r)
{
	unsigned long rem = do_div(ull, uld);
	if (r) {
		*r = rem;
	}
	return ull;
}

#endif /* !_RTAI_XNTIMER_H */
