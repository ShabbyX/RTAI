all: install

SCILAB_VERSION = 4.4.1
SCILAB_DIR = /usr/lib/scicoslab-gtk-$(SCILAB_VERSION)
RTAIDIR = $(shell rtai-config --prefix)

CONTRIBDIR = $(SCILAB_DIR)/contrib/RTAI
MACRODIR   = $(CONTRIBDIR)/macros
DEVICEDIR  = $(CONTRIBDIR)/routines
TEMPLDIR   = $(SCILAB_DIR)/contrib/RT_templates

DEVSRC = *.c *.h *.tmp Makefile
MACROSRC = *.sci *.cosf *.sce
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
	cd RTAI; cp -dpR gif_icons $(MACRODIR)
	cd ../devices; cp $(DEVSRC) $(DEVICEDIR)
	cd $(CONTRIBDIR); scicoslab -nw -f builder.sce
	cd $(CONTRIBDIR)/routines; make install; rm *.o *.a

user:
	mkdir -p ~/.ScicosLab/$(SCILAB_VERSION)
	cat scilab >> ~/.ScicosLab/$(SCILAB_VERSION)/.scilab
