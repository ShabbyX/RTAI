# Copyright 2006 Pierangelo Masarati
# All rights reserved.
#
# Use this software as you like, according to GNU's GPL,
# provided you do not alter this Copyright statement.
# This software is provided AS IS, without any warranty.

prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)
LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags)
LXRT_LDFLAGS = $(shell rtai-config --lxrt-ldflags)

PREFIX      = $(prefix)/share/rtai/matrixx
MATRIXX_SRC = $(PREFIX)/matrixx-files
MATRIXX_DEV = $(PREFIX)/devices

DOUBLE_FLAG = -DAC100DOUBLE
FILEIO_FLAG = -DFILE_IO

CPPFLAGS =  $(LXRT_CFLAGS) -I/usr/include \
	-I$(MATRIXX_SRC)  \
	-I$(MATRIXX_DEV)  \
	-I$(PREFIX)  \
	-I$(PREFIX)/vxworks-wrappers/ -I./ \
        -DMSG_Q_PRIORITY -DRS_VXWORKS $(DOUBLE_FLAG) $(FILEIO_FLAG)
CFLAGS = -O2 -g -Wall
LIBS = $(LXRT_LDFLAGS) -lm -llxrt -L$(PREFIX)/lib/

SRC =   rtmain.c   \
	$(MATRIXX_SRC)/sa_fuzzy.c \
	$(MATRIXX_SRC)/sa_math.c \
	$(PREFIX)/sa_utils.c \
	$(MATRIXX_DEV)/rtai_scope.c \
	$(MATRIXX_DEV)/rtai_log.c \
	$(MATRIXX_DEV)/rtai_meter.c \
	$(MATRIXX_DEV)/rtai_led.c \
	$(MATRIXX_DEV)/rtai_comedi_datain.c \
	$(MATRIXX_DEV)/rtai_comedi_dataout.c \
	$(MATRIXX_DEV)/rtai_comedi_dioin.c \
	$(MATRIXX_DEV)/rtai_comedi_dioout.c


%: %.c mxp mxp.c mxp.h $(SRC) rtmain.h
	./mxp < $@.c
	$(CC) $(CFLAGS) $(DOUBLE_FLAG) -I$(MATRIXX_SRC)  -DMXP_LIB=1 -o mxp.o -c mxp.c
	$(CC) $(CPPFLAGS)  -o $@ $@.c $(SRC) mxp.o $(LIBS)

mxp: mxp.c mxp.h
	$(CC) $(CFLAGS) $(DOUBLE_FLAG) -I$(MATRIXX_SRC) -o mxp mxp.c

mxp.c:
	cp $(PREFIX)/mxp.c .

mxp.h:
	cp $(PREFIX)/mxp.h .

rtmain.c:
	cp $(PREFIX)/rtmain.c .

rtmain.h:
	cp $(PREFIX)/rtmain.h .

clean:
	rm -f *.o mxp mxp_data.h mxp_data.hc mxp.c mxp.h rtmain.c rtmain.h

.PHONY: clean

