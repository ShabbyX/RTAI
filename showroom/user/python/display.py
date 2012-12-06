#!/usr/bin/python

from rtai_lxrt import *
from rtai_mbx import *
from rtai_msg import *
from rtai_sem import *

TEST_POLL = False

class SAMP(Structure) :
	_fields_ = [("min", c_longlong), 
	            ("max", c_longlong),
		    ("index", c_int),
		    ("ovrn", c_int)]
samp = SAMP(0, 0, 0, 0)

class POLL(Structure) :
	_fields_ = [("max", c_int), 
	            ("min", c_int)]
ufds = POLL(0, 1)

max = -1000000000
min = 1000000000
msg = c_ulong(0)

rt_allow_nonroot_hrt()

task = rt_task_init_schmod(nam2num("LATCHK"), 20, 0, 0, 0, 0xF)
mbx = rt_get_adr(nam2num("LATMBX"))
polld = (rt_poll_s*2)()
rt_make_hard_real_time()

while True :

	if TEST_POLL :
		while True :
			polld[0] = ( mbx, RT_POLL_MBX_RECV )
			polld[1] = ( mbx, RT_POLL_MBX_RECV )
			retval = rt_poll(byref(polld), 2, -200000000)
			libc.printf("POLL: %x %p %p\n", retval, polld[0].what, polld[1].what)
			if polld[0].what == NULL :
				break

	rt_mbx_receive(mbx, byref(samp), sizeof(samp))
	if max < samp.max :
		max = samp.max
	if min > samp.min :
		min = samp.min
	print "* min:", samp.min, "/", min, "max:", samp.max, "/", max, "average: ", samp.index, "(", samp.ovrn, ") <Hit [RETURN] to stop> *\n"
	if libc.poll(byref(ufds), 1, 1) :
		ch = libc.getchar()
		break

print "* SENDING END MESSAGE TO LATENCY *"
rt_rpc_timed(rt_get_adr(nam2num("LATCAL")), msg, byref(msg), nano2count(1000000000))
rt_make_soft_real_time
rt_task_delete(task)
print "* EXITING DISPLAY *"
