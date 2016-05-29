# Basic Makefile to pull in kernel's KBuild to build an out-of-tree
# kernel module

KDIR ?= /lib/modules/`uname -r`/build

all: babble-reader
	$(MAKE) -C $(KDIR) M=$$PWD

babble-reader: babble-reader.c
	$(CC) -O2 -Wall -o $@ $<

clean:
	-rm babble-reader *.o *.ko modules.order Module.symvers *.mod.c

