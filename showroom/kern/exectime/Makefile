# Add -O0 to disable optimizing.  Otherwise the test shows very small values
EXTRA_CFLAGS += $(shell rtai-config --module-cflags) -O0

obj-m += exectime_rt.o

exectime_rt-objs := exectime.o
