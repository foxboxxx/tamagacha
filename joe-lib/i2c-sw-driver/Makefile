BOOTLOADER = my-install

PROGS += i2c-sw.c

O = $(CS140E_2026_PATH)/libpi
STAFF_OBJS += $(CS140E_2026_PATH)/libpi/staff-objs/staff-hw-spi.o
STAFF_OBJS += $(O)/staff-objs/staff-kmalloc.o

TTYUSB = 

RUN = 1

GREP_STR := 'TRACE:\|HASH:\|ERROR:\|PANIC:\|SUCCESS:'
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3

clean::
	make -C tests clean
