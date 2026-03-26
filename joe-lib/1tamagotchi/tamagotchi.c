#include "tamagotchi.h"


#define BTN_A 17
#define BTN_B 19
#define BTN_C 21
#define BTN_D 23

#define MAX_DISPLAYED_HOLDINGS 7
#define MAX_NAME_ROWS          4
#define MAX_NAME_LENGTH        60
#define RANDOM_MARKET_IDX      7

unsigned gpio_buttons[4] = {BTN_A, BTN_B, BTN_C, BTN_D};
unsigned num_buttons = 4;

static unsigned last_press_time = 0;
#define DEBOUNCE_US 250000 // 250 ms debounce so no repeatd button presses

static unsigned last_anim_time = 0;
#define IDLE_COOLDOWN_US 750000  // idle animation cooldown...

static unsigned press = 0;
static unsigned tbp = 0; // to_be_processed
static unsigned refresh = 1;

static unsigned ticker = 0;
static unsigned num_holdings = 0;
static unsigned yes_no = 0;
static unsigned gacha_mode = 0;
static unsigned first_name = 0;
static uint32_t last_trade_time = 0;
static unsigned hearts = 3;

static unsigned high_streak = 0;
static unsigned curr_streak = 0;
static unsigned hol_game_state = 0;

static unsigned cached_losses = 0;

typedef enum {
    CAT_IDLE,
    CAT_LOOKING,
    CAT_HAPPY,
    CAT_SITTING,
    CAT_SLEEPING
} cat_state_t;

cat_state_t cat_state = CAT_IDLE;

typedef enum {
    HOME,         
    MENU_SEL,
    MENU_BUY_SELL, 
    MARKET_SELECT, 
    BET_CHOICE,
    QTY_SELECT,
    GAME_HIGHER_LOWER,
    GACHA,
    SLEEP,
    HEALTH,
    STATS,
    DEATH
} game_state_t;

game_state_t game_state = HOME;

static unsigned menu_no = 0;
static unsigned buy_sell_no = 0;
static unsigned market_no = 0;
static unsigned qty_no = 0; // 0 = 1, 1 = 5, 2 = 10, 3 = max

int custom_handler(uint32_t pc) {
    unsigned curr_press_time = timer_get_usec();
    int button = 0;
    for (int i = 0; i < num_buttons; i++) {
        unsigned cur_pin = gpio_buttons[i];
        if (gpio_event_detected(cur_pin)) {
            if ((curr_press_time - last_press_time) >= DEBOUNCE_US && !tbp) { // ACTION
                last_press_time = curr_press_time;
                press = gpio_buttons[i];
                tbp = 1;
                refresh = 1;
            }
            gpio_event_clear(cur_pin);
            return 1;
        }
    }
    return 0;
}

// int check_death(i2c_dev_t* rtc) {
//     uint32_t current_time = rtc_get_total_seconds(rtc);
//     if (last_trade_time > 0 && (current_time - last_trade_time) >= 86400) return 1;
//     // Check 3 loss streak — get from file
//     char *streak_str = get_field_data("loss_streak");
//     int losses = simple_atoi(streak_str);
//     if (losses >= 3) return 1;
//     return 0;
// }

