prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)

LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags)

all: calproc ucal

ifneq ($(findstring 2.6.,$(shell rtai-config --linux-version 2>/dev/null)) || $(findstring 3.,$(shell rtai-config --linux-version 3>/dev/null)),)

LINUX_DIR = $(shell rtai-config --linux-dir)

all:
	@if test -e $(LINUX_DIR)/Module.symvers; then mv -f $(LINUX_DIR)/Module.symvers $(LINUX_DIR)/__Module.symvers; fi; if test -e $(LINUX_DIR)/vmlinux; then mv -f $(LINUX_DIR)/vmlinux $(LINUX_DIR)/__vmlinux; fi; \
	$(MAKE) -C $(LINUX_DIR) CC=$(CC) SUBDIRS=$$PWD V=$(V) modules \
	&& if test -e $(LINUX_DIR)/__Module.symvers; then mv -f $(LINUX_DIR)/__Module.symvers $(LINUX_DIR)/Module.symvers; fi && if test -e $(LINUX_DIR)/__vmlinux; then mv -f $(LINUX_DIR)/__vmlinux $(LINUX_DIR)/vmlinux; fi

clean:
	$(RM) $(LINUX_DIR)/.tmp_versions/*_rt.mod *.o *.ko *.mod.c .*.cmd *.order *.symvers calproc ucal

else

MODULE_CFLAGS = $(shell rtai-config --module-cflags)

all: calmod.o smi.o

calmod.o: calmod.c
	$(CC) $(MODULE_CFLAGS) -c $<

smi.o: calmod.c
	$(CC) $(MODULE_CFLAGS) -c $<

clean:
	$(RM) *.o calproc ucal

endif

LXRT_LDFLAGS = $(shell rtai-config --lxrt-ldflags)

calproc: calproc.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(LXRT_LDFLAGS)

ucal: ucal.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(LXRT_LDFLAGS)

.PHONY: clean

