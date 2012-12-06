prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)

LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags)
LXRT_LDFLAGS = $(shell rtai-config --lxrt-ldflags)

all:: testmsg uptestmsg

ifneq ($(findstring 2.6.,$(shell rtai-config --linux-version 2>/dev/null)),)

LINUX_DIR = $(shell rtai-config --linux-dir)

all:: rtai_sysvmsg.ko

rtai_sysvmsg.ko: util.ko msg.ko
	$(LD) -r -static -o rtai_sysvmsg.ko util.ko msg.ko
#	$(LD) -r -static -o $@ $< msg.ko

msg.ko:
	$(MAKE) -C $(LINUX_DIR) CC=$(CC) SUBDIRS=$$PWD V=$(V) modules

util.ko:
	$(MAKE) -C $(LINUX_DIR) CC=$(CC) SUBDIRS=$$PWD V=$(V) modules

clean::
	$(RM) $(LINUX_DIR)/.tmp_versions/*_rt.mod *.o *.ko *.mod.c .*.cmd

#rtai_sysvipc.o: util_rt.o msg_rt.o
#	ld -r -static -o rtai_sysvipc.o util_rt.o msg_rt.o

else

MODULE_CFLAGS = $(shell rtai-config --module-cflags)

all:: util.o msg.o rtai_sysvipc.o

util_rt.o: %.c
	$(CC) $(MODULE_CFLAGS) -c $<

clean::
	$(RM) *.o

endif

testmsg: testmsg.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(LXRT_LDFLAGS) -lpthread -llxrt

uptestmsg: uptestmsg.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(LXRT_LDFLAGS) -lpthread -llxrt

clean::
	$(RM) testmsg uptestmsg

.PHONY: clean
