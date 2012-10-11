#!/bin/bash
# This script will make almost ANY partition bootable, regardless the filesystem
# used on it. bootsyslinux.sh/bootinst.bat is only for FAT filesystems, while this one should
# work everywhere. Moreover it setups a 'changes' directory to be used for
# persistent changes.

set -e
TARGET=""
MBR=""

# Check md5sums before we start
if [ ! -d $portdir ]; then
   echo "Please enter the path to your porteus directory:"
   echo "For example: /mnt/sda5/porteus"
   read portdir
fi
cd $portdir
sh chkbase.sh
cd $bootdir

# Find out which partition or disk are we using
MYMNT=$(cd -P $(dirname $0) ; pwd)
while [ "$MYMNT" != "" -a "$MYMNT" != "." -a "$MYMNT" != "/" ]; do
   TARGET=$(egrep "[^[:space:]]+[[:space:]]+$MYMNT[[:space:]]+" /proc/mounts | cut -d " " -f 1)
   if [ "$TARGET" != "" ]; then break; fi
   MYMNT=$(dirname "$MYMNT")
done

if [ "$TARGET" = "" ]; then
   echo "Can't find device to install to."
   echo "Make sure you run this script from a mounted device."
   exit 1
fi

if [ "$(cat /proc/mounts | grep "^$TARGET" | grep noexec)" ]; then
   echo "The disk $TARGET is mounted with noexec parameter, trying to remount..."
   mount -o remount,exec "$TARGET"
fi

clear
echo "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-"
echo "              Backup Mother Boot Record                      "
echo "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-"
echo
echo "Would you like to backup the MBR of $TARGET now?"
echo "[36m(backing up is recommended)""[0m"
echo "Press  [y/n]"
read bup

if [ "$bup" = "y" ]; then
	dd if=$TARGET of=$MYMNT/mbr.bak bs=512 count=1
	echo
	echo "Backup of mbr is now at $MYMNT/mbr.bak"
	echo "Instructions to restore an MBR in the case of overwriting"
	echo "can be found in boot/docs/restore-mbr.txt"
	echo
	echo "Press enter to continue"
	read abook
fi

MBR=$(echo "$TARGET" | sed -r "s/[0-9]+\$//g")
NUM=$(echo "$TARGET" | sed s^$MBR^^)
cd "$MYMNT"

# only partition is allowed, not the whole disk
if [ "$MBR" = "$TARGET" ]; then
   echo "Error: You must install your system to a partition, not the whole disk"
   exit 1
fi

clear
echo "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-="
echo "             Welcome to Porteus boot installer              "
echo "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-="
echo
echo "This installer will setup disk $TARGET to boot only Porteus."
if [ "$MBR" != "$TARGET" ]; then
   echo
   echo "[31mWarning!""[0m"
   echo "Master boot record MBR of $MBR will be overwritten."
   echo
   echo "[36mIf you use $MBR to boot any existing operating system, it will not"
   echo "work anymore.[0m Only Porteus will boot from this device. Be careful!"
   echo "If you made a backup of your MBR it is at $MYMNT/mbr.bak"
   echo "Read the file boot/docs/restore-mbr.txt"
   echo
   echo "[31mNotice!""[0m"
   echo "When you are installing Porteus on writable media like usb stick/hard drive"
   echo "with FAT or NTFS filesystems, you need to use a save.dat container"
   echo "to save persistent changes. This is not necessary if you are saving your"
   echo "changes to a linux partition. You can create a save.dat container file"
   echo "using the Porteus save.dat Manager tool or Porteus Settings Assistant,"
   echo "both of which are available from the KDE or LXDE menu, under the 'System'"
   echo "heading. Booting will continue in 'Always Fresh' mode, until a save.dat"
   echo "file is created and referenced in your /boot/porteus.cfg file."
   echo "[36mYou have been notified!""[0m"
fi
echo
echo "Press any key to continue, or Ctrl+C to abort..."
read junk
clear

echo "Flushing filesystem buffers, this may take a while..."
sync

if [ -e $MYMNT/boot/lilo.conf ]; then
echo "Existing $MYMNT/boot/lilo.conf has been found - using it for installation..."
else
echo "$MYMNT/boot/lilo.conf does not exist - creating default menu for Porteus..."
cat << ENDOFTEXT >$MYMNT/boot/lilo.conf
boot=$MBR
prompt
#timeout=100
large-memory
lba32
compact
change-rules
reset
install=menu
menu-scheme = Wb:Yr:Wb:Wb
menu-title = "Porteus Boot-Manager"
default = "KDE"

# kde
image=$MYMNT/boot/vmlinuz
initrd=$MYMNT/boot/initrd.xz
label= "KDE"
vga=791
append = "changes=/porteus"

# lxde
image=$MYMNT/boot/vmlinuz
initrd=$MYMNT/boot/initrd.xz
label= "LXDE"
vga=791
append = "lxde changes=/porteus"

# fresh
image=$MYMNT/boot/vmlinuz
initrd=$MYMNT/boot/initrd.xz
label= "Always_Fresh"
append = "nomagic base_only norootcopy"

# cp2ram
image=$MYMNT/boot/vmlinuz
initrd=$MYMNT/boot/initrd.xz
label= "Copy_To_RAM"
vga=791
append = "copy2ram"

# startx
image=$MYMNT/boot/vmlinuz
initrd=$MYMNT/boot/initrd.xz
label= "VESA_mode"
append = "nomodeset autoexec=vesa-mode changes=/porteus"

# text
image=$MYMNT/boot/vmlinuz
initrd=$MYMNT/boot/initrd.xz
label="Text_mode"
append = "autoexec=telinit~3"

# pxe
image=$MYMNT/boot/vmlinuz
initrd=$MYMNT/boot/initrd.xz
label= "PXE_server"
append = "autoexec=pxe-boot~&"

# plop
image=$MYMNT/boot/syslinux/plpbt
label= "PLoP"

# Windows?
#other = /dev/sda1
#label = "First_partition"
ENDOFTEXT
fi

echo "Updating MBR to setup boot record..."
./boot/syslinux/lilo -C $MYMNT/boot/lilo.conf -S $MYMNT/boot/ -m $MYMNT/boot/lilo.map
echo "Disk $MBR should be bootable now. Installation finished."

echo
echo "Read the information above and then press any key to exit..."
read junk
