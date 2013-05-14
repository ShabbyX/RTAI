all: install

SCILAB_VERSION = 4.1.2
SCILAB_DIR = /usr/local/scilab-$(SCILAB_VERSION)
RTAIDIR = $(shell rtai-config --prefix)

CONTRIBDIR = $(SCILAB_DIR)/contrib/RTAI
MACRODIR   = $(CONTRIBDIR)/macros
DEVICEDIR  = $(CONTRIBDIR)/routines
TEMPLDIR   = $(CONTRIBDIR)/RT_templates

DEVSRC = *.c *.h *.tmp Makefile
MACROSRC = *.sci *.cosf
TEMPLSRC = rtai.gen rtai.mak standalone.cmd
RTAISRC  = builder.sce loader.sce

MACROS_DIR = $(PWD)

install:
	mkdir -p $(MACRODIR)
	mkdir -p $(DEVICEDIR)
	mkdir -p $(TEMPLDIR)
	cp $(RTAISRC) $(CONTRIBDIR)
	cd RTAI; cp $(MACROSRC) $(MACRODIR)
	cd RTAI/RT_templates; cp $(TEMPLSRC) $(TEMPLDIR)
	cd ../devices; cp $(DEVSRC) $(DEVICEDIR)
	cp ../devices/scicos_block4.h $(SCILAB_DIR)/routines/scicos
	cd $(CONTRIBDIR); scilab -nw -f builder.sce
	cd $(CONTRIBDIR)/routines; make install; rm *.o *.a

user:
	mkdir -p ~/.Scilab/scilab-$(SCILAB_VERSION)
	cat scilab >> ~/.Scilab/scilab-$(SCILAB_VERSION)/.scilab
