BOOTLOADER = my-install
# PROGS := tests/partner-test.c
PROGS := tama-fat32-test.c

COMMON_SRC := tama-fat32.c

FAT_32_SRC = ../../labs/16-fat32-readonly/code

COMMON_SRC += ../rtc-driver/rtc.c
COMMON_SRC += ../i2c-sw-driver/i2c-sw.c
COMMON_SRC += ../kalshi-driver/kalshi-interface.c

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

O := $(CS140E_2026_PATH)/libpi
STAFF_OBJS += $(O)/staff-objs/staff-kmalloc.o
# to make sure that it handles GPIO pin 54
STAFF_OBJS += $(O)/staff-objs/gpio.o

# uncomment if you want it to automatically run.
RUN=1

EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|SUCCESS:\|NRF:'
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3
