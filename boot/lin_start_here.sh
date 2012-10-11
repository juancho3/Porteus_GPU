#!/bin/sh
# Script to install Porteus to USB device
# Author: Brokenman <brokenman@porteus.org>

set -e
# Set Porteus version
ver="Porteus-v1.2"

# Set variables
bootdir=`pwd`
portdir=../porteus

# safety checks

main_menu(){
clear
echo "[36m                      _.====.._"
echo "                    ,:._       ~-_"
echo "                        '\        ~-_"
echo "                          \        \\."
echo "                        ,/           ~-_"
echo "               -..__..-''   PORTEUS   ~~--..__""[0m"

echo "[32m--==--==--==--==--==--==--==--==--==--==--==--==--==--=="
echo "                    Welcome to $ver      "
echo "--==--==--==--==--==--==--==--==--==--==--==--==--==--==""[0m"
echo
echo "[1] Read Porteus requirements"
echo "[2] Read Porteus installation instructions"
echo "[3] Read Porteus cheatcodes"
echo "[4] Read about bootloaders"
echo "[5] Install Porteus to FATx or EXTx partition (extlinux)"
echo "[6] Install Porteus to any partition (LILO)"
echo "    no graphical - only text menu available"
echo "[7] Quit this application"
echo
echo "Please choose a menu item ..."
read option > /dev/null
echo

while [ "$option" != "7" -a "$option" != "q" ]
        do
                case $option in
                        1 )
                        clear
                        cat docs/requirements.txt|less
                        main_menu
                        exit
                        ;;
                        2 )
			clear
			cat docs/install.txt|less
			main_menu
			exit
                        ;;
                        3 )
			clear
                        cat docs/cheatcodes.txt|less
			main_menu
			exit
                        ;;
                        4 )
			clear
			cat docs/bloaders.txt|less
			main_menu
			exit
                        ;;
			5 )
			echo
			. syslinux/bootextlinux.sh
			exit
                        ;;
                        6 )
                        echo
                        . syslinux/liloinst.sh
                        exit
                        ;;
                esac
done
}

main_menu