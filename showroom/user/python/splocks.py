#!/usr/bin/python

from rtai_lxrt import *
from rtai_spl  import *

from thread import *

printf = libc.printf

LOOPS = 1

NTASKS = 3

end = NTASKS

spl = c_void_p(NULL)

def fun(idx, null) :
	global spl, end
	loops = LOOPS
	task = rt_task_init_schmod(0xcacca + idx, 0, 0, 0, 0, 0xF)
	while loops > 0 :
		loops -= 1
		printf("TASK %d 1 COND/TIMED PRELOCKED\n", idx)
		if idx%2 :
			if rt_spl_lock_if(spl) != 0 :
				printf("TASK %d 1 COND PRELOCKED FAILED GO UNCOND\n", idx)
				rt_spl_lock(spl)
		elif rt_spl_lock_timed(spl, nano2count(20000)) != 0 :
			printf("TASK %d 1 TIMED PRELOCKED FAILED GO UNCOND\n", idx)
			rt_spl_lock(spl)
		printf("TASK %d 1 LOCKED\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 2 COND PRELOCK\n", idx)
		if rt_spl_lock_if(spl) != 0 :
			printf("TASK %d 2 COND PRELOCK FAILED GO UNCOND\n", idx)
			rt_spl_lock(spl)
		printf("TASK %d 2 LOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 3 PRELOCK\n", idx)
		rt_spl_lock(spl)
		printf("TASK %d 3 LOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 3 PREUNLOCK\n", idx)
		rt_spl_unlock(spl)
		printf("TASK %d 3 UNLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 2 PREUNLOCK\n", idx)
		rt_spl_unlock(spl)
		printf("TASK %d 2 UNLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 1 PREUNLOCKED\n", idx)
		rt_spl_unlock(spl)
		printf("TASK %d 1 UNLOCKED\n", idx)
		rt_busy_sleep(100000)
	rt_sleep(nano2count(10000000))
	printf("TASK %d EXITED\n", idx)
	rt_task_delete(task)
	end -= 1
	return


task = rt_task_init_schmod(nam2num("TASK"), 1, 0, 0, 0, 0xF)
spl = rt_spl_init(nam2num("SPL"))
start_rt_timer(0)
for i in range(0, NTASKS) :
	start_new_thread(fun, (i + 1, NULL))
while end > 0 :
	rt_sleep(nano2count(1000000000))
stop_rt_timer()
rt_spl_delete(spl)
rt_task_delete(task)
