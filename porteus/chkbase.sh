#!/bin/sh
# script by fanthom

# Variables
md5=`which md5sum`
out=/tmp/md5sums.txt$$

# Let's start
if [ "$md5" = "" ]; then
    echo "[31mmd5sum utility is missing - can't perform md5sum check, exiting...""[0m" | fmt -w 80
    exit
fi

echo && echo "[36mChecking md5sums - wait a while...""[0m"
for x in `find ../boot/initrd.xz` `find ../boot/vmlinuz`; do
    md5sum $x >> $out
done

for x in `find ./base/* | sort`; do
	if [ ! -d $x ]; then
	    md5sum $x >> $out
	fi
done

echo
num=`grep -c / md5sums.txt`; x=1
while [ $x -le $num ]; do
    file=`awk NR==$x md5sums.txt |  cut -d " " -f2-`
    first=`awk NR==$x md5sums.txt |  cut -d " " -f1`
    second=`grep $file $out |  cut -d " " -f1`
    if [ "$first" = "$second" ];then
	echo "checking md5sum for $file    [36mok""[0m"
    else
	echo "checking md5sum for $file   [31mfail!""[0m"
	fail="yes"
    fi
    x=$(($x+1))
done
rm $out

echo
if [ "$fail" ]; then
echo "[31mSome md5sums doesn't match or some files from default ISO are missing, please check above...""[0m" | fmt -w 80
echo "[31mBe aware that vmlinuz, initrd.xz, 000-kernel.xzm and 001-core.xzm are most important and booting wont be possible when they are missing.""[0m" | fmt -w 80

else
    echo "[36mAll md5sums are ok, you should have no problems with booting Porteus!""[0m" | fmt -w 80
fi

echo "[36mPress enter to continue or ctrl+c to exit""[0m"
read ans
