/**********************************************************/
/*    HARD REAL TIME RTDM Driver User-Space Application   */
/*                                                        */
/*    Based on code of Jan Kiszka - thanks Jan!           */
/*    (C) 2006 Jan Kiszka, www.captain.at                 */
/*    License: GPL                                        */
/**********************************************************/

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
//#include <linux/autoconf.h>

#include <rtdm/rtdm.h>
#include "inc.h"

unsigned int my_state = 0;
int timer_started = 0;
int my_fd = -1;
int shutdownnow = 0;
RT_TASK *my_task;

//                        --s-ms-us-ns
RTIME my_task_period_ns =   1000000000llu;

#ifdef USEMMAP
int *mmappointer;
#endif

/**********************************************************/
/*            CLOSE RT DRIVER                             */
/**********************************************************/
static int close_file( int fd, unsigned char *name)
{
	int ret, i = 0;
	do {
		i++;
		ret = rt_dev_close(fd);
		switch(-ret) { 
			case EBADF:
				printf("%s -> invalid fd or context\n",name);
				break;
			case EAGAIN: 
				printf("%s -> EAGAIN (%d times)\n",name,i);
				rt_sleep(nano2count(50000)); // wait 50us
				break;
			case 0:
				printf("%s -> closed\n",name);
				break;
			default:
				printf("%s -> ???\n",name);
				break;
		}
	} while (ret == -EAGAIN && i < 10);
	return ret;
}

/**********************************************************/
/*            CLEANING UP                                 */
/**********************************************************/
void cleanup_all(void) 
{
	if (my_state & STATE_FILE_OPENED) {
		close_file( my_fd, DEV_FILE " (user)");
		my_state &= ~STATE_FILE_OPENED;
	}
	if (my_state & STATE_TASK_CREATED) {
		printf("delete my_task\n");
		rt_task_delete(my_task);
		my_state &= ~STATE_TASK_CREATED;
	}
	if (timer_started) {
		printf("stop timer\n");
		stop_rt_timer();
	}
}

void catch_signal(int sig)
{
	shutdownnow = 1;
	cleanup_all();
	printf("exit\n");
	return;
}

/**********************************************************/
/*            REAL TIME TASK                              */
/**********************************************************/
void my_task_proc(void *arg)
{
	ssize_t sz;
	ssize_t written;
	ssize_t read;
	int counter;
	int readbackcounter;
	unsigned char buf[17] = "CAPTAIN WAS HERE\0";
	unsigned char buf2[17] = "XXXXXXXXXXXXXXXX\0";

	my_task = rt_task_init_schmod(nam2num("MYTSK"), 2, 0, 0, SCHED_FIFO, 0xF);
#ifdef USEMMAP
	printf("ioctl = %d\n", rt_dev_ioctl(my_fd, MMAP, &mmappointer));
	printf("*p = %d, p = %p\n", *((int *)mmappointer), mmappointer);
#endif

	mlockall(MCL_CURRENT | MCL_FUTURE);
    
	counter = 0;
	while (1) {
		if (++counter >= (BUFFER_SIZE/sizeof(int))) {
			counter = 0;
		}
		sprintf(buf, "CAPTAIN %d", counter);
		rt_make_hard_real_time();

		sz = sizeof(buf);
		written = rt_dev_write(my_fd, &buf, sizeof(buf));
		printf("\nWRITE: written=%d sz=%d\n", written, sz);
		if (written != sz) {
			if (written < 0 ) {
				printf("error while rt_dev_write, code %d\n",written);
			} else {
				printf("only %d / %d byte transmitted\n",written, sz);
			}
			goto exit_my_task;
 		}

		sz = sizeof(buf2);
		read = rt_dev_read(my_fd, &buf2, sizeof(buf2));
		if (read == sz) {
			printf("READ: read=%s\n",buf2);
		} else {
			if (read < 0 ) {
 				printf("error while rt_dev_read, code %d\n",read);
			} else {
				printf("only %d / %d byte received \n",read,sz);
			}
 		}

  		if (shutdownnow == 1) break;
    
		rt_make_soft_real_time();
#ifdef USEMMAP
		mmappointer[counter] = counter;
		printf("MMAP: mmappointer[%d] = %d\n", counter, mmappointer[counter]);
		rt_dev_ioctl(my_fd, SETVALUE, counter);
		rt_dev_ioctl(my_fd, GETVALUE, &readbackcounter);
		printf("IOCTL: mmap readbackcounter=%d\n", readbackcounter);
#endif
		rt_dev_ioctl(my_fd, SETVALUE, &counter);
		rt_dev_ioctl(my_fd, GETVALUE, &readbackcounter);
		printf("IOCTL: pass readbackcounter=%d\n", readbackcounter);
	}

exit_my_task:
	if (my_state & STATE_FILE_OPENED) {
		if (!close_file( my_fd, DEV_FILE " (write)")) {
			my_state &= ~STATE_FILE_OPENED;
		}
	}
	printf("exit\n");
}

/**********************************************************/
/*            MAIN: mainly RT task initialization         */
/**********************************************************/
static pthread_t write_thr;
int main(void)
{
	RT_TASK *maint;

	signal(SIGTERM, catch_signal);
	signal(SIGINT, catch_signal);

	printf("PRESS CTRL-C to EXIT\n");
	maint = rt_task_init_schmod(nam2num("MAIN"), 3, 0, 0, SCHED_FIFO, 0xF);

	start_rt_timer(0);

	my_fd = rt_dev_open(DEV_FILE, 0);
	if (my_fd < 0) {
		printf("can't open %s\n", DEV_FILE);
		goto error;
	}
	my_state |= STATE_FILE_OPENED;
	printf("%s opened\n", DEV_FILE);

	if (pthread_create(&write_thr, NULL, (void *)my_task_proc, NULL) < 0) {
		printf("failed to create my_task thread\n");
		goto error;
	}
	my_state |= STATE_TASK_CREATED;
	printf("my_task created\n");

	pause();
	return 0;

error:
	cleanup_all();
	printf("exiting in error\n");
	return 0;
}
