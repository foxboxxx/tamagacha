BOOTLOADER = my-install

PROGS += int-test.c
# PROGS += buy-time.c

O = $(CS140E_2026_PATH)/libpi
STAFF_OBJS += $(CS140E_2026_PATH)/libpi/staff-objs/staff-hw-spi.o
STAFF_OBJS += $(O)/staff-objs/staff-kmalloc.o

COMMON_SRC += ../../font/font.c
COMMON_SRC += ../../display-driver/st7735r-lcd.c
COMMON_SRC += ../../random-lib/random.c
COMMON_SRC += ../../i2c-sw-driver/i2c-sw.c
COMMON_SRC += ../../rtc-driver/rtc.c
COMMON_SRC += ../../kalshi-driver/kalshi-interface.c
COMMON_SRC += ../../fat32/tama-fat32.c
COMMON_SRC += interrupt-asm.S


FAT_32_SRC = ../../../labs/16-fat32-readonly/code
COMMON_SRC += $(FAT_32_SRC)/pi-sd.c \
              $(FAT_32_SRC)/mbr-helpers.c \
              $(FAT_32_SRC)/fat32-helpers.c \
              $(FAT_32_SRC)/fat32-lfn-helpers.c \
              $(FAT_32_SRC)/external-code/unicode-utf8.c \
              $(FAT_32_SRC)/external-code/emmc.c \
			  $(FAT_32_SRC)/fat32.c \
			  $(FAT_32_SRC)/mbr.c

CFLAGS_EXTRA += -I$(FAT_32_SRC)
CFLAGS_EXTRA += -I$(FAT_32_SRC)/external-code

# OPT_LEVEL=-O3 -fno-strict-aliasing
OPT_LEVEL=-O3

TTYUSB = 

RUN = 1

GREP_STR := 'TRACE:\|HASH:\|ERROR:\|PANIC:\|SUCCESS:'
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3

clean::
	make -C tests clean

