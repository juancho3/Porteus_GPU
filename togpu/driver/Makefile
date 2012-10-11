obj-m += xt_TOGPU.o

all: xt_TOGPU xt_TOGPU_launcher

xt_TOGPU-objs := xt_TOGPU_main.o xt_TOGPU_pagewalk.o xt_TOGPU_hook.o xt_TOGPU_symbols.o insn.o inat.o

xt_TOGPU:
	make -C /lib/modules/$(shell uname -r)/build M=`pwd` modules
	mkdir -p /lib/modules/$(shell uname -r)/extra
	rm -f /lib/modules/$(shell uname -r)/extra/xt_TOGPU.ko
	install -m 755 xt_TOGPU.ko /lib/modules/$(shell uname -r)/extra/xt_TOGPU.ko
	/sbin/depmod

xt_TOGPU_launcher:
	gcc -O2 -c xt_TOGPU_launcher.c
	nvcc -O2 -c -arch=sm_20 xt_TOGPU_kernel.cu
	nvcc -link -O2 -arch=sm_20 xt_TOGPU_launcher.o xt_TOGPU_kernel.o -o xt_TOGPU_launcher

clean:
	make -C /lib/modules/$(shell uname -r)/build M=`pwd` clean
	rm -f *.ko
	rm -f *.o
	rm -f xt_TOGPU_launcher

inat.o: inat-tables.h inat.c

inat-tables.h: x86-opcode-map.txt
	awk -f gen-insn-attr-x86.awk $< > $@
