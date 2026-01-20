make
insmod source/srsfs.ko
mkdir -p /mnt/srsfs
mount -t srsfs todo /mnt/srsfs
mkdir -p testdata/ && cd testdata/
cat /dev/random | dd count=1 > f
cp f /mnt/srsfs/
echo 'СЕЙЧАС ВНИМАТЕЛЬНО'
diff f /mnt/srsfs/f
echo 'diff не должен был ничего вывести'
cat /dev/random | dd count=10 > ff
cp ff /mnt/srsfs/
diff ff /mnt/srsfs/ff
cd ..
umount /mnt/srsfs
rmmod srsfs
