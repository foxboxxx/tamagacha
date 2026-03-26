#ifndef __KEYPAD_H__
#define __KEYPAD_H__

// TODO: populate this
void set_buttons(unsigned *gpio_buttons, unsigned num_buttons);
void keypad_init(unsigned *in_buttons, unsigned in_num, int (*handler)(uint32_t pc));

#endif
