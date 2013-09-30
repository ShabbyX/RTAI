#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_mbx.h>

MODULE_LICENSE("GPL");

#define USE_POLL 0

#if USE_POLL
#define poll_mbx(mbx, what) \
	do { \
		struct rt_poll_s polld = { mbx, what }; \
		rt_poll(&polld, 1, 0); \
	} while (0)
#else
#define poll_mbx(mbx, what)
#endif

//#define DELETE

#define ONE_SHOT

static char *strs[] = { "Joey    ", "Johnny  ", "Dee Dee ", "Marky   " };
static char sync_str[] = "sync\n";

#define NUM_TASKS (sizeof(strs)/sizeof(char *))

static char *task[] = { "T1", "T2", "T3", "T4" };
static RT_TASK *start_task, *tasks[NUM_TASKS];

static char *sem[] = { "S1", "S2", "S3", "S4" };
static SEM *sems[NUM_TASKS], *sync_sem, *prio_sem, *print_sem;

static MBX *mbx_in, *mbx_out;

#define TAKE_PRINT  rt_sem_wait(print_sem);
#define GIVE_PRINT  rt_sem_signal(print_sem);

/*
 *  Each task waits to receive the semaphore, prints its string, and
 *  passes the semaphore to the next task.  Then it sends a sync semaphore,
 *  and waits for another semaphore, and this time displays it in
 *  priority order.  Finally, message queues are tested.
 */

static RTIME checkt;
static int checkj, cleanup;

static void task_code(long task_no)
{
	int i, ret;
	char buf[9];
	buf[8] = 0;
	for (i = 0; i < 5; ++i)	{
		rt_sem_wait(sems[task_no]);
		TAKE_PRINT;
		rt_printk(strs[task_no]);
		GIVE_PRINT;
		if (task_no == NUM_TASKS-1) {
			TAKE_PRINT;
			rt_printk("\n");
			GIVE_PRINT;
		}
		rt_sem_signal(sems[(task_no + 1) % NUM_TASKS]);
	}
	rt_sem_signal(sync_sem);
	rt_sem_wait(prio_sem);
	TAKE_PRINT;
	rt_printk(strs[task_no]);
	GIVE_PRINT;
	rt_sleep(nano2count(1000000000LL));
	rt_sem_wait_timed(prio_sem, nano2count((task_no + 1)*1000000000LL));
	TAKE_PRINT;
	rt_printk("sem timeout, task %d, %s\n", task_no, strs[task_no]);
	GIVE_PRINT;
	rt_sem_signal(sync_sem);

/* message queue stuff */
	poll_mbx(mbx_in, RT_POLL_MBX_RECV);
	if ((ret = rt_mbx_receive(mbx_in, buf, 8)) != 0) {
		TAKE_PRINT;
		rt_printk("rt_mbx_receive() failed with %d\n", ret);
		GIVE_PRINT;
	}
	TAKE_PRINT;
	rt_printk("\nreceived by task %d ", task_no);
	rt_printk(buf);
	GIVE_PRINT;
	poll_mbx(mbx_out, RT_POLL_MBX_SEND);
	rt_mbx_send(mbx_out, strs[task_no], 8);
/* test receive timeout */
	rt_sem_wait(sync_sem);
	if (rt_mbx_receive_timed(mbx_in, buf, 8, nano2count((task_no + 1)*1000000000LL))) {
		TAKE_PRINT;
		rt_printk("mbx timeout, task %d, %s\n", task_no, strs[task_no]);
		GIVE_PRINT;
	}
	TAKE_PRINT;
	rt_printk("\ntask %d complete\n", task_no);
	GIVE_PRINT;
	rt_task_suspend(tasks[task_no]);
}

/*
 * initialization task
 */
