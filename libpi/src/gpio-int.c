// jreed: readme says to panic if an invalid is pin is given, but for tests to pass you must return

// engler, cs140 put your gpio-int implementations in here.
#include "rpi.h"

// in libpi/include: has useful enums.
#include "rpi-interrupts.h"

enum {
    GPEDS0=0x20200040,
    GPREN0=0x2020004c,
    GPFEN0=0x20200058,
    GPAREN0=0x2020007c,
    GPAFEN0=0x20200088,
};

// returns 1 if there is currently a GPIO_INT0 interrupt, 
// 0 otherwise.
//
// note: we can only get interrupts for <GPIO_INT0> since the
// (the other pins are inaccessible for external devices).
int gpio_has_interrupt(void) {
    // todo("implement: is there a GPIO_INT0 interrupt?\n");

    // enable GPIO address before BCM interrupt controller
    // for (int i = 0; i < 32; i++) {
    //     gpio_int_rising_edge(i);
    //     gpio_int_falling_edge(i);
    // }
    // // should not RMW to irq enable
    // PUT32(IRQ_Enable_1, 49);

    dev_barrier();
    int out = (GET32(IRQ_pending_2) >> (GPIO_INT0 - 32)) & 1;
    dev_barrier();
    return out;

}

// p97 set to detect rising edge (0->1) on <pin>.
// as the broadcom doc states, it  detects by sampling based on the clock.
// it looks for "011" (low, hi, hi) to suppress noise.  i.e., its triggered only
// *after* a 1 reading has been sampled twice, so there will be delay.
// if you want lower latency, you should us async rising edge (p99)
//
// also have to enable GPIO interrupts at all in <IRQ_Enable_2>
void gpio_int_rising_edge(unsigned pin) {
    if(pin>=32)
        panic("unsupported pin: %d", pin);
    dev_barrier();
    OR32(GPREN0, 1 << (pin & 31));
    dev_barrier();
    PUT32(IRQ_Enable_2, 1 << (GPIO_INT0 - 32));
    dev_barrier();
}

// p98: detect falling edge (1->0).  sampled using the system clock.  
// similarly to rising edge detection, it suppresses noise by looking for
// "100" --- i.e., is triggered after two readings of "0" and so the 
// interrupt is delayed two clock cycles.   if you want  lower latency,
// you should use async falling edge. (p99)
//
// also have to enable GPIO interrupts at all in <IRQ_Enable_2>
void gpio_int_falling_edge(unsigned pin) {
    if(pin>=32)
        panic("unsupported pin: %d", pin);
    dev_barrier();
    OR32(GPFEN0, 1 << (pin & 31));
    dev_barrier();
    PUT32(IRQ_Enable_2, 1 << (GPIO_INT0 - 32));
    dev_barrier();
}


// p96: a 1<<pin is set in EVENT_DETECT if <pin> triggered an interrupt.
// if you configure multiple events to lead to interrupts, you will have to 
// read the pin to determine which caused it.
int gpio_event_detected(unsigned pin) {
    if(pin>=32)
        panic("unsupported pin: %d", pin);
    dev_barrier();
    unsigned event_reg = GET32(GPEDS0);
    dev_barrier();
    return (event_reg >> (pin & 31)) & 1;
}

// p96: have to write a 1 to the pin to clear the event.
void gpio_event_clear(unsigned pin) {
    if(pin>=32)
        panic("unsupported pin: %d", pin);
    dev_barrier();
    PUT32(GPEDS0, 1 << pin);
    dev_barrier();
}


// ------------ Trading Tamagotchi Routines -------------

// jreed: setup async edges for lower latency with tamagotchi
// pg 99
void gpio_int_async_rising_edge(unsigned pin) {
    if(pin>=32)
        panic("unsupported pin: %d", pin);
    dev_barrier();
    OR32(GPREN0, 1 << (pin & 31));
    dev_barrier();
    PUT32(IRQ_Enable_2, 1 << (GPIO_INT0 - 32));
    dev_barrier();
}

void gpio_int_async_falling_edge(unsigned pin) {
    if(pin>=32)
        panic("unsupported pin: %d", pin);
    dev_barrier();
    OR32(GPAFEN0, 1 << (pin & 31));
    dev_barrier();
    PUT32(IRQ_Enable_2, 1 << (GPIO_INT0 - 32));
    dev_barrier();
}

// same as above, but for fast interrupts. Should never
// be enabled at the same time as above, or any other fiq routine
void gpio_fiq_async_rising_edge(unsigned pin) {
    if(pin>=32)
        panic("unsupported pin: %d", pin);
    dev_barrier();
    OR32(GPREN0, 1 << (pin & 31));
    dev_barrier();
    // from page 116. Bit 7 is enbale, lower 7 bits are source
    PUT32(IRQ_FIQ_control, (1 << 7) | GPIO_INT0);
    dev_barrier();
}

void gpio_fiq_async_falling_edge(unsigned pin) {
    if(pin>=32)
        panic("unsupported pin: %d", pin);
    dev_barrier();
    OR32(GPAFEN0, 1 << (pin & 31));
    dev_barrier();
    // from page 116. Bit 7 is enbale, lower 7 bits are source
    PUT32(IRQ_FIQ_control, (1 << 7) | GPIO_INT0);
    dev_barrier();
}
