EXTRA_CFLAGS += $(shell rtai-config --module-cflags)

obj-m += condtest_rt.o jepplin_rt.o mqtest_rt.o rwlocks_rt.o

condtest_rt-objs := condtest.o

jepplin_rt-objs := jepplin.o

mqtest_rt-objs := mqtest.o

rwlocks_rt-objs := rwlocks.o
