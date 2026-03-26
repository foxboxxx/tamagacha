#pragma once

#include "rpi.h"
#include "../i2c-sw-driver/i2c-sw.h"

// helper
uint32_t rtc_get_total_seconds(i2c_dev_t* dev);

// SETTERS
void rtc_set_seconds(i2c_dev_t* dev, uint8_t sec);
void rtc_set_minutes(i2c_dev_t* dev, uint8_t min);
void rtc_set_hours(i2c_dev_t* dev, uint8_t hour);
void rtc_set_days(i2c_dev_t* dev, uint8_t day);
void rtc_set_weekdays(i2c_dev_t* dev, uint8_t wd);
void rtc_set_months(i2c_dev_t* dev, uint8_t month);
void rtc_set_years(i2c_dev_t* dev, uint8_t year);

// GETTERS
uint8_t rtc_get_seconds(i2c_dev_t* dev);
uint8_t rtc_get_minutes(i2c_dev_t* dev);
uint8_t rtc_get_hours(i2c_dev_t* dev);
uint8_t rtc_get_days(i2c_dev_t* dev);
uint8_t rtc_get_weekdays(i2c_dev_t* dev);
uint8_t rtc_get_months(i2c_dev_t* dev);
uint8_t rtc_get_years(i2c_dev_t* dev);

// stupid init i need to set the control register every time 
void rtc_init(i2c_dev_t* dev);