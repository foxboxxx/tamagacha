#include "rtc.h"

#define PCF8523_ADDR 0x68
#define CONTROL1     0x00
#define CONTROL2     0x01
#define CONTROL3     0x02
#define SECONDS      0x03
#define MINUTES      0x04
#define HOURS        0x05
#define DAYS         0x06
#define WEEKDAYS     0x07
#define MONTHS       0x08
#define YEARS        0x09

// starter test function...
static void try_read(i2c_dev_t* dev, uint8_t addr, uint8_t reg, uint8_t* data, int p) {
    if (i2c_write_read_reg_n(dev, addr, reg, data, 1) > 0) {
        if (p) printk("Read register %x successfully.\n");
    } else {
        if (p) printk("Failed to read Control3\n");
    }
}

static uint8_t to_bcd(uint8_t val, uint8_t ones_bits, uint8_t tens_bits) {
    uint8_t ones_mask = (1 << ones_bits) - 1;  
    uint8_t tens_mask = (1 << tens_bits) - 1;   
    uint8_t ones = val % 10;
    uint8_t tens = val / 10;
    return ((tens & tens_mask) << ones_bits) | (ones & ones_mask);
}

static uint8_t from_bcd(uint8_t bcd, uint8_t ones_bits, uint8_t tens_bits) {
    uint8_t ones_mask = (1 << ones_bits) - 1;
    uint8_t tens_mask = (1 << tens_bits) - 1;
    uint8_t ones = bcd & ones_mask;
    uint8_t tens = (bcd >> ones_bits) & tens_mask;
    return tens * 10 + ones;
}

static void rtc_set_reg(i2c_dev_t* dev, uint8_t reg, uint8_t val) {
    uint8_t data[2] = { reg, val };
    i2c_write_n_bytes(dev, PCF8523_ADDR, data, 2);
}

static uint8_t rtc_get_reg(i2c_dev_t* dev, uint8_t reg) {
    uint8_t val;
    i2c_write_read_reg_n(dev, PCF8523_ADDR, reg, &val, 1);
    return val;
}

// new helper for time calc
uint32_t rtc_get_total_seconds(i2c_dev_t* dev) {
    uint16_t yr = rtc_get_years(dev) + 2000;
    uint8_t mo = rtc_get_months(dev);
    uint8_t da = rtc_get_days(dev);
    uint8_t ho = rtc_get_hours(dev);
    uint8_t mi = rtc_get_minutes(dev);
    uint8_t se = rtc_get_seconds(dev);
    if (mo <= 2) {
        mo += 12;
        yr--;
    }
    uint32_t days = (da - 1) + (153 * (mo - 3) + 2) / 5 + (365 * yr) + (yr / 4) - (yr / 100) + (yr / 400) - 730425; // 730425 for jan1,2000
    return (days * 86400) + (ho * 3600) + (mi * 60) + se;
}

//setter fucntions
void rtc_set_seconds(i2c_dev_t* dev, uint8_t sec) { rtc_set_reg(dev, SECONDS, to_bcd(sec, 4, 3) & 0x7F); }
void rtc_set_minutes(i2c_dev_t* dev, uint8_t min) { rtc_set_reg(dev, MINUTES, to_bcd(min, 4, 3) & 0x7F); }
void rtc_set_hours(i2c_dev_t* dev, uint8_t hour) { rtc_set_reg(dev, HOURS, to_bcd(hour, 4, 2)); } // 24hr mode, bits 5:0 
void rtc_set_days(i2c_dev_t* dev, uint8_t day) { rtc_set_reg(dev, DAYS, to_bcd(day, 4, 2)); }
void rtc_set_weekdays(i2c_dev_t* dev, uint8_t wd) { rtc_set_reg(dev, WEEKDAYS, wd & 0x07); } // 0=sunday, doesnt use bcd format
void rtc_set_months(i2c_dev_t* dev, uint8_t month) { rtc_set_reg(dev, MONTHS, to_bcd(month, 4, 1)); }
void rtc_set_years(i2c_dev_t* dev, uint8_t year) { rtc_set_reg(dev, YEARS, to_bcd(year, 4, 4)); } // year is offset from 2000

