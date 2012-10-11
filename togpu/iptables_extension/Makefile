.PHONY: all install clean distclean FORCE

XT_IPTABLES_VERSION = $(shell `which iptables` -V | cut -d " " -f 2 | cut -b 2-)
IPT_FIRST_NUM = $(shell echo ${XT_IPTABLES_VERSION} | cut -d "." -f 1 | cut -d "-" -f 1)
IPT_SECOND_NUM = $(shell echo ${XT_IPTABLES_VERSION} | cut -d "." -f 2 | cut -d "-" -f 1)
IPT_THIRD_NUM = $(shell echo ${XT_IPTABLES_VERSION} | cut -d "." -f 3 | cut -d "-" -f 1)
IPTVERSION = $(IPT_FIRST_NUM)$(IPT_SECOND_NUM)$(IPT_THIRD_NUM)
IPTABLES_VERSION_STRING = \"$(IPT_FIRST_NUM).$(IPT_SECOND_NUM).$(IPT_THIRD_NUM)\"

CFLAGS := -fPIC -Wall -MMD -D_LARGEFILE_SOURCE=1 -D_LARGE_FILES -D_FILE_OFFSET_BITS=64 \
          -D_REENTRANT -pipe -DXTABLES_LIBDIR=\"/usr/libexec/xtables\" -DXTABLES_INTERNAL \
          -D_INIT=libxt_TOGPU_init -DPIC -fPIC -O2 \
          -DIPTVERSION=$(IPTVERSION) \
          -DIPTABLES_VERSION_STRING=$(IPTABLES_VERSION_STRING)

all: libxt_TOGPU.so

clean:
	rm -f *.o *.oo *.so *.a

lib%.so: lib%.oo
	${LD} -shared ${LDFLAGS} -o $@ $< -lxtables

lib%.oo: lib%.c
	${CC} ${CFLAGS} -o $@ -c $<

