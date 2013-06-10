EXTRA_CFLAGS += $(shell rtai-config --module-cflags)

obj-m += handler_rt.o

handler_rt-objs := handler.o

prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)
LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags)
LXRT_LDFLAGS = $(shell rtai-config --lxrt-ldflags)

all: resumefromintr

resumefromintr: resumefromintr.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(LXRT_LDFLAGS)

clean:
	rm -f *.o resumefromintr

.PHONY: clean
