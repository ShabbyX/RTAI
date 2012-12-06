SRC := $(wildcard *.c)

MEX = /usr/local/matlab7/bin/mex
MEX_FLAGS= -v

all: $(SRC:.c=.mexglx)

%.mexglx: %.c
	$(MEX) $(MEX_FLAGS) $<

clean:
	rm -f *.mexglx

install:

%::
	true
