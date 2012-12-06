#include <stdlib.h>
#include <sys/mman.h>

#include <rtai_posix.h>
#include <rtai_mq.h>

#define MAKE_IT_HARD
#ifdef MAKE_IT_HARD
#define RT_SET_REAL_TIME_MODE() do { pthread_hard_real_time_np(); } while (0)
#define DISPLAY  rt_printk
#else
#define RT_SET_REAL_TIME_MODE() do { pthread_soft_real_time_np(); } while (0)
#define DISPLAY  printf
#endif

static char *strs[] = { "Joey ", "Johnny ", "Dee Dee ", "Marky " };
static char sync_str[] = "sync\n\n";
#define NUM_TASKS  (sizeof(strs)/sizeof(char *))

static pthread_t start_thread, thread[NUM_TASKS];

static sem_t sems[NUM_TASKS], sync_sem, prio_sem;
static pthread_barrier_t barrier;

static pthread_mutex_t print_mtx;
#define PRINT_LOCK    pthread_mutex_lock(&print_mtx);
#define PRINT_UNLOCK  pthread_mutex_unlock(&print_mtx);

#define JMAX_MSG_SIZE  10
static MQ_ATTR mqattrs = { NUM_TASKS, JMAX_MSG_SIZE, 0, 0};

/*
 *  Each task waits to receive the semaphore, prints its string, and
 *  passes the semaphore to the next task.  Then it sends a sync semaphore,
 *  and waits for another semaphore, and this time displays it in
 *  priority order.  Finally, message queues are tested.
 */
static void *task_code(int task_no)
{
	unsigned int i;
	char buf[JMAX_MSG_SIZE];
	struct timespec t;
	static mqd_t mq_in, mq_out;

	pthread_setschedparam_np(NUM_TASKS - task_no, SCHED_FIFO, 0, 0xF, PTHREAD_HARD_REAL_TIME_NP);
	mq_in  = mq_open("mq_in", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &mqattrs);
	mq_out = mq_open("mq_out", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &mqattrs);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_SET_REAL_TIME_MODE();
	pthread_barrier_wait(&barrier);
	for (i = 0; i < 5; ++i) {
		sem_wait(&sems[task_no]);
		PRINT_LOCK; 
		DISPLAY(strs[task_no]);
		PRINT_UNLOCK;
		if (task_no == (NUM_TASKS - 1)) {
			PRINT_LOCK; 
			DISPLAY("\n");
			PRINT_UNLOCK;
		}
		sem_post(&sems[(task_no + 1) % NUM_TASKS]);
	}
	sem_post(&sync_sem);
	sem_wait(&prio_sem);
	PRINT_LOCK; 
	DISPLAY(strs[task_no]);
	PRINT_UNLOCK;
	t = (struct timespec) { 1, 0 };
	nanosleep(&t, NULL);
	clock_gettime(CLOCK_MONOTONIC, &t);
	t.tv_sec += (task_no + 1);
	sem_timedwait(&prio_sem, &t);
	PRINT_LOCK;
	DISPLAY("sem timeout, task %d, %s\n", task_no, strs[task_no]);
	PRINT_UNLOCK;
	sem_post(&sync_sem);

	/* message queue stuff */
	mq_receive(mq_in, buf, sizeof(buf), &i);
	PRINT_LOCK; 
	DISPLAY("\nreceived by task %d ", task_no);
	DISPLAY(buf);
	PRINT_UNLOCK;
	mq_send(mq_out, strs[task_no], strlen(strs[task_no]) + 1, 1);

	/* test receive timeout */
	sem_wait(&sync_sem);
	clock_gettime(CLOCK_MONOTONIC, &t);
	t.tv_sec += (task_no + 1);
	if (mq_timedreceive(mq_in, buf, sizeof(buf), &i, &t) == -ETIMEDOUT) {
		PRINT_LOCK;
		DISPLAY("\nmbx timeout, task %d, %s\n", task_no, strs[task_no]);
		PRINT_UNLOCK;
	}
	mq_close(mq_in);
	mq_close(mq_out);
	PRINT_LOCK;
	DISPLAY("task %d complete\n", task_no);
	PRINT_UNLOCK;
	pthread_exit(0);
	return 0;
}

