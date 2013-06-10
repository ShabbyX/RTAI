#!/usr/bin/python

from rtai_lxrt import *
from rtai_scb  import *

from math import *
from array import *
from thread import *
from random import *

BUFSIZE = 139
SLEEP_TIME = 100000

SCBSUPRT = USE_GFP_ATOMIC
LCBSIZ = (2*BUFSIZE*sizeof(c_long) + HDRSIZ)

#thread

def thread_fun(lcb, null) :

	n = 0
	data = (c_ulong*BUFSIZE)()

	rt_task_init_schmod(nam2num("THREAD"), 0, 0, 0, 0, 0xF)
#	rt_make_hard_real_time()

	while True :
		cnt = randint(0, BUFSIZE)
		if cnt > 0 :
			while rt_scb_evdrp(lcb, data, cnt*sizeof(c_long)) != 0 :
				rt_sleep(nano2count(SLEEP_TIME))
			rt_scb_get(lcb, data, cnt*sizeof(c_long))
			for i in range(0, cnt) :
				n += 1
				if data[i] != n :
					print "*** SYNC MISSED AT: ", n, "***\n"
					return 0

	rt_make_soft_real_time()
	rt_task_delete(NULL)
	return 0

# main

n = 0
data = (c_ulong*BUFSIZE)()

start_rt_timer(0)
rt_task_init_schmod(nam2num("MAIN"), 1, 0, 0, 0, 0xF)
lcb = rt_scb_init(nam2num("LCB"), LCBSIZ, SCBSUPRT)
start_new_thread(thread_fun, (lcb, NULL))
#rt_make_hard_real_time()

while True :
	cnt = randint(0, BUFSIZE)
	if cnt > 0 :
		for i in range(0, cnt) :
			n += 1
			data[i] = n
		print "TOTAL DATA = ", n, "RAND GEN = ", cnt, "\n"
		while rt_scb_put(lcb, data, cnt*sizeof(c_long)) != 0 :
			rt_sleep(nano2count(SLEEP_TIME))

rt_make_soft_real_time()
rt_task_delete(NULL)
rt_thread_join(thread)
rt_scb_delete(nam2num("LCB"))
