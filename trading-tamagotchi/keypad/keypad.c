#include "rpi.h"
#include "vector-base.h"
#include "timer-interrupt.h"

void disable_interrupts(void);
void enable_interrupts(void);

static unsigned *gpio_buttons;
static unsigned num_buttons;

static int (*interrupt_fn)(uint32_t pc);


// jreed testing. No buttons, so manually writing values (so pins should be output
// instead of inpur)
void set_buttons_async_testing(unsigned *gpio_buttons, unsigned num_buttons) {
    for (int i = 0; i < num_buttons; i++) {
        // printk("hanlding pin %d", gpio_buttons[i]);
        gpio_set_output(gpio_buttons[i]);
        gpio_write(gpio_buttons[i], 1);
        gpio_set_pullup(gpio_buttons[i]);
        gpio_fiq_async_falling_edge(gpio_buttons[i]);
        gpio_int_async_falling_edge(gpio_buttons[i]);
    }
}

// which gpio pins each of the four pins are connected to
// array ordered so that key 1 = idx 0, key 2 = idx 1, etc.
void set_buttons(unsigned *gpio_buttons, unsigned num_buttons) {
    for (int i = 0; i < num_buttons; i++) {
        // printk("hanlding pin %d", gpio_buttons[i]);
        gpio_set_input(gpio_buttons[i]);
        gpio_set_pullup(gpio_buttons[i]);
        gpio_int_falling_edge(gpio_buttons[i]);
    }
}


int button_int_handler() {
    // check each button. Is there a way to make this faster?
    // TODO: make this faster by reading reg once and bit ops.
    // also consider adding some type of delay since
    // user press time can sometimes cause multiple events
    printk("in button interrupt handler!\n");
    for (int i = 0; i < num_buttons; i++) {
        unsigned cur_pin = gpio_buttons[i];
        if (gpio_event_detected(cur_pin)) {
            printk("----------EVENT------------\n");
            printk("button %d (pin %d) was pressed!", i, cur_pin);
            printk("----------EVENT------------\n");
            gpio_event_clear(cur_pin);
            return 1;
        }
    }
    return 0;
}

int dispatch_interrupt(uint32_t pc) {
    if (interrupt_fn) return interrupt_fn(pc);
    return 0;
}

// void keypad_init(unsigned *in_buttons, unsigned in_num, int (*handler)(uint32_t pc)) {
void keypad_init(unsigned *in_buttons, unsigned in_num, int (*handler)()) {
    gpio_buttons = in_buttons;
    num_buttons = in_num;
    // make sure system interrupts are off.
    disable_interrupts();

    interrupt_fn = handler ? handler : button_int_handler;

    // just like lab 4-interrupts: force clear 
    // interrupt state
    //  BCM2835 manual, section 7.5 , 112
    dev_barrier();
    PUT32(IRQ_Disable_1, 0xffffffff);
    PUT32(IRQ_Disable_2, 0xffffffff);
    dev_barrier();
    
    // setup the vector.
    extern uint32_t interrupt_vec[];
    vector_base_set(interrupt_vec);

    // set_buttons(gpio_buttons, num_buttons);
    // use testing version: jreed
    set_buttons_async_testing(gpio_buttons, num_buttons);

    // interrupt_fn = button_int_handler;

    // start global interrupts.
    enable_interrupts();
}
