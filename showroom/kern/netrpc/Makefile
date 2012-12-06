EXTRA_CFLAGS += $(shell rtai-config --module-cflags)

obj-m += comnode_rt.o start_task_rt.o task_code_rt.o

comnode_rt-objs := comnode.o

start_task_rt-objs := start_task.o

task_code_rt-objs := task_code.o
