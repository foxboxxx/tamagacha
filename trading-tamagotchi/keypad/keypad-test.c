#include "rpi.h"
#include "keypad.h"

// which gpio pins each of the four pins are connected to
// array ordered so that key 1 = idx 0, key 2 = idx 1, etc.
// unsigned gpio_buttons[4] = {20, 18, 24, 22};
unsigned gpio_buttons[4] = {17, 19, 21, 23};
unsigned num_buttons = 4;

static unsigned last_press_time = 0;
#define DEBOUNCE_US 250000 // 250 ms debounce so no repeatd button presses
static uint32_t count = 0;

void keypad_and_led_test() {
    printk("inside keypad test");
    unsigned led = 3;
    // set_buttons(gpio_buttons, num_buttons);
    for (int i = 0; i < num_buttons; i++) {
        gpio_set_input(gpio_buttons[i]);
        gpio_set_pullup(gpio_buttons[i]);
    }
    printk("after init\n");
    while(1) {
        printk("new iteration!\n");
        // poll each pin and see if it is low
        for (int i = 0; i < num_buttons; i++) {
            if (!gpio_read(gpio_buttons[i])) {
                printk("----------EVENT------------\n");
                printk("button %d (pin %d) is low!", i, gpio_buttons[i]);
                printk("----------EVENT------------\n");
            }
        }
        delay_cycles(1500000 * 3);
    }

}

int custom_handler() {
    printk("in custom handler\n");
    unsigned curr_press_time = timer_get_usec();
    // if ((curr_press_time - last_press_time) < DEBOUNCE_US) return 1;
    for (int i = 0; i < num_buttons; i++) {
        unsigned cur_pin = gpio_buttons[i];
        if (gpio_event_detected(cur_pin)) {
            // printk("----------EVENT------------\n");
            // printk("button %d (pin %d) was pressed!", i, cur_pin);
            // printk("----------EVENT------------\n");
            // printk("%d\n", i);
            if ((curr_press_time - last_press_time) >= DEBOUNCE_US) {
                last_press_time = curr_press_time;
                count++;
            }
            gpio_event_clear(cur_pin);
            // jreed: write it back high since using output pins
            gpio_write(cur_pin, 1);
            return 1;
        }
    }
    return 0;
}

// routine that waits for interrupts, spins infinetly
void wait_for_interrupt() {
    unsigned ctr = 0;
    while(1) {
        printk("waiting. iteration %d\n", ctr);
        // since I don't have the buttons anymore, manually trigger interrupt by writing one of the buttons to low
        if (ctr % 5 == 0) {
            unsigned cur_pin = gpio_buttons[((ctr / 5) % num_buttons)];
            gpio_write(cur_pin, 0);
        }
        delay_cycles(1500000 * 2);
        ctr++;
    }
}

void notmain() {
    printk("Beginning keypad test!\n");
    keypad_init(gpio_buttons, num_buttons, custom_handler);
    wait_for_interrupt();
}
