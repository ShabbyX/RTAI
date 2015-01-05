prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)

LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags)

all:: display

ifneq ($(findstring 2.6.,$(shell rtai-config --linux-version 2>/dev/null)) || $(findstring 3.,$(shell rtai-config --linux-version 3>/dev/null)),)

LINUX_DIR = $(shell rtai-config --linux-dir)

all:: fp_rt.ko

fp_rt.ko: _fp_rt.ko
	$(LD) -r -static -o $@ $< -L/usr/lib -lm

_fp_rt.ko:
	@if test -e $(LINUX_DIR)/Module.symvers; then mv -f $(LINUX_DIR)/Module.symvers $(LINUX_DIR)/__Module.symvers; fi; if test -e $(LINUX_DIR)/vmlinux; then mv -f $(LINUX_DIR)/vmlinux $(LINUX_DIR)/__vmlinux; fi; \
	$(MAKE) -C $(LINUX_DIR) CC=$(CC) SUBDIRS=$$PWD V=$(V) modules \
	&& if test -e $(LINUX_DIR)/__Module.symvers; then mv -f $(LINUX_DIR)/__Module.symvers $(LINUX_DIR)/Module.symvers; fi && if test -e $(LINUX_DIR)/__vmlinux; then mv -f $(LINUX_DIR)/__vmlinux $(LINUX_DIR)/vmlinux; fi

clean::
	$(RM) $(LINUX_DIR)/.tmp_versions/*_rt.mod *.o *.ko *.mod.c .*.cmd *.order *.symvers

else

MODULE_CFLAGS = $(shell rtai-config --module-cflags)

all:: fp_rt.o

fp_rt.o: fp.c
	$(CC) $(MODULE_CFLAGS) -c $< -o _$@
	$(LD) -r -static -o $@ _$@ -L/usr/lib -lm

clean::
	$(RM) *.o

endif

display: display.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< -lpthread -lm

clean::
	$(RM) display

.PHONY: clean
