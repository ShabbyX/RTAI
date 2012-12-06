all: 

SCILAB_VERSION = 5.2.1
SCILAB_DIR = /usr/share/scilab

RTAIDIR = $(shell rtai-config --prefix)

CONTRIBDIR = $(SCILAB_DIR)/contrib/RTAI
LIBDIR     =  $(SCILAB_DIR)/

RTAISRC  = builder.sce loader.sce
RTAISUBDIR = etc macros RT_templates src

install: install_static

install_static:
	mkdir -p $(CONTRIBDIR)
	cd RTAI; cp $(RTAISRC) $(CONTRIBDIR)
	cd RTAI; cp -dpR $(RTAISUBDIR) $(CONTRIBDIR)
	cd $(CONTRIBDIR)/RT_templates; rm rtai_sh.mak;
	cd $(CONTRIBDIR); scilab -nw -f builder.sce; exit
	cd libs; tar xvfz scicoslibs.tgz;
	mkdir -p $(SCILAB_DIR)/libs; cp -dpR libs/*.a $(SCILAB_DIR)/libs
	cd devices; make; make install

install_shared:
	mkdir -p $(CONTRIBDIR)
	cd RTAI; cp $(RTAISRC) $(CONTRIBDIR)
	cd RTAI; cp -dpR $(RTAISUBDIR) $(CONTRIBDIR)
	cd $(CONTRIBDIR)/RT_templates; rm rtai.mak; mv rtai_sh.mak rtai.mak;
	cd $(CONTRIBDIR); scilab -nw -f builder.sce; exit
	cd devices; make; make install

user:
	mkdir -p ~/.Scilab/scilab-$(SCILAB_VERSION)
	cat scilab >> ~/.Scilab/scilab-$(SCILAB_VERSION)/.scilab