void notmain(void) {
// kmalloc_init_mb(FAT32_HEAP_MB);
    init_tama_info();
    printk("ltt: %d\n", get_last_trade_time());
    i2c_dev_t* rtc = i2c_init(SDA_DEFAULT, SCL_DEFAULT, CLK_100KHZ);
    rtc_init(rtc);
    random_init(rtc);
    uart_init();
    spi_t st7735 = spi_n_init(0, 64);
    st7735_init();
    keypad_init(gpio_buttons, num_buttons, custom_handler);
    //TODO: read/write to file for init seq!
    //--------------------// (THIS IS WHERE WE INIT THE FILE STUFF)
    last_trade_time = get_last_trade_time();
    if (last_trade_time == 0) last_trade_time = rtc_get_total_seconds(rtc);

    delay_ms(5000); // give me enough delay to fnagle crtl c on the terminal, and then starting the python script!

    char streak_type;
    unsigned streak_count = kalshi_get_streak(&streak_type);
    // update loss streak if most recent streak is losses
    if (streak_type == 'L') {
        cached_losses = streak_count;
        cached_losses = 1;
        // write_loss_streak(streak_count);
         write_loss_streak(1);
        write_win_streak(0);
    } 
    // wins otherwise
    else {
        cached_losses = 0;
        write_loss_streak(0);
        if (streak_type == 'W') write_win_streak(streak_count);
    }
    // cached_losses = 3; // to test the death screen if it works --> it does!
    // if you have 3 or more loss streak, you are cooked
    if (cached_losses >= 3) { 
        // game_state = DEATH; 
        refresh = 1; 
    }
    uint32_t curr_time = rtc_get_total_seconds(rtc); 
    printk("curr time: %d\n", curr_time);
    // if theres a last trade time,and its over the deadline, YOU ARE DEAD~!!!
    if (last_trade_time > 0 && (curr_time - last_trade_time) >= 86400) { 
        // game_state = DEATH; 
        refresh = 1; 
    }
    //--------------------//

    // game loop
    while (1) {
        if (game_state != DEATH) {
            // check if we're dead every loop :D
            uint32_t curr_time = rtc_get_total_seconds(rtc);
            if ((last_trade_time > 0 && (curr_time - last_trade_time) >= 86400) || cached_losses >= 3) {
                game_state = DEATH;
                refresh = 1;
                continue;
            }
        }

        if (game_state == HOME) {
            if (refresh) {
                st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                refresh = 0;
                gacha_mode = 0;
            }
            if (tbp) {
                tbp = 0;
                refresh = 1;
                if (press == BTN_A) {
                    game_state = MENU_SEL;
                    continue;
                }
                if (press == BTN_D) {
                    game_state = GACHA;
                    continue;
                }

            }
            else { // IDLE ANIMATIONS!!
                unsigned now = timer_get_usec();
                if ((now - last_anim_time) < IDLE_COOLDOWN_US) continue;
                last_anim_time = now;
                // uint32_t rand = random_gen(1, 1000);
                // if (rand == 1) {
                uint32_t rand = random_gen(1, 4);
                if (rand != 1) continue;
                else rand = random_gen(1, 10);
                if (rand < 4) {
                    st7735_fill_window((void*)&idle[1], 0, WIDTH - 1, 0, HEIGHT - 1);
                    delay_ms(125);

                    st7735_fill_window((void*)&idle[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                    delay_ms(125);
                    refresh = 1;
                } 
                else if (rand < 5) {
                    for(int i = 0; i < random_gen(1, 5); i++) {
                        st7735_fill_window((void*)&happy[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                        delay_ms(125);

                        st7735_fill_window((void*)&happy[1], 0, WIDTH - 1, 0, HEIGHT - 1);
                        delay_ms(125);
                    }
                    refresh = 1;
                }
                else if (rand < 8) {
                    st7735_fill_window((void*)&wave[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                    delay_ms(125);
                    for (int i = 0; i < random_gen(2, 4); i++) {
                        st7735_fill_window((void*)&wave[1], 0, WIDTH - 1, 0, HEIGHT - 1);
                        delay_ms(125);

                        st7735_fill_window((void*)&wave[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                        delay_ms(125);
                    }
                    st7735_fill_window((void*)&wave[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                    delay_ms(125);
                    refresh = 1;
                }
                else if (rand < 10) {
                    for (int i = 0; i < random_gen(1, 2); i++) {
                        st7735_fill_window((void*)&blink[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                        delay_ms(40);

                        st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                        delay_ms(70);
                    }
                    refresh = 1;
                }
                // }

            }
        }
        else if (game_state == MENU_SEL) {
            if (refresh) {
                st7735_fill_window((void*)&sel_menu[menu_no], 0, WIDTH - 1, 0, HEIGHT - 1);
                refresh = 0;
            }
            if (tbp) {
                tbp = 0; 
                refresh = 1;  
                if (press == BTN_A) {
                    menu_no = menu_no < 7 ? menu_no + 1 : 0;
                    refresh = 1;
                    continue;
                }
                if (press == BTN_B) { //TODO:
                    refresh = 1;
                    if (menu_no == 0) { // feeding mode
                        game_state = MENU_BUY_SELL;
                        buy_sell_no = 0;
                        menu_no = 0;
                        continue;
                    }
                    if (menu_no == 1) { // game mode
                        game_state = GAME_HIGHER_LOWER;
                        hol_game_state = 0;
                        menu_no = 0;
                        continue;
                    }
                    if (menu_no == 2) { // shop mode
                        menu_no = 0;
                        continue;
                    }
                    if (menu_no == 3) { // health/timer mode
                        game_state = HEALTH;
                        menu_no = 0;
                        continue;
                    }
                    if (menu_no == 4) { // beer? mode
                        menu_no = 0;
                        continue;
                    }
                    if (menu_no == 5) { // stats mode
                        game_state = STATS;
                        menu_no = 0;
                        continue;
                    }
                    if (menu_no == 6) { // GACHA!!!
                        game_state = GACHA;
                        menu_no = 0;
                        continue;
                    }
                    if (menu_no == 7) { // sleep mode
                        game_state = SLEEP;
                        menu_no = 0;
                        continue;
                    }
                }
                if (press == BTN_C) {
                    game_state = HOME;
                    menu_no = 0;
                    refresh = 1;
                    continue;
                }
                if (press == BTN_D) {
                    game_state = GACHA;
                    menu_no = 0;
                    refresh = 1;
                    continue;
                }
            }
        }
        else if (game_state == MENU_BUY_SELL) {
            if (refresh) {
                st7735_fill_window((void*)&feed[buy_sell_no], 0, WIDTH - 1, 0, HEIGHT - 1);
                refresh = 0;
            }
            if (tbp) {
                tbp = 0;
                refresh = 1;
                if (press == BTN_A) {
                    buy_sell_no = !buy_sell_no;
                    continue;
                }
                if (press == BTN_B) {
                    game_state = MARKET_SELECT;
                    market_no = 0;
                    st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                    continue;
                }   
                if (press == BTN_C) {
                    game_state = HOME;
                    continue;
                }
            }

        }
        else if (game_state == MARKET_SELECT) {
            if (refresh) {
                refresh = 0;
                // st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                int balance = kalshi_get_balance();
                printk("Bal: %d\n", balance);
                num_holdings = kalshi_load_holdings();
                printk("# Holdings: %d\n", num_holdings);
                for (int idx = 0; idx < num_holdings && idx < MAX_DISPLAYED_HOLDINGS; idx++) {
                    char buf[19];
                    buf[0] = '0' + idx + 1;
                    buf[1] = '.';
                    buf[2] = ' ';
                    kalshi_get_ticker(idx, 1, &buf[3]);
                    kalshi_get_ticker(idx, 2, &buf[9]);
                    buf[14] = '\0';
                    if (idx == ticker) draw_string((const char*)buf, 3, 23 + (10 * (idx)), 0xff, 0x0);
                    else draw_string((const char*)buf, 3, 23 + (10 * (idx)), 0x0, COLOR_BG);
                }
                // draw_string("2. KALSHITICKER", 3, 33, 0x0, COLOR_BG);
                // draw_string("3. KALSHITICKER", 3, 43, 0x0, COLOR_BG);
                // draw_string("4. KALSHITICKER", 3, 53, 0x0, COLOR_BG);
                // draw_string("5. KALSHITICKER", 3, 63, 0x0, COLOR_BG);
                // draw_string("6. KALSHITICKER", 3, 73, 0x0, COLOR_BG);
                // draw_string("7. KALSHITICKER", 3, 83, 0x0, COLOR_BG);
            }
            if (tbp) {
                tbp = 0;
                refresh = 1;
                if (press == BTN_A) {
                    int min = num_holdings < MAX_DISPLAYED_HOLDINGS ? num_holdings : MAX_DISPLAYED_HOLDINGS;
                    ticker = ticker < min - 1 ? ticker + 1 : 0;
                    continue;
                }
                if (press == BTN_B) {
                    game_state = BET_CHOICE;
                    first_name = 0;
                    yes_no = 0;
                    refresh = 1;
                    continue;
                }   
                if (press == BTN_C) {
                    game_state = HOME;
                    ticker = 0;
                    refresh = 1;
                    continue;
                }
                if (press == BTN_D) {
                    game_state = GACHA;
                    yes_no = 0;
                    ticker = 0;
                    refresh = 1;
                    continue;
                }
            }
        }
        else if (game_state == BET_CHOICE) {
            // st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
            // draw_string("0123456789ABCDE", 3, 23 + 12, 0x0, COLOR_BG);
            // draw_string("0123456789ABCDE", 3, 33 + 12, 0x0, COLOR_BG);
            // draw_string("0123456789ABCDE", 3, 43 + 12, 0x0, COLOR_BG);
            //             // "    1     5    "
            //             // "   10    MAX   "
            // draw_string("59%", 3 + (8 * 3), 67 + 12, 0x0, COLOR_BG);
            // draw_string("41%", 3 + (8 * 10), 67 + 12, 0x0, COLOR_BG);
            
            // draw_string("YES", 3 + (8 * 3), 57 + 12, 0xffff, 0x0);
            // draw_string("NO", 3 + (8 * 10), 57 + 12, 0x0, COLOR_BG);

            // delay_ms(5000);

            // draw_string("YES", 3 + (8 * 3), 57 + 12, 0x0, COLOR_BG);
            // draw_string("NO", 3 + (8 * 10), 57 + 12, 0xffff, 0x0);
            if (refresh) {
                st7735_fill_window(gacha_mode ? (void*)&gacha[0] : (void*)&feed[2], 0, WIDTH - 1, 0, 19);
                st7735_fill_window(gacha_mode ? (void*)&gacha[0][89 * 128] : (void*)&feed[2][89 * 128], 0, WIDTH - 1, 89, HEIGHT - 1);
                refresh = 0;
                unsigned len = kalshi_get_name_length(ticker) <= MAX_NAME_LENGTH ? kalshi_get_name_length(ticker) : MAX_NAME_LENGTH;
                unsigned cursor = 0;
                char buf[len + 1];
                unsigned chunk = 1;
                if (first_name == 0) {
                    st7735_fill_window(gacha_mode ? (void*)&gacha[0] : (void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                    while (cursor < len) {
                        kalshi_get_name(ticker, chunk++, &buf[cursor]);
                        cursor += 8;
                    }
                    buf[len] = '\0';
                    for (int i = 0; (i*15) < len && i < MAX_NAME_ROWS; i++) {
                        int min = len - (i*15) > 15 ? 15 : len - (i*15);
                        char temp[16];
                        memcpy(temp, &buf[i*15], min);
                        temp[min] = '\0';
                        draw_string(temp, 3, 23 + 4 + (i * 10), 0x0, COLOR_BG);
                    }          
                    first_name = 1;
                }
                char temp[4];
                unsigned yes = kalshi_get_yes(ticker);
                temp[0] = yes / 10 + '0';
                temp[1] = yes % 10 + '0';
                temp[2] = '%';
                temp[3] = '\0';
                draw_string(temp, 3 + (8 * 3), 67 + 12, 0x0, COLOR_BG);
                unsigned no = kalshi_get_no(ticker);
                temp[0] = no / 10 + '0';
                temp[1] = no % 10 + '0'; 
                draw_string(temp, 3 + (8 * 10), 67 + 12, 0x0, COLOR_BG);
                draw_string("YES", 3 + (8 * 3), 57 + 12, !yes_no ? 0xff : 0x0, !yes_no ? 0x0 : COLOR_BG);
                draw_string("NO", 3 + (8 * 10), 57 + 12, yes_no ? 0xff : 0x0, yes_no ? 0x0 : COLOR_BG);
                // draw_string("59%", 3 + (8 * 3), 67 + 12, 0x0, COLOR_BG);
                // draw_string("59%", 3 + (8 * 3), 67 + 12, 0x0, COLOR_BG);
                // draw_string("41%", 3 + (8 * 10), 67 + 12, 0x0, COLOR_BG);
                // draw_string("YES", 3 + (8 * 3), 57 + 12, 0x0, COLOR_BG);
                // draw_string("NO", 3 + (8 * 10), 57 + 12, 0xffff, 0x0);
            }
            if (tbp) {
                tbp = 0;
                refresh = 1;
                if (press == BTN_A) {
                    yes_no = !yes_no;
                    continue;
                }
                if (press == BTN_B) {
                    game_state = QTY_SELECT;
                    refresh = 1;
                    qty_no = 0;
                    continue;
                }   
                if (press == BTN_C) {
                    game_state = HOME;
                    ticker = 0;
                    continue;
                }
                if (press == BTN_D) {
                    game_state = GACHA;
                    ticker = 0;
                    refresh = 1;
                    continue;
                }
            }
        }
        else if (game_state == QTY_SELECT) {
            if (refresh) {
                st7735_fill_window(gacha_mode ? (void*)&gacha[0] : (void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                refresh = 0;
                if (buy_sell_no == 0) {
                    draw_string("      BUY      ", 3, 23 + 8, 0x0, COLOR_BG);
                } 
                else {
                    draw_string("      SELL     ", 3, 23 + 8, 0x0, COLOR_BG);
                }

                unsigned ask = kalshi_get_ask(ticker, yes_no == 0 ? "yes" : "no");
                unsigned bid = kalshi_get_bid(ticker, yes_no == 0 ? "yes" : "no");
                unsigned price = (buy_sell_no == 0) ? ask : bid;
                char cost_str[8];
                cost_str[3] = 'c';
                cost_str[4] = '\0';
                // 1 contract
                unsigned c1 = price * 1;
                cost_str[0] = (c1 / 100) % 10 + '0'; cost_str[1] = (c1 / 10) % 10 + '0'; cost_str[2] = c1 % 10 + '0';
                draw_string(cost_str, 3 + (8 * 4) - 4, 47 + 12 - 15, 0x0, COLOR_BG);

                // 5 contracts
                unsigned c5 = price * 5;
                cost_str[0] = (c5 / 100) % 10 + '0'; cost_str[1] = (c5 / 10) % 10 + '0'; cost_str[2] = c5 % 10 + '0'; 
                draw_string(cost_str, 3 + (8 * 9) - 4, 47 + 12 - 15, 0x0, COLOR_BG);

                // 10 contracts
                unsigned c10 = price * 10;
                cost_str[0] = (c10 / 100) % 10 + '0'; cost_str[1] = (c10 / 10) % 10 + '0'; cost_str[2] = c10 % 10 + '0'; 
                draw_string(cost_str, 3 + (8 * 4) - 4, 47 + 12 + 10, 0x0, COLOR_BG);

                // all contracts...
                unsigned max_qty = kalshi_get_contracts(ticker);
                unsigned cmax = price * max_qty;
                cost_str[0] = (cmax / 100) % 10 + '0'; cost_str[1] = (cmax / 10) % 10 + '0'; cost_str[2] = cmax % 10 + '0';
                draw_string(cost_str, 3 + (8 * 9) - 4, 47 + 12 + 10, 0x0, COLOR_BG);

                draw_string("1", 3 + (8 * 5), 47 + 12 - 5, qty_no == 0 ? 0xff : 0x0, qty_no == 0 ? 0x0 : COLOR_BG);
                draw_string("5", 3 + (8 * 10), 47 + 12 - 5, qty_no == 1 ? 0xff : 0x0, qty_no == 1 ? 0x0 : COLOR_BG);
                draw_string("10", 3 + (8 * 4), 47 + 12 + 20, qty_no == 2 ? 0xff : 0x0, qty_no == 2 ? 0x0 : COLOR_BG);
                draw_string("MAX", 3 + (8 * 9), 47 + 12 + 20, qty_no == 3 ? 0xff : 0x0, qty_no == 3 ? 0x0 : COLOR_BG);
            }
            if (tbp) {
                tbp = 0;
                refresh = 1;
                
                if (press == BTN_A) {
                    qty_no = (qty_no + 1) % 4; 
                    continue;
                }
                
                if (press == BTN_B) {
                    // trade...
                    // Buy contracts at ask price. count = 1, 5, or 10. Returns 1 on OK, 0 on error.
                    // unsigned kalshi_buy(unsigned idx, const char *side, unsigned count);
                    // // Sell contracts at bid price. count = 1, 5, 10, or 0 for ALL. Returns 1 on OK, 0 on error.
                    // unsigned kalshi_sell(unsigned idx, const char *side, unsigned count);

                    // TODO: Fix to actual numbers before demo...!
                    unsigned actual_qty = 1;
                    if (qty_no == 1) actual_qty = 5; // change to 5 later im scared rn
                    else if (qty_no == 2) actual_qty = 10; // change to 10 later im hella scared rn
                    else if (qty_no == 3) actual_qty = 999; // change to 999 later im insanely scraed rn
                    actual_qty = actual_qty;
                    
                    unsigned success = 0;

                    if (buy_sell_no == 0) {
                        success = kalshi_buy(ticker, yes_no == 0 ? "yes" : "no", actual_qty);
                    }
                    else {
                        success = kalshi_sell(ticker, yes_no == 0 ? "yes" : "no", actual_qty);
                    }

                    // printk("ORDER:%d:%d:%d:%d\n", buy_sell_no, ticker, yes_no, actual_qty);

                    delay_ms(2500);
                    // if (success) {
                    if (success) {
                        // update locally and in file...
                        last_trade_time = rtc_get_total_seconds(rtc);
                        write_last_trade(rtc);
                        printk("ltt: %d\n", get_last_trade_time());
                        // TODO: update file by writing here...
                    }
                    st7735_fill_window((void*)&feed[success ? 3 : 7], 0, WIDTH - 1, 0, HEIGHT - 1);
                    // if (gacha_mode) st7735_fill_window((void*)&gacha[0][88 * 128], 0, WIDTH - 1, 88, HEIGHT - 1);
                    delay_ms(62);

                    st7735_fill_window((void*)&feed[success ? 4 : 8], 0, WIDTH - 1, 0, HEIGHT - 1);
                    // if (gacha_mode) st7735_fill_window((void*)&gacha[0][88 * 128], 0, WIDTH - 1, 88, HEIGHT - 1);
                    delay_ms(62);

                    st7735_fill_window((void*)&feed[success ? 5 : 9], 0, WIDTH - 1, 0, HEIGHT - 1);
                    // if (gacha_mode) st7735_fill_window((void*)&gacha[0][88 * 128], 0, WIDTH - 1, 88, HEIGHT - 1);
                    delay_ms(62);

                    st7735_fill_window((void*)&feed[success ? 6 : 10], 0, WIDTH - 1, 0, HEIGHT - 1);
                    // if (gacha_mode) st7735_fill_window((void*)&gacha[0][88 * 128], 0, WIDTH - 1, 88, HEIGHT - 1);
                    delay_ms(2500);
                    // }
                    game_state = HOME; 
                    continue;
                }   
                
                if (press == BTN_C) {
                    game_state = HOME; 
                    continue;
                }
                if (press == BTN_D) {
                    game_state = GACHA;
                    continue;
                }
            }
        }
        else if (game_state == GAME_HIGHER_LOWER) {
            static unsigned yes_percent = 0;
            if (refresh) {
                if (hol_game_state == 0) {
                    yes_percent = kalshi_pick_random();
                    hol_game_state = 1;
                    st7735_fill_window((void*)&game[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                    unsigned random_idx = 7;
                    unsigned len = kalshi_get_name_length(random_idx) <= MAX_NAME_LENGTH ? kalshi_get_name_length(random_idx) : MAX_NAME_LENGTH;
                    unsigned cursor = 0;
                    char buf[len + 1];
                    unsigned chunk = 1;
                    while (cursor < len) {
                        kalshi_get_name(random_idx, chunk++, &buf[cursor]);
                        cursor += 8;
                    }
                    buf[len] = '\0';
                    for (int i = 0; (i*15) < len && i < MAX_NAME_ROWS; i++) {
                        int min = len - (i*15) > 15 ? 15 : len - (i*15);
                        char temp[16];
                        memcpy(temp, &buf[i*15], min);
                        temp[min] = '\0';
                        draw_string(temp, 3, 23 + 4 + (i * 10), 0x0, COLOR_BG);
                    }     
                }
                // printk("yes:%d\n", yes_percent);
                st7735_fill_window((void*)&game[hol_game_state == 1 ? 2 : 1][79 * 128], 0, WIDTH - 1, 79, HEIGHT - 1);
                // st7735_fill_window((void*)&game[hol_game_state == 0 ? 0 : hol_game_state == 1 ? 2 : 1], 0, WIDTH - 1, 0, HEIGHT - 1);
                refresh = 0;
                draw_string("YES", 3 + (8 * 3), 67 + 12, hol_game_state == 1 ? 0xff : 0x0, hol_game_state == 1  ? 0x0 : COLOR_BG);
                draw_string("NO", 3 + (8 * 10), 67 + 12, hol_game_state == 2 ? 0xff : 0x0, hol_game_state == 2  ? 0x0 : COLOR_BG);
                draw_string("HS:", 3, 97, 0x0, COLOR_BG);
                char hs[3] = {(high_streak / 10) % 10 + '0', high_streak % 10 + '0', '\0'};
                draw_string((const char*)hs, 3 + (8*3), 97, 0x0, COLOR_BG);
                draw_string("C:", 3 + (8 * 11), 97, 0x0, COLOR_BG);
                char curr[3] = {(curr_streak / 10) % 10 + '0', curr_streak % 10 + '0', '\0'};
                draw_string((const char*)curr, 3 + (8 * 13), 97, 0x0, COLOR_BG);
            }
            if (tbp) {
                tbp = 0;
                refresh = 1;
                if (press == BTN_A) {
                    hol_game_state = hol_game_state == 0 ? 1 : hol_game_state == 1 ? 2 : 1;
                    continue;
                }
                if (press == BTN_B) {
                    if (hol_game_state) {
                        if ((hol_game_state == 1 && yes_percent >= 50) || (hol_game_state == 2 && yes_percent < 50)) {
                            curr_streak++;
                        } else {
                            curr_streak  = 0;
                        }
                        hol_game_state = 0;
                        // hol_game_state = 3; 
                    }
                    high_streak = curr_streak > high_streak ? curr_streak : high_streak;
                    continue;
                }   
                if (press == BTN_C) {
                    game_state = HOME;
                    hol_game_state = 0;
                    curr_streak = 0;
                    ticker = 0;
                    continue;
                }
                if (press == BTN_D) {
                    game_state = GACHA;
                    hol_game_state = 0;
                    curr_streak = 0;
                    ticker = 0;
                    refresh = 1;
                    continue;
                }
            }
        }
        else if (game_state == HEALTH) {
            if (refresh) {
                refresh = 0;
                char stype;
                unsigned scount = kalshi_get_streak(&stype);
                cached_losses = (stype == 'L') ? scount : 0;
                cached_losses = 1;
                st7735_fill_window((void*)&health[cached_losses > 3 ? 3 : cached_losses], 0, WIDTH - 1, 0, HEIGHT - 1);
                // st7735_fill_window((void*)&health[3 - get_hearts()], 0, WIDTH - 1, 0, HEIGHT - 1);
            }
            uint32_t now = rtc_get_total_seconds(rtc);
            uint32_t elapsed = now - last_trade_time;
            uint32_t deadline = 24 * 3600; 

            char time_str[9]; // "XXhXXmXXs\0"
            if (elapsed >= deadline) {
                // past time (dead he is dead)
                time_str[0] = '0'; time_str[1] = '0'; time_str[2] = 'h';
                time_str[3] = '0'; time_str[4] = '0'; time_str[5] = 'm';
                time_str[6] = '0'; time_str[7] = '0'; time_str[8] = 's';
            } else {
                uint32_t remaining = deadline - elapsed;
                unsigned hrs = remaining / 3600;
                unsigned mins = (remaining % 3600) / 60;
                unsigned secs = remaining % 60;
                time_str[0] = '0' + (hrs / 10);
                time_str[1] = '0' + (hrs % 10);
                time_str[2] = 'h';
                time_str[3] = '0' + (mins / 10);
                time_str[4] = '0' + (mins % 10);
                time_str[5] = 'm';
                time_str[6] = '0' + (secs / 10);
                time_str[7] = '0' + (secs % 10);
                time_str[8] = 's';
            }
            time_str[9] = '\0';
            draw_string(time_str, 28, 64, 0xffff, 0x0);
            if (tbp) {
                tbp = 0;  
                if (press == BTN_C) {
                    refresh = 1;
                    game_state = HOME;
                    continue;
                }
                if (press == BTN_D) {
                    game_state = GACHA;
                    hol_game_state = 0;
                    refresh = 1;
                    continue;
                }
            }

        }
        else if (game_state == GACHA) {
            kalshi_pick_random();
            gacha_mode = 1;
            ticker = 7;
            buy_sell_no = 0;
            yes_no = 0;
            first_name = 0;
            game_state = BET_CHOICE;
        }
        else if (game_state == SLEEP) {
            if (tbp) {
                tbp = 0;
                game_state = HOME;
                refresh = 1;

                st7735_fill_window((void*)&sit[1], 0, WIDTH - 1, 0, HEIGHT - 1);
                delay_ms(250);

                st7735_fill_window((void*)&sit[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                delay_ms(250);   
                continue; 
            }
            if (refresh) {
                st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                delay_ms(250);

                st7735_fill_window((void*)&sit[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                delay_ms(250);

                st7735_fill_window((void*)&sit[1], 0, WIDTH - 1, 0, HEIGHT - 1);
                delay_ms(250);

                st7735_fill_window((void*)&sit[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                delay_ms(250);

                // sleep test animation WORKS
                for(int i = 0; i < 3; i++) {
                    st7735_fill_window((void*)&sleep[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                    delay_ms(125);

                    st7735_fill_window((void*)&sleep[1], 0, WIDTH - 1, 0, HEIGHT - 1);
                    delay_ms(125);

                    st7735_fill_window((void*)&sleep[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                    delay_ms(125);

                    st7735_fill_window((void*)&sit[2], 0, WIDTH - 1, 0, HEIGHT - 1);
                    delay_ms(250);
                }
                refresh = 0;
            }
        }   
        else if (game_state == DEATH) {
            if (refresh) {
                refresh = 0;
                st7735_fill_window((void*)&death[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                draw_string("   GAME OVER   ", 3, 20, 0xffff, 0x0);
                draw_string("  Press B to   ", 3, 70, 0xffff, 0x0);
                draw_string("    restart    ", 3, 78, 0xffff, 0x0);
            }
            if (tbp) {
                tbp = 0;
                if (press == BTN_B) {
                    // reset EVERYTHINGG
                    write_field_data("loss_streak", "0");
                    write_field_data("win_streak", "0");
                    write_field_data("num_trades", "0");
                    write_field_data("state", "healthy");
                    write_field_data("level", "1");
                    write_field_data("peak", "NA");
                    write_last_trade(rtc);
                    last_trade_time = rtc_get_total_seconds(rtc);
                    cached_losses = 0;
                    hearts = 3;
                    high_streak = 0;
                    curr_streak = 0;
                    ticker = 0;
                    game_state = HOME;
                    refresh = 1;
                }
            }
}
        else if (game_state == STATS) {
            if (tbp) {
                tbp = 0;
                refresh = 1;
                game_state = HOME;
                continue;
            }
            if (refresh) {
                refresh = 0;
                st7735_fill_window((void*)&stats[0], 0, WIDTH - 1, 0, HEIGHT - 1);
                draw_string(get_hearts_readable(), 3, 23, 0x0, COLOR_BG);
                draw_string(get_balance_readable(), 3, 33, 0x0, COLOR_BG);
                draw_string(get_age_readable(rtc), 3, 43, 0x0, COLOR_BG);
                draw_string(get_num_trades_readable(), 3, 53, 0x0, COLOR_BG);
                draw_string(get_loss_streak_readable(), 3, 63, 0x0, COLOR_BG);
                draw_string(get_win_streak_readable(), 3, 73, 0x0, COLOR_BG);
                draw_string(get_level_readable(), 3, 83, 0x0, COLOR_BG);
            }
        }
    }


            // static int once = 1;
            // if (once) {
            //     once = 0;
            //     printk("HOLDINGS");
            //     delay_ms(1000);
            //     printk("TRENDING");
            //     delay_ms(1000);
            //     printk("PROFIT");
            //     delay_ms(1000);
            //     printk("BALANCE");
            //     delay_ms(1000);
            //     printk("T:0:1");
            //     delay_ms(1000);
            //     printk("T:0:2");
            //     delay_ms(1000);
            //     printk("N:0:1");
            //     delay_ms(1000);
            //     printk("N:0:2");
            //     delay_ms(1000);
            //     printk("N:0:3");
            //     delay_ms(1000);
            //     printk("P:0:y");
            //     delay_ms(1000);
            //     printk("P:0:n");
            //     delay_ms(1000);
            //     printk("C:0");
            //     delay_ms(1000);
            //     printk("L:0");
            //     int balance = kalshi_get_balance();
            //     printk("Bal: %d", balance);
            // }

    // 
    st7735_fill_window((void*)&menu[0], 0, WIDTH - 1, 0, HEIGHT - 1);
    delay_ms(1000);
    for (int i = 0; i < 2; i++) {
        st7735_fill_window((void*)&feed[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(750);

        st7735_fill_window((void*)&feed[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(750);

        st7735_fill_window((void*)&feed[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(750);

        st7735_fill_window((void*)&feed[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(750);

        st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(500);
    }

    // trade selection screen
    st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
    // delay_ms(500);
        // draw_string(t, 0, 20, 0x0, COLOR_BG);
    draw_string("1. KALSHITICKER", 3, 23, 0x0, COLOR_BG);
    draw_string("2. KALSHITICKER", 3, 33, 0x0, COLOR_BG);
    draw_string("3. KALSHITICKER", 3, 43, 0x0, COLOR_BG);
    draw_string("4. KALSHITICKER", 3, 53, 0x0, COLOR_BG);
    draw_string("5. KALSHITICKER", 3, 63, 0x0, COLOR_BG);
    draw_string("6. KALSHITICKER", 3, 73, 0x0, COLOR_BG);
    draw_string("7. KALSHITICKER", 3, 83, 0x0, COLOR_BG);

    delay_ms(1000);
    draw_string("1.", 3, 23, 0xffff, 0x0);

    delay_ms(5000);

    st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
    draw_string("0123456789ABCDE", 3, 23 + 12, 0x0, COLOR_BG);
    draw_string("0123456789ABCDE", 3, 33 + 12, 0x0, COLOR_BG);
    draw_string("0123456789ABCDE", 3, 43 + 12, 0x0, COLOR_BG);
                // "    1     5    "
                // "   10    MAX   "
    draw_string("59%", 3 + (8 * 3), 67 + 12, 0x0, COLOR_BG);
    draw_string("41%", 3 + (8 * 10), 67 + 12, 0x0, COLOR_BG);
    
    draw_string("YES", 3 + (8 * 3), 57 + 12, 0xffff, 0x0);
    draw_string("NO", 3 + (8 * 10), 57 + 12, 0x0, COLOR_BG);

    delay_ms(5000);

    draw_string("YES", 3 + (8 * 3), 57 + 12, 0x0, COLOR_BG);
    draw_string("NO", 3 + (8 * 10), 57 + 12, 0xffff, 0x0);

    delay_ms(5000);
    st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
    draw_string("      BUY      ", 3, 23 + 12, 0x0, COLOR_BG);
    draw_string("1", 3 + (8 * 5), 47 + 12 - 5, 0xff, 0x0);
    draw_string("5", 3 + (8 * 10), 47 + 12 - 5, 0x0, COLOR_BG);
    draw_string("10", 3 + (8 * 4), 47 + 12 + 20 - 5, 0x0, COLOR_BG);
    draw_string("MAX", 3 + (8 * 9), 47 + 12 + 20 - 5, 0x0, COLOR_BG);
    delay_ms(2000);
    st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
    draw_string("      BUY      ", 3, 23 + 12, 0x0, COLOR_BG);
    draw_string("1", 3 + (8 * 5), 47 + 12 - 5, 0x0, COLOR_BG);
    draw_string("5", 3 + (8 * 10), 47 + 12 - 5, 0xff, 0x0);
    draw_string("10", 3 + (8 * 4), 47 + 12 + 20 - 5, 0x0, COLOR_BG);
    draw_string("MAX", 3 + (8 * 9), 47 + 12 + 20 - 5, 0x0, COLOR_BG);
    delay_ms(2000);
    st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
    draw_string("      BUY      ", 3, 23 + 12, 0x0, COLOR_BG);
    draw_string("1", 3 + (8 * 5), 47 + 12 - 5, 0x0, COLOR_BG);
    draw_string("5", 3 + (8 * 10), 47 + 12 - 5, 0x0, COLOR_BG);
    draw_string("10", 3 + (8 * 4), 47 + 12 + 20 - 5, 0xff, 0x0);
    draw_string("MAX", 3 + (8 * 9), 47 + 12 + 20 - 5, 0x0, COLOR_BG);
    delay_ms(2000);
    st7735_fill_window((void*)&feed[2], 0, WIDTH - 1, 0, HEIGHT - 1);
    draw_string("      BUY      ", 3, 23 + 12, 0x0, COLOR_BG);
    draw_string("1", 3 + (8 * 5), 47 + 12 - 5, 0x0, COLOR_BG);
    draw_string("5", 3 + (8 * 10), 47 + 12 - 5, 0x0, COLOR_BG);
    draw_string("10", 3 + (8 * 4), 47 + 12 + 20 - 5, 0x0, COLOR_BG);
    draw_string("MAX", 3 + (8 * 9), 47 + 12 + 20 - 5, 0xff, 0x0);
    delay_ms(2000);


    // works!
    // for (int i = 0; i < 100; i++) {
    //     printk("Random: %d\n", random_gen(0, 99));
    // }

    // const char* str = "The quick";
    // draw_string(str, 10, 10, 0x0, COLOR_BG);
    // const char* t = "$0.01";
    // draw_string(t, 0, 20, 0x0, COLOR_BG);

    // idle test animation WORKS
    for(int i = 0; i < 5; i++) {
        st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&idle[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&idle[2], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);
    }
    // idle test animation WORKS
    for(int i = 0; i < 10; i++) {
        st7735_fill_window((void*)&happy[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(125);

        st7735_fill_window((void*)&happy[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(125);
    }
    // wave test animation WORKS
    for (int i = 0; i < 4; i++) {
        st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(500);

        st7735_fill_window((void*)&wave[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&wave[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&wave[2], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&wave[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&wave[2], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&wave[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&wave[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(500);
    }
    // manu test animation WORKS
    for (int i = 0; i < 2; i++) {
        st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(500);
        for (int j = 0; j < 8; j++) {
            st7735_fill_window((void*)&sel_menu[j], 0, WIDTH - 1, 0, HEIGHT - 1);
            delay_ms(250);
        }
        st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(500);
    }
    // sit test animation WORKS
        st7735_fill_window((void*)&idle[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&sit[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&sit[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        st7735_fill_window((void*)&sit[2], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

        delay_ms(1000);

    // sleep test animation WORKS
    for(int i = 0; i < 10; i++) {
        st7735_fill_window((void*)&sleep[0], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(125);

        st7735_fill_window((void*)&sleep[1], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(125);

        st7735_fill_window((void*)&sleep[2], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(125);

        st7735_fill_window((void*)&sit[2], 0, WIDTH - 1, 0, HEIGHT - 1);
        delay_ms(250);

    }

}
