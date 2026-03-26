BOOTLOADER = my-install
PROGS := pi-kalshi-comms.c

# COMMON_SRC := pi-kalshi.c

# uncomment if you want it to automatically run.
RUN=0

EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|SUCCESS:\|NRF:'
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3
