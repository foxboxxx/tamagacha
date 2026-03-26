#pragma once

#include "rpi.h"
#include "../display-driver/st7735r-lcd.h"

void draw_char(char c, uint8_t x, uint8_t y, uint16_t text_color, uint16_t bg_color);
void draw_string(const char* str, uint8_t x, uint8_t y, uint16_t text_color, uint16_t bg_color);

extern unsigned char console_font_8x8[];