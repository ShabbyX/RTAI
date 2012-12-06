#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

#include <task.h>
#include <queue.h>

int finished, data_lines = 21;

RT_TASK display_task;

void display(void *cookie)
{
	RT_QUEUE q;
	int n = 0;
	time_t start;
	struct smpl_t { long minjitter, avgjitter, maxjitter, overrun; } *smpl;

	rt_queue_bind(&q, "queue");
	time(&start);
	while (!finished) {
		smpl = NULL;
		rt_queue_recv(&q, (void *)&smpl, TM_INFINITE);
		if (smpl == NULL) continue; 
		if (data_lines && (n++%data_lines) == 0) {
			time_t now, dt;
			time(&now);
			dt = now - start;
			printf("rth|%12s|%12s|%12s|%8s|     %.2ld:%.2ld:%.2ld\n", "lat min", "lat avg", "lat max", "overrun", dt/3600, (dt/60)%60, dt%60 + 1);
		}
		printf("rtd|%12ld|%12ld|%12ld|%8ld\n", smpl->minjitter, smpl->avgjitter, smpl->maxjitter, smpl->overrun);
		rt_queue_free(&q, smpl);
	}
	rt_queue_unbind(&q);
}

void cleanup_upon_sig(int sig __attribute__((unused)))
{
	if (!finished) {
		finished = 1;
		fflush(stdout);
	}
}

int main (int argc, char **argv)
{
	signal(SIGINT,  cleanup_upon_sig);
	signal(SIGTERM, cleanup_upon_sig);
	signal(SIGHUP,  cleanup_upon_sig);
	signal(SIGALRM, cleanup_upon_sig);
	rt_task_create(&display_task, "dsptsk", 0, 97, 0);
	rt_task_start(&display_task, &display, NULL);
	pause();
	rt_task_join("dsptsk");
	return 0;
}
