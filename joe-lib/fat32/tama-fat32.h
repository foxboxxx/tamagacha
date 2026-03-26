#include "rpi.h"
#include "fat32.h"
#include "../rtc-driver/rtc.h"

int simple_atoi(const char *s);

fat32_fs_t setup_fs();

void init_tama_info();

char *get_level();
char *get_start();
char *get_peak();
char *get_num_trades();
char *get_level();
unsigned get_hearts();
unsigned get_age(i2c_dev_t* rtc);
char *get_hearts_readable();
char *get_balance_readable();
char *get_age_readable(i2c_dev_t* rtc);
char *get_level_readable();
char *get_state_readable();
char *get_num_trades_readable();
char *get_loss_streak_readable();
char *get_win_streak_readable();
char *get_start_readable();
char *get_peak_readable();
char *get_time_rem_readable(i2c_dev_t* rtc);

uint32_t get_last_trade_time(void);
void write_last_trade(i2c_dev_t *rtc);

void write_level(unsigned lvl);
void write_win_streak(unsigned lvl);
void write_loss_streak(unsigned streak);

char *get_field_data(char *field_name);
void write_field_data(char *field_name, char *new_val);