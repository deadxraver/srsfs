make
insmod source/srsfs.ko
mkdir -p /mnt/srsfs
mount -t srsfs todo /mnt/srsfs
mkdir -p testdata/ && cd testdata/
echo '' > f
for i in {0..1030}; do echo 1 >> f; done
cp f /mnt/srsfs/
echo 'СЕЙЧАС ВНИМАТЕЛЬНО'
diff f /mnt/srsfs/f
echo 'diff не должен был ничего вывести'
cd ..
umount /mnt/srsfs
rmmod srsfs
