/*
COPYRIGHT (C) 2003  Trevor Woolven (trevw@zentropix.com)
		    Paolo Mantegazza (mantegazza@aero.polimi.it)

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


// adaption to user space of Trevor's kernel space message queue test

#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <rtai_lxrt.h>
#include <rtai_mq.h>
#include <rtai_sem.h>

static inline void count2timespec(RTIME rt, struct timespec *t)
{
	t->tv_sec = (rt = count2nano(rt))/1000000000;
	t->tv_nsec = rt - t->tv_sec*1000000000LL;
}

#define MAKE_IT_HARD
#ifdef MAKE_IT_HARD
#define RT_MAKE_HARD_REAL_TIME() do { rt_make_hard_real_time(); } while (0)
#define DISPLAY  rt_printk
#else
#define RT_MAKE_HARD_REAL_TIME()
#define DISPLAY  printf
#endif

static SEM *sem1, *sem2, *barrier;

struct Msg { const char *str; int str_size; unsigned int prio; };

void *child_func(void *arg)
{
	size_t n = 0;
	char inBuf[50];
	unsigned int priority = 0;
	int my_oflags, nMsgs = 0;
	struct mq_attr my_attrs = { 0, 0, 0, 0};
	mode_t my_mode = 0;
	struct Msg myMsg[] = { { "Send parent message on queue!", 29, 4 } };
	mqd_t rx_q = INVALID_PQUEUE, tx_q = INVALID_PQUEUE;
	struct timespec timeout;

	rt_task_init_schmod(nam2num("CHDTSK"), 2, 0, 0, SCHED_FIFO, 0x1);
	rt_sem_wait_barrier(barrier);
	DISPLAY("Starting child task\n");
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_MAKE_HARD_REAL_TIME();
	rt_sem_wait(sem1);

	//Open a queue for reading
	rx_q = mq_open("my_queue1", O_RDONLY, my_mode, &my_attrs);
	if (rx_q <= 0) {
		DISPLAY("ERROR: child cannot open my_queue1\n");
	} else {
		//Get the message(s) off the pQueue
		n = mq_getattr(rx_q, &my_attrs);
		nMsgs = my_attrs.mq_curmsgs;
		DISPLAY("Child finds %d messages on queue %ld\n", nMsgs, rx_q);
		while(nMsgs-- > 0) {
			n = mq_receive(rx_q, inBuf, sizeof(inBuf), &priority);
			inBuf[n] = '\0';
			//Report what happened
			DISPLAY("Child got >%s<, %d bytes at priority %d\n", inBuf,n, priority);
		}
	}
	//Create another queue for comms back to parent
	my_oflags = O_RDWR | O_CREAT | O_NONBLOCK;
	my_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	my_attrs.mq_maxmsg = 10;
	my_attrs.mq_msgsize = 50;

	tx_q = mq_open("my_queue2", my_oflags, my_mode, &my_attrs);

	//Now send parent a message on this queue
	if (tx_q > 0) {
		count2timespec(rt_get_time() + nano2count(100000000), &timeout);
		n = mq_timedsend(tx_q, myMsg[0].str, myMsg[0].str_size, myMsg[0].prio, &timeout);
//		n = mq_send(tx_q, myMsg[0].str, myMsg[0].str_size, myMsg[0].prio);
		DISPLAY("Child sent >%s< to parent on queue %ld\n", myMsg[0].str, tx_q);
	} else {
		DISPLAY("ERROR: child could not create my_queue2\n");
	}

	//...and a semaphore to wake the lazy git up!
	DISPLAY("Child wakes up parent\n");
	rt_sem_signal(sem2);

	//Close queue (at least my access to it)
	DISPLAY("Child closing queues\n");
	mq_close(rx_q);
	mq_close(tx_q);
	//Unlink the queue I own
	mq_unlink("mq_queue2");
	DISPLAY("Child end\n");
	rt_make_soft_real_time();
	pthread_exit(0);
}

static pthread_t parent_thread, child_thread;

void *parent_func(void *arg)
{
	int i, my_oflags, n = 0;
	mode_t my_mode;
	struct mq_attr my_attrs;
	char inBuf[50];
	unsigned int priority = 0;
	static const char str1[] = "Test string number one";
	static const char str2[] = "This is test string num 2!!";
	static const char str3[] = "Test string number 3!";
	static const char str4[] = "This is test string number four!!";
	#define nMsgs 4
	static const struct Msg myMsgs[nMsgs] = {
	{ str1, 22, 5 }, { str2, 27, 1 }, { str3, 21, 6 }, { str4, 33, 7 } };
	mqd_t tx_q = INVALID_PQUEUE, rx_q = INVALID_PQUEUE;
	struct timespec timeout;

	rt_task_init_schmod(nam2num("PRNTSK"), 1, 0, 0, SCHED_FIFO, 0x1);
	rt_sem_wait_barrier(barrier);
	DISPLAY("Starting parent task\n");
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_MAKE_HARD_REAL_TIME();

	//Create a queue
	my_oflags = O_RDWR | O_CREAT | O_NONBLOCK;
	my_mode = S_IRUSR | S_IWUSR | S_IRGRP;
	my_attrs.mq_maxmsg = 10;
	my_attrs.mq_msgsize = 50;

	tx_q = mq_open("my_queue1", my_oflags, my_mode, &my_attrs);

	if (tx_q > 0) {
		DISPLAY("Parent sending %d messages on queue %ld\n", nMsgs, tx_q);
		for (i = 0; i <  nMsgs ; i++) {
			//Put message on queue
			n = mq_send(tx_q, myMsgs[i].str, myMsgs[i].str_size, myMsgs[i].prio);
			//Report how much was sent
			DISPLAY("Parent sent >%s<, %d bytes at priority %d\n", myMsgs[i].str, n, myMsgs[i].prio);
		}
	} else {
		DISPLAY("ERROR: parent could not create my_queue1\n");
	}
	rt_sem_signal(sem1);

	//Wait for comms back from kiddy
	DISPLAY("Parent waiting\n");
	rt_sem_wait(sem2);
	DISPLAY("Parent waking up\n");

	//Now open the receive queue to read what kiddy has to say
	rx_q = mq_open("my_queue2", my_oflags, 0, 0);
	count2timespec(rt_get_time() + nano2count(100000000), &timeout);
	n = mq_timedreceive(rx_q, inBuf, sizeof(inBuf), &priority, &timeout);
//	n = mq_receive(rx_q, inBuf, sizeof(inBuf), &priority);
	if (n > 0) {
		inBuf[n] = '\0';
		DISPLAY("Parent got >%s< from child on queue %d\n", inBuf, (int)rx_q);
	} else {
		DISPLAY("Parent got no message from child on queue %d, error %d\n", (int)rx_q, n);
	}

	//Tidy-up...
	rt_make_soft_real_time();
	pthread_join(child_thread, NULL);
	DISPLAY("Parent closing queues\n");
	mq_close(tx_q);
	mq_close(rx_q);
	mq_unlink("my_queue1");
	mq_unlink("my_queue2");
	rt_sleep(nano2count(100000));
	DISPLAY("Parent end\n");
	pthread_exit(0);
}

int main(void)
{
	RT_TASK *me;
	me = rt_task_init_schmod(nam2num("MAIN"), 3, 0, 0, SCHED_FIFO, 0x1);
	barrier = rt_sem_init(nam2num("BARIER"), 3);
	sem1 = rt_sem_init(nam2num("SEM1"), 0);
	sem2 = rt_sem_init(nam2num("SEM2"), 0);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	printf("\n==== Posix Queues test program ====\n");
	pthread_create(&parent_thread, NULL, parent_func, NULL);
	pthread_create(&child_thread, NULL, child_func, NULL);
	rt_sem_wait_barrier(barrier);
	printf("\n==== All Posix Queues threads running ====\n");
	pthread_join(parent_thread, NULL);
	rt_sem_delete(barrier);
	rt_sem_delete(sem1);
	rt_sem_delete(sem2);
	stop_rt_timer();
	rt_task_delete(me);
	printf("\n==== Posix Queues test program removed====\n");
	return 0;
}
