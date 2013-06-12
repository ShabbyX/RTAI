#include <sys/mman.h>

#include <rtai_sem.h>

#define NTASKS        15
#define RESEMT        RESEM_BINSEM // RESEM_BINSEM, RESEM_CHEKWT, RESEM_RECURS
#define CPUS_ALLOWED  0x1 // MP can mix output, making it a difficult reading!

RT_TASK *task[NTASKS];
SEM     *sem[NTASKS];

void *tskfun(void *tasknr)
{
	int tsknr, k, prio, bprio;

	tsknr = (int)tasknr;
	task[tsknr] = rt_task_init_schmod(0, NTASKS - tsknr - 1, 0, 0, SCHED_FIFO, CPUS_ALLOWED);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	rt_sem_wait(sem[tsknr]);
	rt_send(task[0], 0);
	rt_sem_wait(sem[tsknr - 1]);
	rt_sem_signal(sem[tsknr]);
	rt_sem_signal(sem[tsknr - 1]);

	rt_printk("AT TSKNR EXIT %d > (TSKNR-PRI):\n", tsknr);
	for (k = 0; k < tsknr; k++) {
		rt_get_priorities(task[k], &prio, &bprio);
		rt_printk("%d-%d|", k, prio);
	}
	rt_get_priorities(task[tsknr], &prio, &bprio);
	rt_printk("%d-%d\n\n", tsknr, prio);
	return NULL;
}


int main(void)
{
	int i, k, prio, bprio;

	task[0] = rt_task_init_schmod(0, NTASKS - 1, 0, 0, SCHED_FIFO, CPUS_ALLOWED);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	for (i = 0; i < NTASKS; i++) {
		sem[i] = rt_typed_sem_init(0, RESEMT, RES_SEM);
	}

	rt_sem_wait(sem[0]);

	for (i = 1; i < NTASKS; i++) {
		rt_thread_create(tskfun, (void *)i, 0);
		rt_receive(NULL, &k);
		rt_printk("AFTER TSKNR %d CREATED > (TSKNR-PRI):\n", i);
		for (k = 0; k < i; k++) {
			rt_get_priorities(task[k], &prio, &bprio);
			rt_printk("%d-%d|", k, prio);
		}
		rt_get_priorities(task[i], &prio, &bprio);
		rt_printk("%d-%d\n\n", i, prio);
	}

	rt_sem_signal(sem[0]);

	rt_printk("FINAL > (TSKNR-PRI):\n");
       	for (k = 0; k < (NTASKS - 1); k++) {
		rt_get_priorities(task[k], &prio, &bprio);
		rt_printk("%d-%d|", k, prio);
	}
	rt_get_priorities(task[NTASKS - 1], &prio, &bprio);
	rt_printk("%d-%d\n\n", (NTASKS - 1), prio);

	for (i = 0; i < NTASKS; i++) {
		rt_sem_delete(sem[i]);
	}

	return 0;
}
