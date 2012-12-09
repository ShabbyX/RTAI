prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)
EXTRA_FLAGS += -O0

ifneq ($(findstring 2.6.,$(shell rtai-config --linux-version 2>/dev/null)),)

LINUX_DIR = $(shell rtai-config --linux-dir)

all:
	$(MAKE) -C $(LINUX_DIR) CC=$(CC) SUBDIRS=$$PWD V=$(V) modules

clean:
	$(RM) $(LINUX_DIR)/.tmp_versions/*_rt.mod *.o *.ko *.mod.c .*.cmd

else

MODULE_CFLAGS = $(shell rtai-config --module-cflags) -O0

all: exectime.o

exectime.o: exectime.c
	$(CC) $(MODULE_CFLAGS) -c $<

clean:
	rm -f *.o

endif

.PHONY: clean
