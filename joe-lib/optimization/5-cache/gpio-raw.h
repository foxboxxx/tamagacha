/*
 * Implement the following routines to set GPIO pins to input or 
 * output, and to read (input) and write (output) them.
 *  1. DO NOT USE loads and stores directly: only use GET32 and 
 *    PUT32 to read and write memory.  See <start.S> for thier
 *    definitions.
 *  2. DO USE the minimum number of such calls.
 * (Both of these matter for the next lab.)
 *
 * See <rpi.h> in this directory for the definitions.
 *  - we use <gpio_panic> to try to catch errors.  For lab 2
 *    it only infinite loops since we don't have <printk>
 */
#include "rpi.h"

// See broadcomm documents for magic addresses and magic values.
//
// If you pass addresses as:
//  - pointers use put32/get32.
//  - integers: use PUT32/GET32.
//  semantics are the same.
enum {
    // Max gpio pin number.
    GPIO_MAX_PIN = 53,

    GPIO_BASE = 0x20200000,
    gpio_set0  = (GPIO_BASE + 0x1C),
    gpio_clr0  = (GPIO_BASE + 0x28),
    gpio_lev0  = (GPIO_BASE + 0x34),

    // <you will need other values from BCM2835!>
    gpio_pud = (GPIO_BASE + 0x94),
    gpio_pud_clk = (GPIO_BASE + 0x98)
};

// writing optimized inline versions. Will only work for pins <= 31
static inline void gpio_set_on_raw(unsigned pin) {
   unsigned *addr = (unsigned *)gpio_set0;
   // put32(addr, (1 << pin));
    *(volatile uint32_t *)addr = (1 << pin);
} 


// Set GPIO <pin> = off
static inline void gpio_set_off_raw(unsigned pin) {
    // if(pin > GPIO_MAX_PIN)
    //     gpio_panic("illegal pin=%d\n", pin);
   // unsigned *addr = (unsigned *)gpio_clr0 + pin / 32;
   // pin &= 31;
   // put32(addr, (1 << pin));
    unsigned *addr = (unsigned *)gpio_clr0;
    *(volatile uint32_t *)addr = (1 << pin);
}

// Set <pin> to <v> (v \in {0,1})
static inline void gpio_write_raw(unsigned pin, unsigned v) {
    if(v)
        gpio_set_on_raw(pin);
    else
        gpio_set_off_raw(pin);
}

// Return 1 if <pin> is on, 0 if not.
static inline int gpio_read_raw(unsigned pin) {
   unsigned *addr = (unsigned *)gpio_lev0;
   // unsigned out = get32(addr);
   unsigned out = *(volatile unsigned *)addr;
   return (out >> pin) & 1;
}
