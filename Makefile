obj-m += srsfs.o

srsfs-objs := source/srsfs.o source/list.o source/srsfs_futil.o source/srsfs_dbg_logs.o

PWD := $(CURDIR)
KDIR = /lib/modules/`uname -r`/build
EXTRA_CFLAGS = -Wall -g

BLK_SZ=16M
BLK_CNT=16

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -rf .cache testtmp

test: all
	insmod srsfs.ko
	mkdir -p /mnt/srsfs/
	mkdir -p testtmp/
	mount -t srsfs todo /mnt/srsfs/
	@echo '==== Testing for $(BLK_CNT) blocks of size $(BLK_SZ) each ===='
	dd if=/dev/random count=$(BLK_CNT) bs=$(BLK_SZ) of=testtmp/f
	dd if=testtmp/f count=$(BLK_CNT) bs=$(BLK_SZ) of=/mnt/srsfs/f
	test "$$(diff testtmp/f /mnt/srsfs/f)" = ""
	@echo '==== dd tests OK ===='
	ln /mnt/srsfs/f /mnt/srsfs/lnf
	test "$$(diff /mnt/srsfs/f /mnt/srsfs/lnf)" = ""
	@echo '==== ln tests OK ===='
	umount /mnt/srsfs
	rmmod srsfs
