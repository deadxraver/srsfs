obj-m += source/srsfs.o

PWD := $(CURDIR)
KDIR = /lib/modules/`uname -r`/build
EXTRA_CFLAGS = -Wall -g

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -rf .cache testtmp

test: all
	insmod source/srsfs.ko
	mkdir -p /mnt/srsfs/
	mkdir -p testtmp/
	mount -t srsfs todo /mnt/srsfs/
	cat /dev/random | dd count=1 > testtmp/f
	cp testtmp/f /mnt/srsfs/
	test "$$(diff testtmp/f /mnt/srsfs/f)" = ""
	cat /dev/random | dd count=10 > testtmp/ff
	cp testtmp/ff /mnt/srsfs/
	test "$$(diff testtmp/ff /mnt/srsfs/ff)" = ""
	@echo 'cp tests OK'
	ln /mnt/srsfs/ff /mnt/srsfs/lnff
	test "$$(diff /mnt/srsfs/ff /mnt/srsfs/lnff)" = ""
	@echo 'ln tests OK'
	umount /mnt/srsfs
	rmmod srsfs