static void start_task_code(long notused)
{
	int i;
	char buf[9];
	buf[8] = 0;
  /* create the sync semaphore */
	sync_sem = rt_named_sem_init("SYNCSM", 0);
  /* create the priority-test semaphore */
	prio_sem = rt_named_sem_init("PRIOSM", 0);
  /* pass the semaphore to the first task */
	rt_sem_signal(sems[0]);
  /* wait for each task to send the sync semaphore */
	for (i = 0; i < NUM_TASKS; ++i) {
		rt_sem_wait(sync_sem);
	}
	TAKE_PRINT;
	rt_printk(sync_str);
	GIVE_PRINT;
  /* post the priority-test semaphore -- the tasks should then run */
  /* in priority order */
	for (i = 0; i < NUM_TASKS; ++i) {
		rt_sem_signal(prio_sem);
	}
	TAKE_PRINT;
	rt_printk("\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		rt_sem_wait(sync_sem);
	}
	TAKE_PRINT;
	rt_printk(sync_str);
	GIVE_PRINT;

  /* now, test message queues */
	TAKE_PRINT;
	rt_printk("testing message queues\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		poll_mbx(mbx_in, RT_POLL_MBX_SEND);
		if (rt_mbx_send(mbx_in, strs[i], 8)) {
			TAKE_PRINT;
			rt_printk("rt_mbx_send() failed\n");
			GIVE_PRINT;
		}
	}
	for (i = 0; i < NUM_TASKS; ++i) {
		poll_mbx(mbx_out, RT_POLL_MBX_RECV);
		rt_mbx_receive(mbx_out, buf, 8);
		TAKE_PRINT;
		rt_printk("\nreceived from mbx_out: %s", buf);
		GIVE_PRINT;
	}
	TAKE_PRINT;
	rt_printk("\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		rt_sem_signal(sync_sem);
	}
	TAKE_PRINT;
	rt_printk("\ninit task complete\n");
	GIVE_PRINT;
	cleanup = 1;
	rt_task_suspend(start_task);
}

int init_module(void)
{
	int i;
	print_sem = rt_typed_named_sem_init("PRTSEM", 1, BIN_SEM);
	if (!(start_task = rt_named_task_init("STRTSK", start_task_code, 0, 3000, 10, 0, 0))) {
		printk("Could not start init task\n");
	}
	if (!(mbx_in = rt_named_mbx_init("MBXIN", NUM_TASKS*8)) || !(mbx_out = rt_named_mbx_init("MBXOUT", NUM_TASKS*8))) {
		printk("could not create message queue\n");
		return 1;
	}
	for (i = 0; i < NUM_TASKS; ++i) {
		sems[i] = rt_named_sem_init(sem[i], 0);
		if (!(tasks[i] = rt_named_task_init(task[i], task_code, i, 3000, NUM_TASKS - i, 0, 0))) {
			printk("rt_task_ipc_init failed\n");
			return 1;
		}
	}
#ifdef ONE_SHOT
	rt_set_oneshot_mode();
#endif
	start_rt_timer_ns(10000000);
	checkt = rdtsc();
	checkj = jiffies;
	rt_task_resume(start_task);
	for (i = 0; i < NUM_TASKS; ++i) {
		rt_task_resume(tasks[i]);
	}
	return 0;
}

void cleanup_module(void)
{
	while (!cleanup) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}
	checkt = rdtsc() - checkt;
	checkj = jiffies - checkj;
	stop_rt_timer();
	printk("\n(JIFFIES COUNT CHECK: TRUE = %d, LINUX = %d)\n", (int)llimd(checkt, HZ, CPU_FREQ), checkj);
#ifdef DELETE
	do {
		int i;
		rt_named_task_delete(start_task);
		for (i = 0; i < NUM_TASKS; ++i) {
			rt_named_task_delete(tasks[i]);
			rt_named_sem_delete(sems[i]);
		}
		rt_named_sem_delete(print_sem);
		rt_named_sem_delete(prio_sem);
		rt_named_sem_delete(sync_sem);
		rt_named_mbx_delete(mbx_in);
		rt_named_mbx_delete(mbx_out);
	} while (0);
#endif
}