// getter functions
uint8_t rtc_get_seconds(i2c_dev_t* dev) { return from_bcd(rtc_get_reg(dev, SECONDS), 4, 3); }
uint8_t rtc_get_minutes(i2c_dev_t* dev) { return from_bcd(rtc_get_reg(dev, MINUTES), 4, 3); }
uint8_t rtc_get_hours(i2c_dev_t* dev) { return from_bcd(rtc_get_reg(dev, HOURS), 4, 2); }
uint8_t rtc_get_days(i2c_dev_t* dev) { return from_bcd(rtc_get_reg(dev, DAYS), 4, 2); }
uint8_t rtc_get_weekdays(i2c_dev_t* dev) { return rtc_get_reg(dev, WEEKDAYS) & 0x07; }
uint8_t rtc_get_months(i2c_dev_t* dev) { return from_bcd(rtc_get_reg(dev, MONTHS), 4, 1); }
uint8_t rtc_get_years(i2c_dev_t* dev) { return from_bcd(rtc_get_reg(dev, YEARS), 4, 4); }

void rtc_init(i2c_dev_t* dev) {
    uint8_t data[2] = { CONTROL3, 0x00 };
    i2c_write_n_bytes(dev, PCF8523_ADDR, data, 2); // turn on battery switch over!!!
}

void xnotmain(void) {
    i2c_dev_t* dev = i2c_init(SDA_DEFAULT, SCL_DEFAULT, CLK_100KHZ);
    uint8_t control1, control2, control3;

    i2c_scan(dev); // search for device address -> found addr at 0x68

    printk("control3 should be 0b11100000 or 0xe0 if battery switch off, and 0 if on!\n");
    if (i2c_write_read_reg_n(dev, PCF8523_ADDR, CONTROL3, &control3, 1) > 0) {
        printk("Control3 register print out is %x\n", control3);
    } else {
        printk("Failed to read Control3\n");
        return;
    }

    rtc_init(dev);
    // uint8_t data[2] = { CONTROL3, 0x00 };
    // i2c_write_n_bytes(dev, PCF8523_ADDR, data, 2); // turn on battery switch over!!!

    // infinte loop test to make sure reading everything correctly
    // uint8_t seconds, minutes, hours, days, weekdays, months, years;
    // while(1) {
    //     try_read(dev, PCF8523_ADDR, SECONDS, &seconds, 1);
    //     printk("Seconds: %b\n", seconds);
    //     try_read(dev, PCF8523_ADDR, MINUTES, &minutes, 1);
    //     printk("Minutes: %b\n", minutes);
    //     try_read(dev, PCF8523_ADDR, HOURS, &hours, 1);
    //     printk("Hours", hours);
    //     try_read(dev, PCF8523_ADDR, DAYS, &days, 1);
    //     printk("Days: %b\n", days);
    //     try_read(dev, PCF8523_ADDR, WEEKDAYS, &weekdays, 1);
    //     printk("Weekdays: %b\n", weekdays);
    //     try_read(dev, PCF8523_ADDR, MONTHS, &months, 1);
    //     printk("Months: %b\n", months);
    //     try_read(dev, PCF8523_ADDR, YEARS, &years, 1);
    //     printk("Years: %b\n", years);
    //     delay_ms(1000);
    // }

    // simple set / get test on seconds!
    // void rtc_set_seconds(i2c_dev_t* dev, uint8_t sec);
    // uint8_t rtc_get_seconds(i2c_dev_t* dev);
    // rtc_set_seconds(dev, 21);
    // while(1) {
    //     printk("curr seconds: %d\n", rtc_get_seconds(dev));
    //     delay_ms(1000);
    // }

    // set to current time
    // rtc_set_seconds(dev, 45);
    // rtc_set_minutes(dev, 58);
    // rtc_set_hours(dev, 13);
    // rtc_set_days(dev, 19);
    // rtc_set_weekdays(dev, 4);
    // rtc_set_months(dev, 3);
    // rtc_set_years(dev, 26);

    // read 5 seconds...
    for (int i = 0; i < 5; i++) {
        printk("Second: %d\n", rtc_get_seconds(dev));
        printk("Minute: %d\n", rtc_get_minutes(dev));
        printk("Hour: %d\n", rtc_get_hours(dev));
        printk("Day: %d\n", rtc_get_days(dev));
        printk("Weekday: %d\n", rtc_get_weekdays(dev));
        printk("Month: %d\n", rtc_get_months(dev));
        printk("Year: %d\n", rtc_get_years(dev));
        delay_ms(1000);
    }

    return;
}