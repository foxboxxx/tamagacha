#include "pi-files.h"
#include "rpi.h"
#include "fat32.h"
#include "fat32-helpers.h"
#include "pi-sd.h"
#include <string.h>
#include "../i2c-sw-driver/i2c-sw.h"
#include "../kalshi-driver/kalshi-interface.h"
#include "../rtc-driver/rtc.h"


static fat32_fs_t static_fs;
static unsigned is_setup = 0;
static unsigned is_created = 0;

enum {SEC_PER_DAY = 86400, SEC_PER_MIN = 60};

// 820540800  is number of seconds from Jan 1 2000 (clock start) to Jan 1 2026
char *fields[10] = {"birth", "start", "peak", "num_trades", "last_trade", "loss_streak", "win_streak", "state", "new_game", "level"};
char *defaults[10] = {"820540800", "10", "20", "0", "820540800", "0", "0", "healthy", "0", "1"};
unsigned num_fields = 10;
char *name = "INFO.TXT";

// basic helper to convert strings to ints for easy conversions!!!
int simple_atoi(const char *s) {
    int neg = 0, val = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return neg ? -val : val;
}

// only works for 1/2 digit numbers
char *simple_itoa(unsigned val) {
    // char *str = kmalloc(3);
    // if (v > 9) {
    //     str[0] = (v / 10) + '0';
    //     str[1] = (v % 10) + '0';
    //     str[2] = '\0';
    // } else {
    //     str[0] = v + '0';
    //     str[1] = '\0';
    // }
    // return str;
    char tmp[33];
    unsigned len = 0;

    if (val == 0) {
        tmp[len++] = '0';
    } else {
        while (val > 0) {
            tmp[len++] = '0' + (char)(val % 10);
            val /= 10;
        }
    }

    char *ret = kmalloc(len + 1);

    // reverse into return value
    for (int i = 0; i < len; i++) {
        ret[i] = tmp[len - 1 - i];
    }
    ret[len] = '\0';
    return ret;
}

// set up routine to interact with the filesystem
void setup_fs() {
    // kmalloc_init_mb(FAT32_HEAP_MB);
    // kmalloc_init_set_start((void*)0x800000); 
    // kmalloc_init_mb(FAT32_HEAP_MB);
    kmalloc_init_set_start((void*)0x800000, FAT32_HEAP_MB * 1024 * 1024);
    pi_sd_init();

    mbr_t *mbr = mbr_read();

    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));

    static_fs = fat32_mk(&partition);
    is_setup = 1;
}

fat32_fs_t *get_fs(){
    if (!is_setup) {
        setup_fs();     
    }
    return &static_fs;
}

// crates .txt file tracking tama info
void init_tama_info() {
    fat32_fs_t *fs = get_fs();
    pi_dirent_t root = fat32_get_root(fs);
    // fat32_delete(fs, &root, name);
    if (fat32_stat(fs, &root, name) != NULL) {
        printk("%s already exists!\n", name);
        return;
    }
    assert(fat32_create(fs, &root, name, 0));
    unsigned buf_size = 100 * num_fields;
    char data[buf_size];
    char *ptr = data;
    // write empty cols
    for (int i = 0; i < num_fields; i++) {
        // length error checking, should be safe though
        unsigned entry_len = strlen(fields[i]) + strlen(defaults[i]) + 4;
        if ((ptr - data) + entry_len >= buf_size) {
            panic("INFO.TXT buffer overflow! Increase buf_size.\n");
        }
        strcpy(ptr, fields[i]);
        ptr += strlen(fields[i]);
        strcpy(ptr, "=\"");
        ptr += 2;
        strcpy(ptr, defaults[i]);
        ptr += strlen(defaults[i]);
        strcpy(ptr, "\"\n");
        ptr += 2;
    }
    pi_file_t info_file = (pi_file_t) {
        .data = data,
        .n_data = ptr - data,
        .n_alloc = buf_size,
    };
    assert(fat32_write(fs, &root, name, &info_file));
}

char *get_field_data(char *field_name) {
    fat32_fs_t *fs = get_fs();
    pi_dirent_t root = fat32_get_root(fs);
    pi_file_t *file = fat32_read(fs, &root, name);
    if (!file) {
        panic("Couldn't read %s\n", name);
    }

    unsigned n = file->n_data;
    unsigned field_len = strlen(field_name);

    for (unsigned i = 0; i < n; ) {
        if (i + field_len + 2 < n && 
            strncmp(&file->data[i], field_name, field_len) == 0 &&
            file->data[i + field_len] == '=' &&
            file->data[i + field_len + 1] == '"') {

            unsigned start = i + field_len + 2;
            unsigned end = start;

            // Find the closing quote
            while (end < n && file->data[end] != '"') {
                end++;
            }

            unsigned len = end - start;
            // Allocate memory for the string + null terminator
            char *val = kmalloc(len + 1); 
            
            // Copy the data and null-terminate
            memcpy(val, &file->data[start], len);
            val[len] = '\0';
            return val;
        }

        // Move to the beginning of the next line
        while (i < n && file->data[i] != '\n') i++;
        i++;    
    }
    panic("field %s not found", field_name);
}

