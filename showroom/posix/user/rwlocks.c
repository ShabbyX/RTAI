/*
COPYRIGHT (C) 2003  Paolo Mantegazza (mantegazza@aero.polimi.it)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/


/* RTAI rwlocks test */

#include <stdlib.h>
#include <sys/mman.h>

#include <rtai_posix.h>

#define TICK 1000000

#define MAKE_IT_HARD
#ifdef MAKE_IT_HARD
#define RT_SET_REAL_TIME_MODE() do { pthread_hard_real_time_np(); } while (0)
#define DISPLAY  rt_printk
#else
#define RT_SET_REAL_TIME_MODE() do { pthread_soft_real_time_np(); } while (0)
#define DISPLAY  printf
#endif

#define LOOPS 1

#define NTASKS 4

static RT_TASK **task;

static pthread_t *thread;

static pthread_rwlock_t rwls;
static pthread_rwlock_t *rwl;

static pthread_barrier_t barrier;

static void *thread_fun(int idx)
{
	unsigned int loops = LOOPS;
	struct timespec abstime;
	char name[7];

	sprintf(name, "TASK%d", idx);
	pthread_setschedparam_np(NTASKS - idx + 1, SCHED_FIFO, 0, 0x1, PTHREAD_HARD_REAL_TIME_NP);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_SET_REAL_TIME_MODE();
	while(loops--) {
		DISPLAY("TASK %d 1 COND/TIMED PREWLOCKED\n", idx);
		clock_gettime(0, &abstime);
		abstime.tv_sec += 2;
		if (idx%2) {
			if (pthread_rwlock_trywrlock(rwl)) {
				DISPLAY("TASK %d 1 COND PREWLOCKED FAILED GO UNCOND\n", idx);
				pthread_rwlock_wrlock(rwl);
			}
		} else if (pthread_rwlock_timedwrlock(rwl, &abstime) >= SEM_TIMOUT) {
			DISPLAY("TASK %d 1 TIMED PREWLOCKED FAILED GO UNCOND\n", idx);
			pthread_rwlock_wrlock(rwl);
		}
		DISPLAY("TASK %d 1 WLOCKED\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 2 COND PREWLOCK\n", idx);
		if (pthread_rwlock_trywrlock(rwl)) {
			DISPLAY("TASK %d 2 COND PREWLOCK FAILED GO UNCOND\n", idx);
			pthread_rwlock_wrlock(rwl);
		}
		DISPLAY("TASK %d 2 WLOCK\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 3 PREWLOCK\n", idx);
		pthread_rwlock_wrlock(rwl);
		DISPLAY("TASK %d 3 WLOCK\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 3 PREWUNLOCK\n", idx);
		pthread_rwlock_unlock(rwl);
		DISPLAY("TASK %d 3 WUNLOCK\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 2 PREWUNLOCK\n", idx);
		pthread_rwlock_unlock(rwl);
		DISPLAY("TASK %d 2 WUNLOCK\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 1 PREWUNLOCKED\n", idx);
		pthread_rwlock_unlock(rwl);
		DISPLAY("TASK %d 1 WUNLOCKED\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 1 COND/TIMED PRERDLOCKED\n", idx);
		clock_gettime(0, &abstime);
		abstime.tv_sec += 2;
		if (idx%2) {
			if (pthread_rwlock_tryrdlock(rwl)) {
				DISPLAY("TASK %d 1 COND PRERDLOCKED FAILED GO UNCOND\n", idx);
				pthread_rwlock_rdlock(rwl);
			}
		} else if (pthread_rwlock_timedrdlock(rwl, &abstime) >= SEM_TIMOUT) {
			DISPLAY("TASK %d 1 TIMED PRERDLOCKED FAILED GO UNCOND\n", idx);
			pthread_rwlock_rdlock(rwl);
		}
		DISPLAY("TASK %d 1 RDLOCKED\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 2 COND PRERDLOCK\n", idx);
		if (pthread_rwlock_tryrdlock(rwl)) {
			DISPLAY("TASK %d 2 COND PRERDLOCK FAILED GO UNCOND\n", idx);
			pthread_rwlock_rdlock(rwl);
		}
		DISPLAY("TASK %d 2 RDLOCK\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 3 PRERDLOCK\n", idx);
		pthread_rwlock_rdlock(rwl);
		DISPLAY("TASK %d 3 RDLOCK\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 3 PRERDUNLOCK\n", idx);
		pthread_rwlock_unlock(rwl);
		DISPLAY("TASK %d 3 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 2 PRERDUNLOCK\n", idx);
		pthread_rwlock_unlock(rwl);
		DISPLAY("TASK %d 2 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
		DISPLAY("TASK %d 1 PRERDUNLOCK\n", idx);
		pthread_rwlock_unlock(rwl);
		DISPLAY("TASK %d 1 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
	}
	rt_make_soft_real_time();
	pthread_barrier_wait(&barrier);
	DISPLAY("TASK %d EXITED\n", idx);
	return NULL;
}

int main(void)
{
	int i;

	thread = (void *)malloc(NTASKS*sizeof(pthread_t));
	task = (void *)malloc(NTASKS*sizeof(RT_TASK *));
	pthread_setschedparam_np(2*NTASKS, SCHED_FIFO, 0, 0x1, PTHREAD_HARD_REAL_TIME_NP);
	pthread_rwlock_init(rwl = &rwls, 0);
	pthread_barrier_init(&barrier, NULL, NTASKS + 1);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	for (i = 0; i < NTASKS; i++) {
		pthread_create(&thread[i], NULL, (void *)thread_fun, (void *)(i + 1));
	}
	pthread_barrier_wait(&barrier);
	pthread_rwlock_destroy(rwl);
	pthread_barrier_destroy(&barrier);
	stop_rt_timer();
	free(thread);
	free(task);
	return 0;
}
