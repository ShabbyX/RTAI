#!/usr/bin/python

from rtai_lxrt import *
from rtai_msg  import *
from rtai_sem import *

from math import *
from array import *
from thread import *
from random import *

printf = libc.printf

PERIOD = 1000000
LOOPS  = 100
NTASKS = 32
ONEMS = PERIOD

def taskname(x) :
	return (1000 + (x))

task = (c_long*NTASKS)()

ntasks = NTASKS

cpus_allowed = 0

def thread_fun(mytask_indx, null2) :
	global cpus_allowed
	sem = c_void_p()
	mytask_name = taskname(mytask_indx)
	cpus_allowed = 1 - cpus_allowed
 	mytask = rt_task_init_schmod(mytask_name, 1, 0, 0, 0, 1 << cpus_allowed)
 	if mytask == NULL :
		printf("CANNOT INIT TASK %lu\n", mytask_name)
		sys.exit(1)
	printf("THREAD INIT: index = %d, name = %lu, address = %p.\n", mytask_indx, mytask_name, mytask)

	if mytask_indx%2 != 0 :
		rt_make_hard_real_time()

	rt_receive(0, byref(sem))

	period = nano2count(PERIOD)
	start_time = rt_get_time() + nano2count(ONEMS)
	rt_task_make_periodic(mytask, start_time + (mytask_indx + 1)*period, ntasks*period)

	count = maxj = 0
	t0 = rt_get_cpu_time_ns()
	while count < LOOPS :
		count += 1
		rt_task_wait_period()
		t = rt_get_cpu_time_ns()
		jit = t - t0 - ntasks*PERIOD
		if jit < 0 :
			jit = -jit
		if count > 1 and jit > maxj :
			maxj = jit
		t0 = t
	maxjp = (maxj + 499)/1000

	rt_sem_signal(sem)
 	if mytask_indx%2 != 0 :
		rt_make_soft_real_time()

	rt_task_delete(mytask)
	printf("THREAD %lu ENDS, LOOPS: %d MAX JIT: %d (us)\n", mytask_name, count, maxjp)
	return


indx = (c_int*NTASKS)()
mytask_name = nam2num("MASTER")

mytask = rt_task_init_schmod(mytask_name, 1, 0, 0, 0, 0xF)
if mytask == NULL :
	printf("CANNOT INIT TASK %lu\n", mytask_name)

printf("MASTER INIT: name = %lu, address = %p.\n", mytask_name, mytask)

sem = rt_sem_init(nam2num("SEM"), 0)
start_rt_timer(0)

for i in range(0, ntasks) :
	indx[i] = i
	start_new_thread(thread_fun, (i, NULL))
	while rt_get_adr(taskname(i)) == NULL :
		rt_sleep(nano2count(ONEMS))
	rt_send(rt_get_adr(taskname(i)), c_ulong(sem))

for i in range(0, ntasks) :
	rt_sem_wait(sem)
	while rt_get_adr(taskname(i)) != NULL :
		rt_sleep(nano2count(ONEMS))

rt_sem_delete(sem)
stop_rt_timer()
rt_task_delete(mytask)
printf("MASTER %lu %p ENDS\n", mytask_name, mytask)
