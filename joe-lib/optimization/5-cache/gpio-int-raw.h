// jreed: readme says to panic if an invalid is pin is given, but for tests to pass you must return

// engler, cs140 put your gpio-int implementations in here.
#include "rpi.h"
#include "gpio-raw.h"

// in libpi/include: has useful enums.
#include "rpi-interrupts.h"

enum {
    GPEDS0=0x20200040,
    GPREN0=0x2020004c,
    GPFEN0=0x20200058,
    GPAREN0=0x2020007c,
    GPAFEN0=0x20200088,
};

// does no error checking, focused on speed
static inline int gpio_event_detected_raw(unsigned pin) {
    dev_barrier();
    // unsigned event_reg = GET32(GPEDS0);
    unsigned event_reg = *(volatile unsigned *)GPEDS0;
    dev_barrier();
    return (event_reg >> pin) & 1;
}

// p96: have to write a 1 to the pin to clear the event.
static inline void gpio_event_clear_raw(unsigned pin) {
    dev_barrier();
    // PUT32(GPEDS0, 1 << pin);
    *(volatile unsigned *)GPEDS0 = 1 << pin;
    dev_barrier();
}
