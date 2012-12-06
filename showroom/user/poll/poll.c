/*
 * RTAI mailboxes polling test
 *
 * COPYRIGHT (C) 2008  Paolo Mantegazza (mantegazza@aero.polimi.it)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <rtai_mbx.h>

//#define HAND
#define NTHR  10
#define NMBX  20
#define BFSZ  80
#define RNDT  500000LL  // long long nanos

static MBX *mbx[NMBX + 1];

static void poll_fun(void *arg)
{
	int i;
	struct rt_poll_s polld[NMBX];
	char buf[BFSZ];
	unsigned long thread = (unsigned long)arg;

	rt_thread_init(rt_get_name(0), 0, 0, SCHED_OTHER, 0xF);
	rt_make_hard_real_time();

	while (1) {
		for (i = 0; i < NMBX; i++) {
			polld[i] = (struct rt_poll_s){ mbx[i], RT_POLL_MBX_RECV };
		}
		rt_poll(polld, NMBX, -(nano2count(rand()*RNDT)/RAND_MAX));
		for (i = 0; i < NMBX; i++) {
			if (!polld[i].what) {
				if (!rt_mbx_receive_if(mbx[i], buf, sizeof(buf))) {
				printf("thread: %lu, mbx: %d, received: %s.\n",thread, i, buf);
				rt_mbx_send(mbx[NMBX], buf, 1);
				if (!buf[0]) break;
				}
			}
		}
	}
	rt_make_soft_real_time();
	rt_task_delete(NULL);

	return;
}

int main(void)
{
	int i;
	struct rt_poll_s polld[1];
	char buf[BFSZ] = " abcdefghilmnopqrstuvz";
	pthread_t poll_thread[NTHR];

	rt_thread_init(nam2num("MAIN"), 0, 0, SCHED_OTHER, 0xF);
	start_rt_timer(0);
	rt_make_hard_real_time();

	for (i = 0; i < (NMBX + 1); i++) {
		mbx[i] = rt_mbx_init(rt_get_name(NULL), BFSZ);
	}

	for (i = 0; i < NTHR; i++) {
		poll_thread[i] = rt_thread_create(poll_fun, (void *)i, 0);
	}

	i = 0;
	while (1) {
#ifdef HAND
		printf("0 <= mbx < %d: ", NMBX);
		scanf("%d", &i);
		if (i < 0 || i >= NMBX) break;
		printf("msg: ");
		scanf("%s", buf);
#else
		rt_sleep(nano2count((rand()*RNDT)/RAND_MAX));
//		if (++i >= NMBX) i = 0;
		i = (rand()*(NMBX - 1LL))/RAND_MAX;
		buf[0] = '0' + i;
#endif
		rt_mbx_send(mbx[i], buf, sizeof(buf));
		polld[0] = (struct rt_poll_s){ mbx[NMBX], RT_POLL_MBX_RECV };
		rt_poll(polld, 1, 0);
		rt_mbx_receive(mbx[NMBX], buf, 1);
	}

	buf[0] = 0;
        for (i = 0; i < NTHR; i++) {
		rt_mbx_send(mbx[0], buf, sizeof(buf));
        }

	for (i = 0; i < (NMBX + 1); i++) {
		rt_mbx_delete(mbx[i]);
	}
	rt_task_delete(NULL);

	return 0;
}