// begin getter functions for each field
// returns age in total seconds
unsigned get_age(i2c_dev_t* rtc) {
    unsigned birth = simple_atoi(get_field_data("birth"));
    return rtc_get_total_seconds(rtc) - birth;
}

char *get_start() {
    return get_field_data("start");
}

char *get_peak() {
    return get_field_data("peak");
}

char *get_num_trades() {
    return get_field_data("num_trades");
}

char *get_level() {
    return get_field_data("level");
}

unsigned get_hearts() {
    unsigned loss_streak = simple_atoi(get_field_data("loss_streak"));
    unsigned win_streak = simple_atoi(get_field_data("win_streak"));
    if (loss_streak != 0 && win_streak != 0) {
        panic("both streaks nonzero! loss streak = %d, win streak = %d", loss_streak, win_streak);
    }
    if (!loss_streak) {
        return 3;
    } else if (loss_streak == 1) {
        return 2;
    } else if (loss_streak == 2) {
        return 1;
    } else {
        return 0;
    }
}

uint32_t get_last_trade_time() {
    char *val = get_field_data("last_trade");
    // val is "YYYYMMDDHHMMSS" format
    uint32_t yr = (val[0]-'0')*1000 + (val[1]-'0')*100 + (val[2]-'0')*10 + (val[3]-'0');
    uint32_t mo = (val[4]-'0')*10 + (val[5]-'0');
    uint32_t da = (val[6]-'0')*10 + (val[7]-'0');
    uint32_t ho = (val[8]-'0')*10 + (val[9]-'0');
    uint32_t mi = (val[10]-'0')*10 + (val[11]-'0');
    uint32_t se = (val[12]-'0')*10 + (val[13]-'0');
    // same formula as rtc_get_total_seconds
    if (mo <= 2) { mo += 12; yr--; }
    uint32_t days = (da - 1) + (153 * (mo - 3) + 2) / 5 + 365 * yr + yr / 4 - yr / 100 + yr / 400  - 730425;
    return days * 86400 + ho * 3600 + mi * 60 + se;
    // return simple_atoi(val);
}

// concatenates a given prefix and suffix to create readable output
// does not do error checking
char *readable_concat(char *prefix, char *suffix) {
    char *ret = kmalloc(16);
    unsigned prefix_len = strlen(prefix);
    memcpy(ret, prefix, prefix_len);
    memcpy(ret + prefix_len, suffix, strlen(suffix));
    ret[strlen(suffix) + prefix_len] = '\0';
    return ret;
}

// get a printable version of each value for stats screen
char *get_hearts_readable() {
    unsigned hearts = get_hearts();
    char *ret = kmalloc(9);
    memcpy(ret, "HEARTS=", 7);
    ret[7] = hearts + '0';
    ret[8] = '\0';
    return ret;
}

// for demo purposes gets age in hours old
// note for joe: since no RTC I can't test this, but should be right
// returns age in days
char *get_age_readable(i2c_dev_t* rtc) {
    unsigned age_sec = get_age(rtc);
    char *age_str = simple_itoa(age_sec / SEC_PER_DAY);
    return readable_concat("AGE=", age_str);
}

// calculates how much of the 24 hours you have left since your last trade
// (value returned in minutes)
// replaces get_last_trade_readable
char *get_time_rem_readable(i2c_dev_t* rtc) {
    unsigned last_trade = get_last_trade_time(rtc);
    unsigned cur = rtc_get_total_seconds(rtc);
    char *rem_str = simple_itoa((cur - last_trade) / SEC_PER_MIN);
    return readable_concat("TIME_REM=", rem_str);
}

char *get_start_readable() {
    return readable_concat("START=", get_field_data("start"));
}

char *get_peak_readable() {
    return readable_concat("PEAK=", get_field_data("peak"));
}

char *get_level_readable() {
    char *level = get_field_data("level");
    return readable_concat("LEVEL=", level);
}

char *get_state_readable() {
    char *state = get_field_data("state");
    return readable_concat("STATE=", state);
}

char *get_num_trades_readable() {
   return readable_concat("TRADES=", get_num_trades());
}

char *get_win_streak_readable() {
   return readable_concat("WIN_STREAK=", get_field_data("win_streak"));
}

