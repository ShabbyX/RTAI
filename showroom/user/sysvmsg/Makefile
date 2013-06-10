EXTRA_CFLAGS += $(shell rtai-config --module-cflags)

obj-m += util_rt.o msg_rt.o ktestmsg_rt.o kptestmsg_rt.o

util_rt-objs := util.o

msg_rt-objs := msg.o

ktestmsg_rt-objs := ktestmsg.o

kptestmsg_rt-objs := kptestmsg.o
