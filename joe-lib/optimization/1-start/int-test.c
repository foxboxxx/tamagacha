#include "rpi.h"
#include "timer-interrupt.h"
#include "cycle-count.h"
#include "vector-base.h"

enum { in_pin = 26, out_pin = 27 };

static volatile unsigned success = 0;
// global state that real interrupt handler must update
static volatile unsigned last_press_time = 0;
static volatile unsigned press = 0;
static volatile unsigned tbp = 0;
static volatile unsigned refresh = 0;

#define DEBOUNCE_US 200000 // 200 ms debounce so no repeatd button presses

uint32_t random_four(void) {
    unsigned seed = 0xdeadbeef;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed % 4;
}

// handler that receives and interrupt, and buys something on Kalshi
// this is the routine we care most about optimizing
void custom_handler(uint32_t pc) {
    dev_barrier();
    unsigned curr_press_time = timer_get_usec();
    int button = 0;
    // randomly gnerate an offset to simulate iterating though the for loop
    // in main handler (which you must do since we don't know which button
    // was pressed)
    int offset = random_four();
    for (int i = 0; i < 4; i++) {
        unsigned cur_pin = in_pin + (i + offset) % 4;
        if (gpio_event_detected(cur_pin)) {
            if ((curr_press_time - last_press_time) >= DEBOUNCE_US && !tbp) {
                last_press_time = curr_press_time;
                press = cur_pin;
                tbp = 1;
                refresh = 1;

                // optionally test buy time here, skip to test interrupt time
            }
            gpio_event_clear(cur_pin);
            success = 1;
            dev_barrier();
            return;
        }
    }
    success = 0;
    dev_barrier();
    return;
}

void test_cost(unsigned pin) { 
    // initial state.
    assert(gpio_read(in_pin) == 0);

    unsigned sum = 0;
    uint32_t c,e;
    for(int i = 0; i < 10; i++) {
        // measure the cost of a rising edge interrupt.
        // by reading cycle counter and spinning until the
        // rising edge count increases (i.e., an interrupt
        // occured).
        success = 0;
        c = cycle_cnt_read();

        gpio_set_on(pin);
        while (success == 0);
        e = cycle_cnt_read();
        output("%d: rising\t= %d cycles\n", i*2, e-c);
        sum += e-c;

        // measure the cost of a falling edge interrupt.
        // by reading cycle counter and spinning until the
        // falling edge count increases (i.e., an interrupt
        // occured).
        success = 0;
        c = cycle_cnt_read();
        gpio_set_off(pin);
        while (success == 0);
        e = cycle_cnt_read();
        output("%d: falling\t= %d cycles\n", i*2+1, e-c);
        sum += e-c;
    }
    output("ave cost = %d\n", sum / 20);
}

void notmain() {
    cycle_cnt_init();
    // timing management derived from 340lx lab 1 (also where I will be getting
    // most optimizations from)
    gpio_set_output(out_pin);
    gpio_set_input(in_pin);

    // setup interrupts
    // 2. setup interrupts in our standard way.
    extern uint32_t interrupt_vec[];

    // setup interrupts.  you've seen this code
    // before.  (we're assuming ints are off.)
    dev_barrier();
    PUT32(IRQ_Disable_1, 0xffffffff);
    PUT32(IRQ_Disable_2, 0xffffffff);
    dev_barrier();
    vector_base_set(interrupt_vec);

    gpio_int_falling_edge(in_pin);
    gpio_int_rising_edge(in_pin);

    // the above sample twice triggering (for a stable
    // signal) --- in theory these should be faster.
    // gpio_int_async_rising_edge(in_pin);
    // gpio_int_async_falling_edge(in_pin);

    // clear any existent GPIO event so that we don't 
    // get a delayed interrupt.
    gpio_event_clear(in_pin);

    // now we are live!
    enable_interrupts();

    // 3. call test routine
    test_cost(out_pin);
    printk("Test complete!\n");
}
