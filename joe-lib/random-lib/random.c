#include "random.h"
#include "rpi.h"
#include "../i2c-sw-driver/i2c-sw.h"
#include "../rtc-driver/rtc.h"

static uint32_t seed;

void random_init(i2c_dev_t* dev) {
    uint32_t sec = rtc_get_seconds(dev);
    uint32_t min = rtc_get_minutes(dev);
    uint32_t hr  = rtc_get_hours(dev);
    uint32_t day = rtc_get_days(dev);
    uint32_t mon = rtc_get_months(dev);
    uint32_t yr  = rtc_get_years(dev);
    seed = sec | (min << 6) | (hr << 12) | (day << 17) | (mon << 22) | (yr << 26);
    if (seed == 0) seed = 0xdeadbeef;
}

uint32_t random_next(void) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

// Generates a random number in range from min to max inclusiv
uint32_t random_gen(uint32_t min, uint32_t max) {
    return min + (random_next() % (max - min + 1));
}