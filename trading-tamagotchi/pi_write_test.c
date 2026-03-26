#include "rpi.h"


void notmain(void){
    unsigned i = 0;
    int out;
    while (1) {
        printk("%d: hello world!\n", i);
        delay_cycles(10000000);
        i++;
        // check if we recieved anything in RX, if so, print it
        // if ((out = uart_get8_sync()) != -1) {
        //     printk("on RX: %c\n", out);
        // }
        // if (uart_has_data()) delay_cycles(100000000);
        while (uart_has_data()) {
            printk("%c", uart_get8());
        }
    }
}
