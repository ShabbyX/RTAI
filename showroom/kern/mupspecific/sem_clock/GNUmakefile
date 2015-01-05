prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)

LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags)

all:: screen keybrd

ifneq ($(findstring 2.6.,$(shell rtai-config --linux-version 2>/dev/null)) || $(findstring 3.,$(shell rtai-config --linux-version 3>/dev/null)),)

LINUX_DIR = $(shell rtai-config --linux-dir)

all::
	@if test -e $(LINUX_DIR)/Module.symvers; then mv -f $(LINUX_DIR)/Module.symvers $(LINUX_DIR)/__Module.symvers; fi; if test -e $(LINUX_DIR)/vmlinux; then mv -f $(LINUX_DIR)/vmlinux $(LINUX_DIR)/__vmlinux; fi; \
	$(MAKE) -C $(LINUX_DIR) CC=$(CC) SUBDIRS=$$PWD V=$(V) modules \
	&& if test -e $(LINUX_DIR)/__Module.symvers; then mv -f $(LINUX_DIR)/__Module.symvers $(LINUX_DIR)/Module.symvers; fi && if test -e $(LINUX_DIR)/__vmlinux; then mv -f $(LINUX_DIR)/__vmlinux $(LINUX_DIR)/vmlinux; fi

clean::
	$(RM) $(LINUX_DIR)/.tmp_versions/*_rt.mod *.o *.ko *.mod.c .*.cmd *.order *.symvers

else

MODULE_CFLAGS = $(shell rtai-config --module-cflags)

all:: clock.o cmdclk.o cmdcrn.o dispclk.o

clock.o: clock.c
	$(CC) $(MODULE_CFLAGS) -c $<

cmdclk.o: cmdclk.c
	$(CC) $(MODULE_CFLAGS) -c $<

cmdcrn.o: cmdcrn.c
	$(CC) $(MODULE_CFLAGS) -c $<

dispclk.o: dispclk.c
	$(CC) $(MODULE_CFLAGS) -c $<

clean::
	$(RM) *.o

endif

screen: screen.c
	$(CC) $(LXRT_CFLAGS) -o $@ $<

keybrd: keybrd.c
	$(CC) $(LXRT_CFLAGS) -o $@ $<

clean::
	$(RM) screen keybrd

.PHONY: clean
