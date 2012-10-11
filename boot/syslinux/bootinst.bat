@echo off
cls
set DISK=none
set BOOTFLAG=boot666s.tmp

echo This file is used to determine current drive letter. It should be deleted. >\%BOOTFLAG%
if not exist \%BOOTFLAG% goto readOnly

echo Wait please, searching for current drive letter.
for %%d in ( C D E F G H I J K L M N O P Q R S T U V W X Y Z ) do if exist %%d:\%BOOTFLAG% set DISK=%%d

if %DISK% == C goto BackupMbr
goto StartNow

:BackupMbr
cls
\boot\tools\MbrFix /drive 0 savembr mbr.bak
echo.
echo It appears that drive C: has been chosen! Be aware that
echo if you have another OS on this partition it may not boot
echo any longer. A backup of your MBR has been made just in case
echo something goes wrong. It is saved as C:\boot\mbr.bak file.
echo.
echo For instructions on how to restore your MBR
echo exit this script and run: C:\boot\tools\MbrFix
echo.
echo Press enter key to continue or ctrl + c to exit.
pause > nul

:StartNow
cls
del \%BOOTFLAG%
if %DISK% == none goto DiskNotFound

echo =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
echo                          Welcome to Porteus boot installer
echo =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
echo.
echo This installer will setup disk %DISK%: to boot only Porteus.
echo.
echo Warning! Master Boot Record (MBR) of the device %DISK%: will be overwritten.
echo If %DISK%: is a partition on the same disk drive like your Windows installation,
echo then your Windows will not boot anymore. Be careful!
echo.
echo Notice!
echo When you are installing Porteus on writable media (usb stick/hard drive)
echo with FAT or NTFS filesystems, you need to use a save.dat container
echo to save persistent changes (this is not necessary if you are saving your
echo changes to a linux partition). You can create a save.dat container file
echo using the Porteus save.dat Manager tool or Porteus Settings Assistant,
echo both of which are available from the KDE or LXDE menu, under the 'System'
echo heading. Booting will continue in 'Always Fresh' mode, until a save.dat
echo file is created and referenced in your /boot/porteus.cfg file.
echo You have been notified!
echo.
echo Press any key to continue, or kill this window [x] to abort...
pause > nul

cls
echo Setting up boot record for %DISK%:, wait please...

if %OS% == Windows_NT goto setupNT
goto setup95

:setupNT
\boot\syslinux\syslinux.exe -maf -d \boot\syslinux %DISK%:
goto setupDone

:setup95
\boot\syslinux\syslinux.com -maf -d \boot\syslinux %DISK%:

:setupDone
echo Disk %DISK%: should be bootable now. Installation finished.
goto pauseit

:readOnly
echo You're starting Porteus installer from a read-only media, this will not work.
goto pauseit

:DiskNotFound
echo Error: can't find out current drive letter

:pauseit
echo.
echo Read the information above and then press any key to exit...
pause > nul

:end
