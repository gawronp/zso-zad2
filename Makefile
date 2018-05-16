CCFLAGS=-std=gnu11
KDIR ?= /lib/modules/`uname -r`/build

default: zso5_set_repeats
	$(MAKE) -C $(KDIR) M=$$PWD

install:
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
	rm -f zso5_set_repeats

zso5_set_repeats: zso5_set_repeats.c
	gcc zso5_set_repeats.c -o zso5_set_repeats