char *get_loss_streak_readable() {
   return readable_concat("LOSS STREAK=", get_field_data("loss_streak"));
}

// note: not very much bounds checking being done here, but its chill
char *get_balance_readable() {
    unsigned balance = kalshi_get_balance();
    char *balance_val = simple_itoa(balance);
    unsigned balance_len = strlen(balance_val);
    char *prefix = "BAL=$";
    char *ret = kmalloc(16);
    memcpy(ret, prefix, 5);
    memcpy(ret + 5, balance_val, balance_len - 2);
    ret[5 + balance_len - 2] = '.';
    ret[5 + balance_len - 1] = balance_val[balance_len-2];
    ret[5 + balance_len] = balance_val[balance_len-1];
    ret[5 + balance_len + 1] = '\0';
    return ret;
}

// can add arbitrary getter functions as necessary, ust call get_field_data with desired field name


void write_field_data(char *field_name, char *new_val) {
    fat32_fs_t *fs = get_fs();
    pi_dirent_t root = fat32_get_root(fs);
    pi_file_t *file = fat32_read(fs, &root, name);
    if (!file) {
        panic("Couldn't read %s\n", name);
    }

    char new_data[file->n_alloc];
    char *ptr = new_data;

    unsigned n = file->n_data;
    unsigned field_len = strlen(field_name);
    unsigned found = 0;

    for (unsigned i = 0; i < n; ) {
        // Match the field: key="
        if (!found && i + field_len + 2 < n && 
            strncmp(&file->data[i], field_name, field_len) == 0 &&
            file->data[i + field_len] == '=' &&
            file->data[i + field_len + 1] == '"') {

            // 1. Copy the key and the =" prefix
            memcpy(ptr, field_name, field_len);
            ptr += field_len;
            *ptr++ = '=';
            *ptr++ = '"';

            // 2. Insert the NEW value
            unsigned val_len = strlen(new_val);
            memcpy(ptr, new_val, val_len);
            ptr += val_len;

            // 3. Close with " and newline
            *ptr++ = '"';
            *ptr++ = '\n';

            // 4. Skip the OLD value in the source file
            unsigned cursor = i + field_len + 2;
            while (cursor < n && file->data[cursor] != '"') cursor++;
            cursor++; // skip the closing quote
            while (cursor < n && file->data[cursor] != '\n') cursor++;
            i = cursor + 1; // move i to the start of the next line
            found = 1;
        } else {
            // Not the target field: copy the original line character by character
            while (i < n) {
                *ptr++ = file->data[i];
                if (file->data[i] == '\n') {
                    i++;
                    break;
                }
                i++;
            }
        }
    }

    if (!found) panic("field %s not found", field_name);

    // Prepare the updated file structure
    pi_file_t updated_file = (pi_file_t) {
        .data = new_data,
        .n_data = ptr - new_data,
        .n_alloc = file->n_alloc,
    };

    // Write the modified buffer back to the SD card
    assert(fat32_write(fs, &root, name, &updated_file));
}

// same long shit as the other rtc BS fuck this sh*T
void write_last_trade(i2c_dev_t *rtc) {
    char str[15]; // fotmatted "YYYYMMDDHHMMSS\0"
    uint16_t yr = rtc_get_years(rtc) + 2000;
    uint8_t mo = rtc_get_months(rtc);
    uint8_t da = rtc_get_days(rtc);
    uint8_t ho = rtc_get_hours(rtc);
    uint8_t mi = rtc_get_minutes(rtc);
    uint8_t se = rtc_get_seconds(rtc);
    str[0]  = '0' + (yr / 1000);
    str[1]  = '0' + (yr / 100 % 10);
    str[2]  = '0' + (yr / 10 % 10);
    str[3]  = '0' + (yr % 10);
    str[4]  = '0' + (mo / 10);
    str[5]  = '0' + (mo % 10);
    str[6]  = '0' + (da / 10);
    str[7]  = '0' + (da % 10);
    str[8]  = '0' + (ho / 10);
    str[9]  = '0' + (ho % 10);
    str[10] = '0' + (mi / 10);
    str[11] = '0' + (mi % 10);
    str[12] = '0' + (se / 10);
    str[13] = '0' + (se % 10);
    str[14] = '\0';
    write_field_data("last_trade", str);
}


// begin setter functions for each field
// use an example of how to handle numerical input, for fields
// with string input just call helper
void write_level(unsigned lvl) {
    write_field_data("level", simple_itoa(lvl));
}

void write_win_streak(unsigned streak) {
    write_field_data("win_streak", simple_itoa(streak));
}

void write_loss_streak(unsigned streak) {
    write_field_data("loss_streak", simple_itoa(streak));
}
