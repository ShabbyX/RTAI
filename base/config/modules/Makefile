all:
	$(MAKE) -s -C $(RTAI_LINUX_DIR) CC=$(CC) $(RTAI_LINUX_DIR)/include/linux/modversions.h
	$(MAKE) -s -C $(RTAI_LINUX_DIR) CC=$(CC) ARCH=$(ARCH) SUBDIRS=$(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi) modules

modules:
	@echo RTAI_KMOD_CFLAGS="\"$(CFLAGS) -DEXPORT_SYMTAB -Wall\""
