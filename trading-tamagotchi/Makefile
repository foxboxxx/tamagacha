BOOTLOADER = my-install
# PROGS := tests/partner-test.c
PROGS := pi_write_test.c

# COMMON_SRC := write_from_pi.c

# TODO: future use? also, might want to change to tamagotchi path if needed
# STAFF_OBJS += $(CS140E_2026_PATH)/libpi/staff-objs/staff-hw-spi.o
# STAFF_OBJS += $(CS140E_2026_PATH)/libpi/staff-objs/staff-kmalloc.o

# uncomment if you want it to automatically run.
RUN=0

EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|SUCCESS:\|NRF:'
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3
