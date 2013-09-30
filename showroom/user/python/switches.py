#!/usr/bin/python

from rtai_lxrt import *
from rtai_sem  import *
from rtai_msg import *

from math import *
from array import *
from thread import *
from random import *

printf = libc.printf

LOOPS = 2000
NR_RT_TASKS = 10

def taskname(x) :
	return (1000 + (x))

mytask = (c_void_p*NR_RT_TASKS)()
hrt = (c_long*NR_RT_TASKS)()
indx = (c_long*NR_RT_TASKS)()

change = 0
end = False

def thread_fun(mytask_indx, null1) :

	msg = c_ulong()
 	mytask[mytask_indx] = rt_task_init_schmod(taskname(mytask_indx), 0, 0, 0, 0, 0x1)
 	if mytask[mytask_indx] == NULL :
		print "CANNOT INIT TASK ", taskname(mytask_indx)
		sys.exit(1)

	rt_make_hard_real_time()
	hrt[mytask_indx] = 1

	while not end :
		if change == 0 :
			rt_task_suspend(mytask[mytask_indx])
		elif change == 1 :
			rt_sem_wait(sem)
		else :
			rt_return(rt_receive(NULL, byref(msg)), 0)

	rt_make_soft_real_time()
	rt_task_delete(mytask[mytask_indx])
	hrt[mytask_indx] = 0
	return

def msleep(ms) :
	libc.poll(NULL, 0, ms)
	return

tsr = tss = tsm = c_longlong(0)
msg = c_ulong(0)

print "\n\nWait for it ...\n"
mainbuddy = rt_task_init_schmod(nam2num("MASTER"), 1000, 0, 0, 0, 0x1)
if mainbuddy == NULL :
	print "CANNOT INIT TASK ", nam2num("MASTER")
	sys.exit(1)

for i in range(0, NR_RT_TASKS) :
	indx[i] = i
	start_new_thread(thread_fun, (i, NULL))

s = 0
while s != NR_RT_TASKS :
	msleep(50)
	s = 0
	for i in range (0, NR_RT_TASKS) :
		s += hrt[i]

sem = rt_sem_init(nam2num("SEMAPH"), 1)
change = 0

print "TESTING WITH", NR_RT_TASKS, "TASKS."
rt_make_hard_real_time()
tsr = rt_get_cpu_time_ns()
for i in range(0, LOOPS) :
	for k in range(0, NR_RT_TASKS) :
		rt_task_resume(mytask[k])
tsr = rt_get_cpu_time_ns() - tsr

change = 1

for k in range(0, NR_RT_TASKS) :
	rt_task_resume(mytask[k])

tss = rt_get_cpu_time_ns()
for i in range(0, LOOPS) :
	for k  in range(0, NR_RT_TASKS) :
	 	rt_sem_signal(sem)
tss = rt_get_cpu_time_ns() - tss

change = 2

for k  in range(0, NR_RT_TASKS) :
	rt_sem_signal(sem)

tsm = rt_get_cpu_time_ns()
for i in range(0, LOOPS) :
	for k in range(0, NR_RT_TASKS) :
		rt_rpc(mytask[k], 0, byref(msg))
tsm = rt_get_cpu_time_ns() - tsm

rt_make_soft_real_time()

printf("\nFOR %d TASKS: ", NR_RT_TASKS)
printf("TIME %d (ms), SUSP/RES SWITCHES %d, ", (int)(tsr/1000000), 2*NR_RT_TASKS*LOOPS)
printf("SWITCH TIME %d (ns)\n", (int)(tsr/(2*NR_RT_TASKS*LOOPS)))

printf("\nFOR %d TASKS: ", NR_RT_TASKS)
printf("TIME %d (ms), SEM SIG/WAIT SWITCHES %d, ", (int)(tss/1000000), 2*NR_RT_TASKS*LOOPS)
printf("SWITCH TIME %d (ns)\n", (int)(tss/(2*NR_RT_TASKS*LOOPS)))

printf("\nFOR %d TASKS: ", NR_RT_TASKS)
printf("TIME %d (ms), RPC/RCV-RET SWITCHES %d, ", (int)(tsm/1000000), 2*NR_RT_TASKS*LOOPS)
printf("SWITCH TIME %d (ns)\n\n", (int)(tsm/(2*NR_RT_TASKS*LOOPS)))
printf("\n\n")

end = True
for i in range(0, NR_RT_TASKS) :
	rt_rpc(mytask[i], 0, byref(msg))
s = 1
while s :
	msleep(50)
	s = 0
	for i in range(0, NR_RT_TASKS) :
		s += hrt[i]

rt_sem_delete(sem)
rt_task_delete(mainbuddy)
