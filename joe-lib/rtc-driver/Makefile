BOOTLOADER = my-install

# CFLAGS += -I../joe-lib/joe-lib/i2c-sw-driver/.

PROGS += rtc.c

O = $(CS140E_2026_PATH)/libpi
STAFF_OBJS += $(CS140E_2026_PATH)/libpi/staff-objs/staff-hw-spi.o
STAFF_OBJS += $(O)/staff-objs/staff-kmalloc.o

COMMON_SRC += ../i2c-sw-driver/i2c-sw.c
# COMMON_SRC += $(CS140E_2026_PATH)/joe-lib/i2c-sw-driver/i2c-sw.c

TTYUSB = 

RUN = 1

GREP_STR := 'TRACE:\|HASH:\|ERROR:\|PANIC:\|SUCCESS:'
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3

clean::
	make -C tests clean
