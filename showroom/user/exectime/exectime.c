#include <stdio.h>
#include <rtai_lxrt.h>

int main(void)
{
	int i;
	RTIME t1 = 0, t2 = 0;
	RTIME start[3], end[3], end2[3];;

	rt_set_oneshot_mode();
	start_rt_timer(nano2count(100000));

	rt_thread_init(nam2num("MAIN"), 0, 0, SCHED_FIFO, 0xF);
	rt_make_hard_real_time();

	t1 = rt_get_time_ns();
	rt_get_exectime(rt_buddy(), start);

	/* do some random stuff to avoid optimization */
	for (i = 0; i < 100000; ++i)
		t2 += t1;

	t2 = rt_get_time_ns();
	rt_get_exectime(rt_buddy(), end);

	rt_sleep(nano2count(10000000));

	rt_get_exectime(rt_buddy(), end2);

	rt_make_soft_real_time();
	rt_task_delete('\0');

	printf("This value should be updated even before sleep: count2nano(end[0]-start[0]): %llu\n",
			(unsigned long long)count2nano(end[0] - start[0]));
	printf("t2-t1 (time difference using rt_get_time): %llu\n",
			(unsigned long long)(t2 - t1));
	printf("count2nano(end2[0]-start[0]) (time difference using rt_get_exectime after sleep): %llu\n",
			(unsigned long long)count2nano(end2[0] - start[0]));
	return 0;
}
