#pragma once

#include "rpi.h"
#include "spi.h"

// gpio pins for data/command line and reset line
#define ST7735_DC  24 
#define ST7735_RST 26

// quick define commands (starting @pg 79)
#define ST7735_SWRESET 0x1
#define ST7735_SLPOUT  0x11
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36

// dimensions
#define WIDTH 128
#define HEIGHT 128

// offsets
#define X_OFFSET 2
#define Y_OFFSET 3

// rgb656 red (5 red bits, 6 green bits, 5 blue bits)
#define COLOR_RED   0b1111100000000000
// others...
#define COLOR_GREEN 0b0000011111100000
#define COLOR_BLUE  0b0000000000011111
#define COLOR_BG    0x0DCE

// send command (dc pin low)
void st7735_send_cmd(uint8_t cmd);

// send one byte data (dc pin high)
void st7735_send_data(uint8_t data);

// send one pixel data (dc pin high)
void st7735_send_data16(uint16_t data);

// initializes spi_t device
void st7735_init(void);

// sets the window for manual transfer... (just use fill window)
void st7735_set_window(uint8_t x_start, uint8_t x_end, uint8_t y_start, uint8_t y_end);

// fills specified window with the input buffer
void st7735_fill_window(void* buf, uint8_t x_start, uint8_t x_end, uint8_t y_start, uint8_t y_end);

// test function, dont use since it sends width * height spi transfers bc one pixel at a time
void st7735_fill_screen(uint16_t color);

// single pixel draw
void draw_pixel(uint8_t x, uint8_t y, uint16_t color);