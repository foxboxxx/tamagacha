#include "st7735r-lcd.h"
// #include "../animation-maker/test_animation.h"
// #include "../animations/cat.h"
// #include "../animations/happy.h"
// #include "../animations/idle.h"
// #include "../animations/menu.h"
// #include "../animations/wave.h"

// datasheet @ https://cdn-shop.adafruit.com/datasheets/ST7735R_V0.2.pdf
// pinout @ https://static.us.edusercontent.com/files/MeOsfGtjs3cl3GPvoI8xKe6d

static spi_t st7735;

// send command byte (D/C pin low)
void st7735_send_cmd(uint8_t cmd) {
    gpio_set_off(ST7735_DC);
    uint8_t rx[1];
    spi_n_transfer(st7735, rx, &cmd, 1);
}

// send data byte (D/C pin high)
void st7735_send_data(uint8_t data) {
    gpio_set_on(ST7735_DC);
    uint8_t rx[1];
    spi_n_transfer(st7735, rx, &data, 1);
}

// send 16bit data (one pixel)
void st7735_send_data16(uint16_t data) {
    gpio_set_on(ST7735_DC);
    // you need tx and rx bc spi is bidirectional, so even if rx isn't being filled it needs to be passed
    uint8_t tx[2] = { (data >> 8) & 0xff, data & 0xff }; 
    uint8_t rx[2];
    spi_n_transfer(st7735, rx, tx, 2);
}

// init function
void st7735_init(void) {
    // init gpio pins for d/c and reset
    gpio_set_output(ST7735_DC);
    gpio_set_output(ST7735_RST);

    // reset display on init (delays needed)
    gpio_set_off(ST7735_RST);
    delay_ms(50); 
    gpio_set_on(ST7735_RST);
    delay_ms(50);

    st7735_send_cmd(ST7735_SWRESET); // sw reset w/ delay
    delay_ms(150);

    st7735_send_cmd(ST7735_SLPOUT);  // wake up display
    delay_ms(255);

    st7735_send_cmd(ST7735_COLMOD);  // color mode cmd
    st7735_send_data(0x05);          // sets colormode to rgb 565 @ pg 42

    st7735_send_cmd(ST7735_MADCTL);
    st7735_send_data(0xC8);  // changes color mode to rgb rather than bgr

    st7735_send_cmd(ST7735_DISPON);  // Turn display on
    delay_ms(100);
}

// set drawing constraints (check pg 102)
void st7735_set_window(uint8_t x_start, uint8_t x_end, uint8_t y_start, uint8_t y_end) {
    // column window
    st7735_send_cmd(ST7735_CASET);
    st7735_send_data16(x_start + X_OFFSET); 
    st7735_send_data16(x_end + X_OFFSET); 

    // row window
    st7735_send_cmd(ST7735_RASET);
    st7735_send_data16(y_start + Y_OFFSET); 
    st7735_send_data16(y_end + Y_OFFSET); 
}

void st7735_fill_window(void* buf, uint8_t x_start, uint8_t x_end, uint8_t y_start, uint8_t y_end) {
    st7735_set_window(x_start, x_end, y_start, y_end);
    st7735_send_cmd(ST7735_RAMWR);
    gpio_set_on(ST7735_DC);
    uint32_t num_pixels = (x_end - x_start + 1) * (y_end - y_start + 1);
    uint8_t rx[num_pixels * 2];
    spi_n_transfer(st7735, rx, (uint8_t*)buf, num_pixels * 2); // num pixels times 2 bc each pixel 2 bytes
}

// test function for filling screen
void st7735_fill_screen(uint16_t color) {
    st7735_set_window(0, WIDTH - 1, 0, HEIGHT - 1);
    st7735_send_cmd(ST7735_RAMWR);
    for(int i = 0; i < WIDTH * HEIGHT; i++) { // sends data from start of the window to end of window!
        st7735_send_data16(color);
    }
}

// one pixel @ x, y
void draw_pixel(uint8_t x, uint8_t y, uint16_t color) {
    st7735_set_window(x, x, y, y);
    st7735_send_cmd(ST7735_RAMWR);
    st7735_send_data16(color);
}

// void xnotmain(void) {
//     // chip select 0 for spi0
//     st7735 = spi_n_init(0, 64);
    
//     st7735_init();

//     st7735_fill_screen(COLOR_RED);

//     // testing blue square pixel by pixel
//     draw_pixel(10, 10, COLOR_BLUE);
//     draw_pixel(10, 11, COLOR_BLUE);
//     draw_pixel(11, 10, COLOR_BLUE);
//     draw_pixel(11, 11, COLOR_BLUE);

//     delay_ms(3000);
//     uint16_t buf[WIDTH * HEIGHT];
//     uint8_t rx[WIDTH * HEIGHT * 2];

//     // cool pattern (slow...)
//     for (int x = 0; x < WIDTH; x++) {
//         for (int y = 0; y < HEIGHT; y++) {
//             draw_pixel(x, y, x + y);
//             buf[x + y * WIDTH] = (x << 5) + (y << 5);
//             buf[x + y * WIDTH] = buf[x + y * WIDTH] << 8 | buf[x + y * WIDTH] >> 8;
//         }
//     }

//     delay_ms(3000);
//     st7735_set_window(0, WIDTH - 1, 0, HEIGHT - 1);
//     st7735_send_cmd(ST7735_RAMWR);
//     gpio_set_on(ST7735_DC);
//     spi_n_transfer(st7735, rx, (uint8_t*)buf, WIDTH * HEIGHT * 2);

//     // test animation...
//     // st7735_fill_screen(0xffff);
//     // for (int i = 0; i < 5; i++) {
//     //     // st7735_fill_window((void*)&cat[0], 32, 32 + 64 - 1, 32, 32 + 64 - 1);
//     //     st7735_fill_window((void*)&cat[0], 0, WIDTH - 1, 0, HEIGHT - 1);
//     //     delay_ms(1000);
//     // }
//         // st7735_fill_window((void*)&cat[0], 32, 32 + 64 - 1, 32, 32 + 64 - 1);
//     st7735_fill_window((void*)&menu[0], 0, WIDTH - 1, 0, HEIGHT - 1);
//     delay_ms(1000);
    
//     for (int i = 0; i < 10; i++) {
//         st7735_fill_window((void*)&happy[0], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&happy[1], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);
//     }

//     delay_ms(5000);

//     for(int i = 0; i < 5; i++) {
//         st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&idle[1], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&idle[2], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);
//     }

//     for (int i = 0; i < 4; i++) {
//         st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(500);

//         st7735_fill_window((void*)&wave[0], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&wave[1], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&wave[2], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&wave[1], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&wave[2], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&wave[1], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);

//         st7735_fill_window((void*)&wave[0], 0, WIDTH - 1, 0, HEIGHT - 1);
//         delay_ms(250);
//     }

// }