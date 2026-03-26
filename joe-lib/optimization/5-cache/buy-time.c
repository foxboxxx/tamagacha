#include "rpi.h"
#include "cycle-count.h"
#include "../../kalshi-driver/kalshi-interface.h"

// blank handler so interrupt doesn't complain
void custom_handler(){
    return;
}

void notmain() {
    printk("entered notmain, waiting for switch to python code\n");
    // stall so that human has time to kill this and run the python code
    delay_cycles(15000000 * 5);
    kalshi_load_holdings();
    printk("about to call buy!\n");
    unsigned rsp = 0;
    cycle_cnt_init();
    unsigned s = cycle_cnt_read();
    rsp = kalshi_buy(0, "no", 1);
    while (rsp == 0);
    unsigned e = cycle_cnt_read();
    printk("time to buy: %d\n", e-s);
}
