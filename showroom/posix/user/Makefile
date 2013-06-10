prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)
LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags) -D_GNU_SOURCE
LXRT_LDFLAGS = $(shell rtai-config --lxrt-ldflags)
RENAME = $(shell rtai-config --posix-wrap)

all: condtest jepplin mqtest rwlocks latency

condtest: condtest.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(RENAME) -llxrt $(LXRT_LDFLAGS)

jepplin: jepplin.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(RENAME) -llxrt $(LXRT_LDFLAGS)

mqtest: mqtest.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(RENAME) -llxrt $(LXRT_LDFLAGS)

rwlocks: rwlocks.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(RENAME) -llxrt $(LXRT_LDFLAGS)

latency: latency.c
	$(CC) $(LXRT_CFLAGS) -o $@ $< $(RENAME) -llxrt $(LXRT_LDFLAGS)

clean:
	rm -f *.o condtest jepplin mqtest rwlocks latency

.PHONY: clean
