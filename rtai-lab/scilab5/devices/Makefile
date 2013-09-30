LIB = libsciblk.a
all: $(LIB)

RTAIDIR = $(shell rtai-config --prefix)
COMEDIDIR = $(shell rtai-config --comedi-dir)
RANLIB = ranlib
AR = ar
SHAREDLIB = libsciblk.so
LIB = libsciblk.a

SRCALL = $(wildcard *.c)

ifeq ($(strip $(COMEDIDIR)),)
EXCLUDE = exit_on_error.c rtai_comedi_datain.c rtai_comedi_dataout.c rtai_comedi_dioin.c rtai_comedi_dioout.c rtai_comedi_encoder.c get_softcalib.c
else
EXCLUDE = exit_on_error.c
endif

SRC=$(filter-out $(EXCLUDE),$(SRCALL))

OBJ = $(SRC:.c=.o)
DBG = -g

#CC_FLAGS = -c $(DBG) -I. -I$(RTAIDIR)/include -O2 -Wall -std=gnu99 -DSOFTCALIB
CC_FLAGS = -c $(DBG) -D RTAI=1 -D _TTY_POSIX_=1 -I. -I$(RTAIDIR)/include -O2 -fno-stack-protector $(CC_PICFLAGS)  

%.o: %.c
	cc $(CC_FLAGS) $<

%.o: %.cpp
	cc $(CC_FLAGS) $<

$(LIB): $(OBJ)
	$(AR) -r $(LIB)  `ls *.o | sort | uniq`;
	gcc -shared -Wl,-soname,$(SHAREDLIB).1 -o $(SHAREDLIB).1.0.0 `ls *.o | sort | uniq` ;
	ln -sf $(SHAREDLIB).1.0.0 $(SHAREDLIB).1;
	ln -sf $(SHAREDLIB).1.0.0 $(SHAREDLIB);

install: $(LIB)
	cp $(LIB) $(RTAIDIR)/lib
	cp -a $(SHAREDLIB) $(RTAIDIR)/lib
	cp -a $(SHAREDLIB).* $(RTAIDIR)/lib

clean:
	rm -f $(LIB) $(OBJ)
