#!/usr/bin/python

from rtai_lxrt import *
from rtai_scb  import *
from rtai_bits import *

from math import *
from array import *
from thread import *
from random import *

SLEEP_TIME = 200000
BITS_TIMOUT = 1

bits = c_void_p()

def fun0(null1, null2) :
	resulting_mask = c_ulong(0)

	task = rt_task_init_schmod(nam2num("TASK0"), 0, 0, 0, 0, 0xF)
	rt_make_hard_real_time()
	print "FUN0 WAITS FOR ALL_SET TO 0x0000FFFF WHILE BITS MASK IS ", hex(rt_get_bits(bits))
	rt_bits_wait(bits, ALL_SET, 0x0000FFFF, NOP_BITS, 0, byref(resulting_mask))
	print "FUN0 RESUMED WITH BITS MASK ", hex(rt_get_bits(bits))
	rt_sleep(nano2count(SLEEP_TIME))
	print "FUN0 WAITS_IF FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS ", hex(rt_get_bits(bits))
	rt_bits_wait_if(bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, byref(resulting_mask))
	print "FUN0 WAITS FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS ", hex(rt_get_bits(bits))
	rt_bits_wait(bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, byref(resulting_mask))
	print "FUN0 RESUMED WITH BITS MASK ", hex(rt_get_bits(bits))
	rt_sleep(nano2count(SLEEP_TIME))
	print "FUN0 WAITS 2 s FOR ALL_SET TO 0xFFFFFFFF WHILE BITS MASK IS ", hex(rt_get_bits(bits))
	timeout = rt_bits_wait_timed(bits, ALL_SET, 0xFFFFFFFF, NOP_BITS, 0, nano2count(2000000000), byref(resulting_mask))
	if timeout == BITS_TIMOUT :
		print "FUN0 RESUMED WITH BITS MASK ", hex(rt_get_bits(bits)), "(TIMEOUT)"
	else :
		print "FUN0 RESUMED WITH BITS MASK ", hex(rt_get_bits(bits))
	rt_make_soft_real_time()
	rt_task_delete(task)
	return

def fun1(null1, null2) :
	resulting_mask = c_ulong(0)

	task = rt_task_init_schmod(nam2num("TASK1"), 0, 0, 0, 0, 0xF)
	rt_make_hard_real_time()
	print "FUN1 WAITS FOR ALL_CLR TO 0xFFFF0000 WHILE BITS MASK IS ", hex(rt_get_bits(bits))
	rt_bits_wait(bits, ALL_CLR, 0xFFFF0000, NOP_BITS, 0, byref(resulting_mask))
	print "FUN1 RESUMED WITH BITS MASK ", hex(rt_get_bits(bits))
	rt_sleep(nano2count(SLEEP_TIME))
	print "FUN1 WAITS_IF FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS ", hex(rt_get_bits(bits))
	rt_bits_wait_if(bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, byref(resulting_mask))
	print "FUN1 WAITS FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS ", hex(rt_get_bits(bits))
	rt_bits_wait(bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, byref(resulting_mask))
	print "FUN1 RESUMED WITH BITS MASK ", hex(rt_get_bits(bits))
	rt_sleep(nano2count(SLEEP_TIME))
	print "FUN1 WAITS 2 s FOR ALL_SET TO 0xFFFFFFFF WHILE BITS MASK IS ", hex(rt_get_bits(bits))
	timeout = rt_bits_wait_timed(bits, ALL_SET, 0xFFFFFFFF, NOP_BITS, 0, nano2count(2000000000), byref(resulting_mask))
	if timeout == BITS_TIMOUT :
		print "FUN1 RESUMED WITH BITS MASK ", hex(rt_get_bits(bits)), "(TIMEOUT)"
	else :
		print "FUN1 RESUMED WITH BITS MASK ", hex(rt_get_bits(bits))
#	rt_make_soft_real_time()
	rt_task_delete(task)
	return

def fun(null1, null2) :
	task = rt_task_init_schmod(nam2num("TASK"), 0, 0, 0, 0, 0xF)
	rt_make_hard_real_time()
	mask = hex(rt_bits_signal(bits, SET_BITS, 0x0000FFFF))
	print "SIGNAL SET_BITS 0x0000FFFF AND RETURNS BITS MASK ", mask
	rt_sleep(nano2count(SLEEP_TIME))
	mask = hex(rt_bits_signal(bits, CLR_BITS, 0xFFFF0000))
	print "SIGNAL CLR_BITS 0xFFFF0000 AND RETURNS BITS MASK ", mask
	rt_sleep(nano2count(SLEEP_TIME))
	print "RESET BITS TO 0"
	rt_bits_reset(bits, 0)
	rt_sleep(nano2count(SLEEP_TIME))
	rt_bits_reset(bits, 0)
	rt_make_soft_real_time()
	rt_task_delete(task)
	return


task = rt_task_init_schmod(nam2num("MYTASK"), 9, 0, 0, 0, 0xF)
bits = rt_bits_init(nam2num("BITS"), 0xFFFF0000)
start_rt_timer(0)
start_new_thread(fun0, (NULL, NULL))
rt_sleep(nano2count(SLEEP_TIME*10))
start_new_thread(fun1, (NULL, NULL))
rt_sleep(nano2count(SLEEP_TIME*10))
start_new_thread(fun, (NULL, NULL))
#print "TYPE <ENTER> TO TERMINATE"
libc.getchar()
stop_rt_timer()
rt_bits_delete(bits)
rt_task_delete(task)
