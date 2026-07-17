SUBDIRS := $(dir $(wildcard */Makefile))
.PHONY: all clean cpsysroot $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ ARCH=$(ARCH)

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

cpsysroot:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir cpsysroot; \
	done