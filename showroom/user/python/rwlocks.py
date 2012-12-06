#!/usr/bin/python

from rtai_lxrt import *
from rtai_rwl  import *

from thread import *

printf = libc.printf

LOOPS = 1

NTASKS = 3

end = NTASKS

rwl = c_void_p(NULL)

def fun(idx, null) :
	global rwl, end
	loops = LOOPS
	task = rt_task_init_schmod(0xcacca + idx, 0, 0, 0, 0, 0xF)
	while loops > 0 :
		loops -= 1
		printf("TASK %d 1 COND/TIMED PREWLOCKED\n", idx)
		if idx%2 :
			if rt_rwl_wrlock_if(rwl) != 0 :
				printf("TASK %d 1 COND PREWLOCKED FAILED GO UNCOND\n", idx);
				rt_rwl_wrlock(rwl)
		elif rt_rwl_wrlock_timed(rwl, nano2count(20000)) >= 60000 :
			printf("TASK %d 1 TIMED PREWLOCKED FAILED GO UNCOND\n", idx)
			rt_rwl_wrlock(rwl)
		printf("TASK %d 1 WLOCKED\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 2 COND PREWLOCK\n", idx)
		if rt_rwl_wrlock_if(rwl) != 0 :
			printf("TASK %d 2 COND PREWLOCK FAILED GO UNCOND\n", idx)
			rt_rwl_wrlock(rwl)
		printf("TASK %d 2 WLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 3 PREWLOCK\n", idx)
		rt_rwl_wrlock(rwl)
		printf("TASK %d 3 WLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 3 PREWUNLOCK\n", idx)
		rt_rwl_unlock(rwl)
		printf("TASK %d 3 WUNLOCK\n", idx);
		rt_busy_sleep(100000)
		printf("TASK %d 2 PREWUNLOCK\n", idx)
		rt_rwl_unlock(rwl)
		printf("TASK %d 2 WUNLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 1 PREWUNLOCKED\n", idx)
		rt_rwl_unlock(rwl)
		printf("TASK %d 1 WUNLOCKED\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 1 COND/TIMED PRERDLOCKED\n", idx)
		if idx%2 != 0 :
			if rt_rwl_rdlock_if(rwl) != 0 :
				printf("TASK %d 1 COND PRERDLOCKED FAILED GO UNCOND\n", idx)
				rt_rwl_rdlock(rwl)
		elif rt_rwl_rdlock_timed(rwl, nano2count(20000)) >= 60000 :
			printf("TASK %d 1 TIMED PRERDLOCKED FAILED GO UNCOND\n", idx)
			rt_rwl_rdlock(rwl)
		printf("TASK %d 1 RDLOCKED\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 2 COND PRERDLOCK\n", idx)
		if rt_rwl_rdlock_if(rwl) != 0 :
			printf("TASK %d 2 COND PRERDLOCK FAILED GO UNCOND\n", idx)
			rt_rwl_rdlock(rwl)
		printf("TASK %d 2 RDLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 3 PRERDLOCK\n", idx)
		rt_rwl_rdlock(rwl)
		printf("TASK %d 3 RDLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 3 PRERDUNLOCK\n", idx)
		rt_rwl_unlock(rwl)
		printf("TASK %d 3 RDUNLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 2 PRERDUNLOCK\n", idx)
		rt_rwl_unlock(rwl)
		printf("TASK %d 2 RDUNLOCK\n", idx)
		rt_busy_sleep(100000)
		printf("TASK %d 1 PRERDUNLOCK\n", idx)
		rt_rwl_unlock(rwl)
		printf("TASK %d 1 RDUNLOCK\n", idx)
		rt_busy_sleep(100000)
	rt_sleep(nano2count(10000000))
	printf("TASK %d EXITED\n", idx)
        rt_task_delete(task)
        end -= 1
        return



task = rt_task_init_schmod(nam2num("TASK"), 1, 0, 0, 0, 0xF)
rwl = rt_rwl_init(nam2num("RWL"))
start_rt_timer(0)
for i in range(0, NTASKS) :
	start_new_thread(fun, (i + 1, NULL))
while end > 0 :
        rt_sleep(nano2count(1000000000))
stop_rt_timer()
rt_rwl_delete(rwl)
rt_task_delete(task)
