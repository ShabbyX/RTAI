prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)
# Add -O0 to prevent optimization.  Otherwise the test shows very small values
LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags) -O0
LXRT_LDFLAGS = $(shell rtai-config --lxrt-ldflags)

all: exectime

exectime: exectime.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(LXRT_LDFLAGS) -llxrt

clean:
	rm -f *.o exectime

.PHONY: clean
