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

//
// Part 1 implement gpio_set_on, gpio_set_off, gpio_set_output
//

// set <pin> to be an output pin.
//
// NOTE: fsel0, fsel1, fsel2 are contiguous in memory, so you
// can (and should) use ptr calculations versus if-statements!

// Set GPIO <pin> = on.
void gpio_set_on(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
   unsigned *addr = (unsigned *)gpio_set0 + pin / 32;
   pin &= 31;
   put32(addr, (1 << pin));
} 


// Set GPIO <pin> = off
void gpio_set_off(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
   unsigned *addr = (unsigned *)gpio_clr0 + pin / 32;
   pin &= 31;
   put32(addr, (1 << pin));
}

// Set <pin> to <v> (v \in {0,1})
void gpio_write(unsigned pin, unsigned v) {
    if(v)
        gpio_set_on(pin);
    else
        gpio_set_off(pin);
}

//
// Part 2: implement gpio_set_input and gpio_read
//


// Return 1 if <pin> is on, 0 if not.
int gpio_read(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
   unsigned *addr = (unsigned *)gpio_lev0 + pin / 32;
   pin &= 31;
   unsigned out = get32(addr);
   return (out >> pin) & 1;
}

// from page 100:
// Note that it is not possible to read back the current Pull-up/down settings and so it is the
// users’ responsibility to ‘remember’ which pull-up/downs are active
void gpio_set_pulldown(unsigned int pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    PUT32(gpio_pud, 1);
    unsigned *addr = (unsigned *)gpio_pud_clk + pin / 32;
    pin %= 32;
    // wait 150 cycles according to p101
    delay_cycles(150);
    unsigned out = get32(addr);
    put32(addr, out | (1 << pin));
    // wait another 150 cycles
    delay_cycles(150);
    // remove the clock
    put32(addr, out & ~(1 << pin));
}

void gpio_set_pullup(unsigned int pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    PUT32(gpio_pud, 2);
    unsigned *addr = (unsigned *)gpio_pud_clk + pin / 32;
    pin %= 32;
    // wait 150 cycles according to p101
    delay_cycles(150);
    unsigned out = get32(addr);
    put32(addr, out | (1 << pin));
    // wait another 150 cycles
    delay_cycles(150);
    // remove the clock
    put32(addr, out & ~(1 << pin));
}

// implement gpio_set_function for lab 3
void gpio_set_function(unsigned pin, gpio_func_t func) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    if((func & 0b111) != func)
        gpio_panic("illegal func=%x\n", func);
    unsigned offset = pin / 10;
    pin %= 10;
    unsigned *addr = (unsigned *)GPIO_BASE + offset;
    unsigned value = get32(addr);
    unsigned mask = 7 << (pin * 3);
    value &= ~mask;
    value |= (func << (pin * 3));
    put32(addr, value);
}

void gpio_set_output(unsigned pin) {
    gpio_set_function(pin, 1);
}

// set <pin> = input.
void gpio_set_input(unsigned pin) {
    gpio_set_function(pin, 0);
}