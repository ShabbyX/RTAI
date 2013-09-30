# Makefile generate from template rtai.mak
# ========================================

all: ../$$MODEL$$

RTAIDIR = $(shell rtai-config --prefix)
C_FLAGS = $(shell rtai-config --lxrt-cflags)
SCIDIR = $$SCILAB_DIR$$
COMEDIDIR = $(shell rtai-config --comedi-dir)
ifneq ($(strip $(COMEDIDIR)),)
COMEDILIB = -lcomedi
endif 

RM = rm -f
FILES_TO_CLEAN = *.o ../$$MODEL$$

CC = gcc
CC_OPTIONS = -O -DNDEBUG -Dlinux -DNARROWPROTO -D_GNU_SOURCE

MODEL = $$MODEL$$
OBJSSTAN = rtmain44.o common.o $$MODEL$$.o $$OBJ$$

SCILIBS = $(SCIDIR)/libs/scicos.a $(SCIDIR)/libs/poly.a $(SCIDIR)/libs/calelm.a $(SCIDIR)/libs/blas.a $(SCIDIR)/libs/lapack.a $(SCIDIR)/libs/blas.a $(SCIDIR)/libs/os_specific.a
OTHERLIBS = 
ULIBRARY = $(RTAIDIR)/lib/libsciblk.a $(RTAIDIR)/lib/liblxrt.a

CFLAGS = $(CC_OPTIONS) -O2 -I$(SCIDIR)/routines -I$(SCIDIR)/routines/scicos $(C_FLAGS) -DMODEL=$(MODEL) -DMODELN=$(MODEL).c

rtmain44.c: $(RTAIDIR)/share/rtai/scicos/rtmain44.c $(MODEL).c
	cp $< .

../$$MODEL$$: $(OBJSSTAN) $(ULIBRARY)
	gcc -static -o $@  $(OBJSSTAN) $(SCILIBS) $(ULIBRARY) -lpthread $(COMEDILIB) -lgfortran -lm
	@echo "### Created executable: $(MODEL) ###"

clean::
	@$(RM) $(FILES_TO_CLEAN)
