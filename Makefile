KCONFIG_DIR=./base/config/kconfig
CC ?= gcc
#CC ?= gcc -Wall -pedantic

ifneq ($(MAKECMDGOALS),help)
ifeq ($(srctree),)
ifneq ($(MAKEFILE_LIST),)
# Since 3.80, we can find out which Makefile is currently processed,
# and infere the location of the source tree using MAKEFILE_LIST.
srctree := $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
else
ifeq ($(srctree),)
srctree := $(shell test -d base/config && pwd)
ifeq ($(srctree),)
$(error Please specify the location of your source tree: make srctree=...)
endif
endif
endif
endif
build_alias := $(shell $(srctree)/base/config/autoconf/config.guess)
host_alias := $(shell  $(srctree)/base/config/autoconf/arch2host.sh $(ARCH) $(build_alias))
override ARCH := $(shell echo $(host_alias)|cut -f1 -d-|sed -e s/^i.86/x86/ -e s/^x86_64/x86/ -e s/^arm.*/arm/ -e s/^sa110/arm/ -e s/^powerpc/ppc/)
endif

ifeq ($(MAKECMDGOALS),)
confgui = $(shell test $(TERM) = emacs && echo xconfig || echo menuconfig)
forceconf = $(shell test -r .rtai_config -a -r .cfok || echo $(confgui))
endif

override srctree := $(shell cd $(srctree) && pwd)

all: $(forceconf)
	@if test -r GNUmakefile ; then \
	$(MAKE) -f GNUmakefile $@ ; else \
	echo "*** Please configure RTAI first (running 'make help' in RTAI's toplevel dir might help)." ; \
	exit 1; fi

CONFIG_METHODS := config nconfig menuconfig xconfig gconfig oldconfig silentoldconfig defconfig \
	savedefconfig allnoconfig allyesconfig allmodconfig alldefconfig \
	randconfig listnewconfig olddefconfig
.PHONY: $(CONFIG_METHODS)
$(CONFIG_METHODS):
	@$(MAKE) -C base/config/kconfig \
	-f Makefile.kconfig $@ \
	srctree=$(PWD) ARCH=`uname -m` CONFIG_RTAI_QUIET=quiet_
	@$(MAKE) config.status


reconfig:
	@touch .rtai_config
	@$(MAKE) config.status

config.status: .rtai_config
	@test -r config.status && recf=yes || recf=no ; \
	eval `grep ^CONFIG_RTAI_INSTALLDIR $<`; \
	eval `grep ^CONFIG_RTAI_MAINTAINER $<`; \
	test x$$CONFIG_RTAI_MAINTAINER_AUTOTOOLS = xy && confopts=--enable-maintainer-mode; \
	CROSS_COMPILE=$(CROSS_COMPILE) \
	CC="$(CROSS_COMPILE)$(CC)" \
	CXX="$(CROSS_COMPILE)$(CXX)" \
	LD="$(CROSS_COMPILE)$(LD)" \
	AR="$(CROSS_COMPILE)$(AR)" \
	RANLIB=$(CROSS_COMPILE)ranlib \
	STRIP=$(CROSS_COMPILE)strip \
	NM=$(CROSS_COMPILE)nm \
	$(srctree)/configure \
	--build=$(build_alias) \
	--host=$(host_alias) \
	--with-kconfig-file=./$< \
	--with-linux-dir=$(RTAI_LINUX_DIR) \
	--prefix=$$CONFIG_RTAI_INSTALLDIR \
	$$confopts ; \
	if test $$? = 0; then \
	   touch .cfok ; \
	   if test x$$recf = xyes ; then \
	      touch .cfchanged ; \
	   fi ; \
	else \
	   rm -f .cfok ; false; \
	fi

$(KCONFIG_DIR):
	@test -d $@ || mkdir -p $@

help:
	@echo ; echo "This is RTAI's build bootstrapping Makefile. In order to build RTAI," ; \
	echo 'you first need to configure it. Proceed as follows:' ; \
	echo ; echo '$$ cd $$your_builddir' ; echo ; \
	echo '# Configuration using a KDE-based, GTK-based or Dialog-based GUI' ; \
	echo '$$ make -f $$rtai_srcdir/makefile srctree=$$rtai_srcdir {xconfig,gconfig,menuconfig}' ; \
	echo '                            OR,' ; \
	echo '# Configuration using a text-based interface' ; \
	echo '$$ make -f $$rtai_srcdir/makefile srctree=$$rtai_srcdir {config,oldconfig}' ; \
	echo ; echo 'In case a configuration error occurs, re-run the command above to fix the' ; \
	echo 'faulty parameter. Once the configuration is successful, type:' ; echo ; \
	echo '$$ make [all]' ; \
	echo ; echo "To change the configuration from now on, simply run:"; echo ; \
	echo '$$ make {xconfig,gconfig,menuconfig,config}' ; echo

clean distclean:
	if test -r $(KCONFIG_DIR)/Makefile.kconfig ; then \
	$(MAKE) -C $(KCONFIG_DIR) -f Makefile.kconfig clean ; fi
	if test -r GNUmakefile ; then \
	$(MAKE) -f GNUmakefile $@ ; else \
	$(MAKE) -C $(KCONFIG_DIR) -f $(srctree)/$(KCONFIG_DIR)/Makefile.kconfig clean ; \
	fi
	@find . -name autom4te.cache | xargs rm -fr

.PHONY: reconfig clean distclean help
