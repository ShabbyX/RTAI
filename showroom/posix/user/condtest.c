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

static pthread_cond_t    conds;
static pthread_mutex_t   mtxs;
static pthread_barrier_t barriers;
static pthread_cond_t    *cond;
static pthread_mutex_t   *mtx;
static pthread_barrier_t *barrier;

static int cond_data;

static void task_exit_handler(void *arg)
{
	pthread_barrier_wait(barrier);
}

static void *task_func1(void *dummy)
{
	pthread_setschedparam_np(1, SCHED_FIFO, 0, 0x1, PTHREAD_HARD_REAL_TIME_NP);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_SET_REAL_TIME_MODE();
	pthread_cleanup_push(task_exit_handler, 0);
	pthread_barrier_wait(barrier);
	DISPLAY("Starting task1, waiting on the conditional variable to be 1.\n");
	pthread_mutex_lock(mtx);
	while(cond_data < 1) {
		pthread_cond_wait(cond, mtx);
	}
	pthread_mutex_unlock(mtx);
	if(cond_data == 1) {
		DISPLAY("task1, conditional variable signalled, value: %d.\n", cond_data);
	}
	DISPLAY("task1 signals after setting data to 2.\n");
	DISPLAY("task1 waits for a broadcast.\n");
	pthread_mutex_lock(mtx);
	cond_data = 2;
	pthread_cond_signal(cond);
	while(cond_data < 3) {
		pthread_cond_wait(cond, mtx);
	}
	pthread_mutex_unlock(mtx);
	if(cond_data == 3) {
		DISPLAY("task1, conditional variable broadcasted, value: %d.\n", cond_data);
	}
	pthread_cleanup_pop(1);
	DISPLAY("Ending task1.\n");
	pthread_exit(0);
	return 0;
}

static void *task_func2(void *dummy)
{
	pthread_setschedparam_np(2, SCHED_FIFO, 0, 0x1, PTHREAD_HARD_REAL_TIME_NP);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_SET_REAL_TIME_MODE();
	pthread_cleanup_push(task_exit_handler, 0);
	pthread_barrier_wait(barrier);
	DISPLAY("Starting task2, waiting on the conditional variable to be 2.\n");
	pthread_mutex_lock(mtx);
	while(cond_data < 2) {
		pthread_cond_wait(cond, mtx);
	}
	pthread_mutex_unlock(mtx);
	if(cond_data == 2) {
		DISPLAY("task2, conditional variable signalled, value: %d.\n", cond_data);
	}
	DISPLAY("task2 waits for a broadcast.\n");
	pthread_mutex_lock(mtx);
	while(cond_data < 3) {
		pthread_cond_wait(cond, mtx);
	}
	pthread_mutex_unlock(mtx);
	if(cond_data == 3) {
		DISPLAY("task2, conditional variable broadcasted, value: %d.\n", cond_data);
	}
	pthread_cleanup_pop(1);
	DISPLAY("Ending task2.\n");
	pthread_exit(0);
	return 0;
}

static void *task_func3(void *dummy)
{
	struct timespec abstime;
	pthread_setschedparam_np(3, SCHED_FIFO, 0, 0x1, PTHREAD_HARD_REAL_TIME_NP);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_SET_REAL_TIME_MODE();
	pthread_cleanup_push(task_exit_handler, 0);
	pthread_barrier_wait(barrier);
	DISPLAY("Starting task3, waiting on the conditional variable to be 3 with a 2 s timeout.\n");
	pthread_mutex_lock(mtx);
	while(cond_data < 3) {
		clock_gettime(CLOCK_MONOTONIC, &abstime);
		abstime.tv_sec += 2;
		if (pthread_cond_timedwait(cond, mtx, &abstime) != 0) {
			break;
		}
	}
	pthread_mutex_unlock(mtx);
	if(cond_data < 3) {
		DISPLAY("task3, timed out, conditional variable value: %d.\n", cond_data);
	}
	pthread_mutex_lock(mtx);
	cond_data = 3;
	pthread_mutex_unlock(mtx);
	DISPLAY("task3 broadcasts after setting data to 3.\n");
	pthread_cond_broadcast(cond);
	pthread_cleanup_pop(1);
	DISPLAY("Ending task3.\n");
	pthread_exit(0);
	return 0;
}

static void *task_func4(void *dummy)
{
	pthread_setschedparam_np(4, SCHED_FIFO, 0, 0x1, PTHREAD_HARD_REAL_TIME_NP);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_SET_REAL_TIME_MODE();
	pthread_cleanup_push(task_exit_handler, 0);
	pthread_barrier_wait(barrier);
	DISPLAY("Starting task4, signalling after setting data to 1, then waits for a broadcast.\n");
	pthread_mutex_lock(mtx);
	cond_data = 1;
  	pthread_mutex_unlock(mtx);
	pthread_cond_signal(cond);
	pthread_mutex_lock(mtx);
	while(cond_data < 3) {
		pthread_cond_wait(cond, mtx);
	}
	pthread_mutex_unlock(mtx);
	if(cond_data == 3) {
		DISPLAY("task4, conditional variable broadcasted, value: %d.\n", cond_data);
	}
	pthread_cleanup_pop(1);
	DISPLAY("Ending task4.\n");
	pthread_exit(0);
	return 0;
}

static void main_exit_handler(void *arg)
{
	pthread_barrier_destroy(barrier);
	stop_rt_timer();
}

static pthread_t thread1, thread2, thread3, thread4;

int main(void)
{
	pthread_setschedparam_np(0, SCHED_FIFO, 0, 0x1, PTHREAD_HARD_REAL_TIME_NP);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_SET_REAL_TIME_MODE();
	pthread_cleanup_push(main_exit_handler, 0);
	start_rt_timer(nano2count(TICK));
	DISPLAY("User space POSIX test program.\n");
	pthread_cond_init(cond = &conds, NULL);
	pthread_mutex_init(mtx = &mtxs, NULL);
	pthread_barrier_init(barrier = &barriers, NULL, 5);
	pthread_create(&thread1, NULL, task_func1, NULL);
	pthread_create(&thread2, NULL, task_func2, NULL);
	pthread_create(&thread3, NULL, task_func3, NULL);
	pthread_create(&thread4, NULL, task_func4, NULL);
	pthread_barrier_wait(barrier);
	DISPLAY("\nDo not panic, wait 2 s, till task3 times out.\n\n");
	pthread_barrier_wait(barrier);
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);
	pthread_join(thread4, NULL);
	pthread_cond_destroy(cond);
	pthread_mutex_destroy(mtx);
	pthread_cleanup_pop(1);
	DISPLAY("User space POSIX test program removed.\n");
	pthread_exit(0);
	return 0;
}
