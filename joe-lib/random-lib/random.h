#pragma once

#include "rpi.h"
#include "../rtc-driver/rtc.h"

void random_init(i2c_dev_t* dev);

uint32_t random_next(void);

uint32_t random_gen(uint32_t min, uint32_t max);