/*
 * initialization task
 */
static void *start_task_code(void *arg)
{
	unsigned int i, k;
	mqd_t mq_in, mq_out;
	char buf[JMAX_MSG_SIZE];

	pthread_setschedparam_np(NUM_TASKS + 10, SCHED_FIFO, 0, 0xF, PTHREAD_HARD_REAL_TIME_NP);
	pthread_mutex_init(&print_mtx, NULL);
	mq_in  = mq_open("mq_in", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &mqattrs);
	mq_out = mq_open("mq_out", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &mqattrs);
	for (i = 0; i < NUM_TASKS; ++i) {
		sem_init(&sems[i], 0, 0);
		pthread_create(&thread[i], NULL, (void *)task_code, (void *)i);
	}	
	/* create the sync semaphore */
	sem_init(&sync_sem, 0, 0);
	/* create the priority-test semaphore */
	sem_init(&prio_sem, 0, 0);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	RT_SET_REAL_TIME_MODE();
	pthread_barrier_wait(&barrier);
	/* pass the semaphore to the first task */
	sem_post(&sems[0]);
	/* wait for each task to send the sync semaphore */
	for (i = 0; i < NUM_TASKS; ++i) {
		sem_wait(&sync_sem);
	}
	PRINT_LOCK; 
	DISPLAY(sync_str);
	PRINT_UNLOCK;
	/* post the priority-test semaphore -- the tasks should then run */
	/* in priority order */
	for (i = 0; i < NUM_TASKS; ++i) {
		sem_post(&prio_sem);
	}
	PRINT_LOCK; 
	DISPLAY("\n");
	PRINT_UNLOCK;
	for (i = 0; i < NUM_TASKS; ++i) {
		sem_wait(&sync_sem);
	}
	PRINT_LOCK; 
	DISPLAY(sync_str);

	/* now, test message queues */
	DISPLAY("testing message queues\n");
	PRINT_UNLOCK;
	for (i = 0; i < NUM_TASKS; ++i) {
		mq_send(mq_in, strs[i], strlen(strs[i]) + 1, 1);
	}
	for (i = 0; i < NUM_TASKS; ++i) {
		mq_receive(mq_out, buf, sizeof(buf), &k);
		PRINT_LOCK; 
		DISPLAY("\nreceived from mq_out: %s", buf);
		PRINT_UNLOCK;
	}
	for (i = 0; i < NUM_TASKS; ++i) {
		sem_post(&sync_sem);
	}
	PRINT_LOCK; 
	DISPLAY("\n");
	PRINT_UNLOCK;

	/* nothing more for this task to do */
	for (i = 0; i < NUM_TASKS; ++i) {
		pthread_join(thread[i], NULL);
		sem_destroy(&sems[i]);
	}
	mq_close(mq_in);
	mq_close(mq_out);
	mq_unlink("mq_in");
	mq_unlink("mq_out");
	sem_destroy(&sync_sem);
	sem_destroy(&prio_sem);
	pthread_mutex_destroy(&print_mtx);
	DISPLAY("\ninitialization task complete\n");
	pthread_exit(0);
	return 0;
}

int main(void)
{
	printf("\n");
	pthread_setschedparam_np(NUM_TASKS + 20, SCHED_FIFO, 0, 0xF, PTHREAD_HARD_REAL_TIME_NP);
	start_rt_timer(0);
	pthread_barrier_init(&barrier, 0, NUM_TASKS + 2);
	pthread_create(&start_thread, NULL, start_task_code, NULL);
	pthread_barrier_wait(&barrier);
	pthread_join(start_thread, NULL);
	pthread_barrier_destroy(&barrier);
	printf("\n");
	stop_rt_timer();
	pthread_exit(0);
	return 0;
}
