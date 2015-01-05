#!/usr/bin/python

from rtai import *

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
rt_make_hard_real_time()

while True :
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
rt_rpc(rt_get_adr(nam2num("LATCAL")), msg, byref(msg))
rt_make_soft_real_time
rt_task_delete(task)
print "* EXITING DISPLAY *"
