#include "rpi.h"

void notmain() {
    printk("hello world\n");
    delay_cycles(1500000 * 12 * 5);
    printk("hello world\n");
}
