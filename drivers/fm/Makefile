# Out-of-tree build for the WCNSS FM driver.
#
#   make -C /path/to/kernel/tree M=$PWD modules
#
# or, with a prepared tree at the usual place:
#
#   make KDIR=~/kbuild/linux-6.16.12
#
# Loading the result needs symbol CRCs matching the running kernel, which
# means building against the same source and config the kernel was built
# from. Compiling only needs a tree that has had `make modules_prepare` run.

obj-m += radio-wcnss-fm.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
