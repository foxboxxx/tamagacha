#pragma once

#include "rpi.h"

#define SDA_DEFAULT  2
#define SCL_DEFAULT  3

#define CLK_100KHZ   5
#define CLK_400KHZ   2

#define TIMEOUT      100

typedef enum {
    C_100KHZ = CLK_100KHZ,
    C_400KHZ = CLK_400KHZ
} i2c_clk_t;

typedef struct {
    i2c_clk_t clk;
    uint32_t sda;
    uint32_t scl;
} i2c_dev_t;

// pass in CLK_100KHZ or CLK_400KHZ for clockspeed pls
i2c_dev_t* i2c_init(uint32_t sda, uint32_t scl, i2c_clk_t clk);

void i2c_start(i2c_dev_t* dev);

void i2c_stop(i2c_dev_t* dev);

int i2c_write_byte(i2c_dev_t* dev, uint8_t byte);

uint8_t i2c_read_byte(i2c_dev_t* dev, int send_ack);

int i2c_write_n_bytes(i2c_dev_t* dev, uint8_t addr, uint8_t* data, int length);

int i2c_read_n_bytes(i2c_dev_t* dev, uint8_t addr, uint8_t* data, int length);

int i2c_write_read_reg_n(i2c_dev_t* dev, uint8_t addr, uint8_t reg, uint8_t* data, int length);

void i2c_scan(i2c_dev_t* dev);