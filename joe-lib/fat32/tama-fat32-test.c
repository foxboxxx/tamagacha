#include "rpi.h"
#include "tama-fat32.h"

void notmain() {
    // init_tama_info();
    printk("%s", get_start_readable());
    printk("%s", get_peak_readable());
    // printk("%s", get_win_streak_readable());
    // printk("in notmain\n");
    // delay_cycles(15000000 * 4);
    // printk("sending balance request\n");
    // printk("%s", get_balance_readable());
}
