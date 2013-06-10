prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

ARCH = $(shell rtai-config --arch)
# If an arch-dependent dir is found, process it too.
ARCHDIR = $(shell test -d $(ARCH) && echo $(ARCH) || echo)

all %::
	for dir in $(TARGETS) $(ARCHDIR); do $(MAKE) -C $$dir $@; done

.PHONY: $(TARGETS)
