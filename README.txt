PORTEUS_GPU   (Oct 2012)
-----------

'Porteus GPU' is a live Linux distro based on Porteus (http://porteus.org),
for easy GPU development. With Porteus GPU you can:

- Develop CUDA applications without installing any software in your PC.
- Get a 'ready-to-use' environment with all nVidia GPU development tools.
- Use the latest drivers and tools for GPU development.

In adittion, Porteus_GPU includes 'TOGPU', a Netfilter extension & driver
for GPU network development.

'TOGPU' integrates 'iptables' into GPU network processing.


INSTALLATION
------------

This is a live distro. No installation is required.
Just find an USB device and follow instructions from 'USB INSTALLATION.txt'.

Quick method:

1) Format your USB device with FAT32 filesystem.
2) Copy all 'Porteus_GPU' files into USB device.
3) Setup the bootstrap to allow booting from you USB device:

   Windows: Double click 'boot/win_start_here.hta' from your browser.
   Linux:   Execute 'boot/lin_start_here.sh' from a console.

4) Remove safely the USB device.



And that's all. Plug you USB device on any PC, and boot it by pressing F10/F12
during start-up.


Suggestion and new ideas are welcome.
juanantonio.andres@uam.es
