#include "rpi.h"
#include "kalshi-interface.h"

// Basic send command
void kalshi_send(const char *cmd) {
    while (*cmd) {
        uart_put8(*cmd);
        cmd++;
    }
    uart_put8('\n');
}

// Poll uart for incoming data from Kalshiii
unsigned kalshi_recv_str(char *buf, unsigned max) {
    unsigned i = 0;
    unsigned timeout = 0;

    while (i < max) {
        timeout = 0;
        while (!uart_has_data()) {
            delay_us(10);
            timeout++;
            if (timeout > 500000) {
                buf[i] = '\0';
                return i;
            }
        }
        char c = (char)uart_get8();
        if (c == '\n' || c == '\r') {
            if (i == 0) continue;
            break;
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

// Same as receive but including simple atoi inside
int kalshi_recv_int(void) {
    char buf[16];
    kalshi_recv_str(buf, 15);

    int neg = 0;
    int val = 0;
    const char *s = buf;

    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return neg ? -val : val;
}

// Construct command via our custom interface...
static void build_cmd(char *cmd, char prefix, unsigned idx, char suffix) {
    unsigned pos = 0;
    cmd[pos++] = prefix;
    cmd[pos++] = ':';
    if (idx >= 10) cmd[pos++] = '0' + (idx / 10);
    cmd[pos++] = '0' + (idx % 10);
    cmd[pos++] = ':';
    cmd[pos++] = suffix;
    cmd[pos] = '\0';
}

unsigned kalshi_load_top10(void) {
    kalshi_send("TOP10");
    return (unsigned)kalshi_recv_int();
}

unsigned kalshi_load_holdings(void) {
    kalshi_send("HOLDINGS");
    return (unsigned)kalshi_recv_int();
}

unsigned kalshi_get_balance(void) {
    kalshi_send("BALANCE");
    return (unsigned)kalshi_recv_int();
}

int kalshi_get_profit(void) {
    kalshi_send("PROFIT");
    return kalshi_recv_int();
}

void kalshi_get_ticker(unsigned idx, unsigned chunk, char *buf) {
    char cmd[12];
    build_cmd(cmd, 'T', idx, '0' + chunk);
    kalshi_send(cmd);
    kalshi_recv_str(buf, 8);
}

void kalshi_get_name(unsigned idx, unsigned chunk, char *buf) {
    char cmd[12];
    build_cmd(cmd, 'N', idx, '0' + chunk);
    kalshi_send(cmd);
    kalshi_recv_str(buf, 8);
}

unsigned kalshi_get_name_length(unsigned idx) {
    char cmd[8];
    unsigned pos = 0;
    cmd[pos++] = 'L';
    cmd[pos++] = ':';
    if (idx >= 10) cmd[pos++] = '0' + (idx / 10);
    cmd[pos++] = '0' + (idx % 10);
    cmd[pos] = '\0';
    kalshi_send(cmd);
    return (unsigned)kalshi_recv_int();
}

unsigned kalshi_get_yes(unsigned idx) {
    char cmd[10];
    build_cmd(cmd, 'P', idx, 'y');
    kalshi_send(cmd);
    return (unsigned)kalshi_recv_int();
}

unsigned kalshi_get_no(unsigned idx) {
    char cmd[10];
    build_cmd(cmd, 'P', idx, 'n');
    kalshi_send(cmd);
    return (unsigned)kalshi_recv_int();
}

unsigned kalshi_get_contracts(unsigned idx) {
    // "C:NN"
    char cmd[8];
    unsigned pos = 0;
    cmd[pos++] = 'C';
    cmd[pos++] = ':';
    if (idx >= 10) cmd[pos++] = '0' + (idx / 10);
    cmd[pos++] = '0' + (idx % 10);
    cmd[pos] = '\0';
    kalshi_send(cmd);
    return (unsigned)kalshi_recv_int();
}

unsigned kalshi_get_ask(unsigned idx, const char *side) {
    // "A:NN:y" or "A:NN:n"
    char cmd[10];
    build_cmd(cmd, 'A', idx, side[0] == 'y' ? 'y' : 'n');
    kalshi_send(cmd);
    return (unsigned)kalshi_recv_int();
}
 
unsigned kalshi_get_bid(unsigned idx, const char *side) {
    // "D:NN:y" or "D:NN:n"
    char cmd[10];
    build_cmd(cmd, 'D', idx, side[0] == 'y' ? 'y' : 'n');
    kalshi_send(cmd);
    return (unsigned)kalshi_recv_int();
}

unsigned kalshi_buy(unsigned idx, const char *side, unsigned count) {
    // builds: "BUY:NN:yes:CC" 
    char cmd[24];
    unsigned pos = 0;

    cmd[pos++] = 'B';
    cmd[pos++] = 'U';
    cmd[pos++] = 'Y';
    cmd[pos++] = ':';

    if (idx >= 10) cmd[pos++] = '0' + (idx / 10);
    cmd[pos++] = '0' + (idx % 10);
    cmd[pos++] = ':';

    const char *s = side;
    while (*s) cmd[pos++] = *s++;
    cmd[pos++] = ':';

    if (count >= 10) cmd[pos++] = '0' + (count / 10);
    cmd[pos++] = '0' + (count % 10);
    cmd[pos] = '\0';

    kalshi_send(cmd);

    char resp[24];
    kalshi_recv_str(resp, 23);
    return (resp[0] == 'O' && resp[1] == 'K');
}

unsigned kalshi_sell(unsigned idx, const char *side, unsigned count) {
    // builds: "SELL:NN:yes:CC" or "SELL:NN:yes:ALL"
    char cmd[28];
    unsigned pos = 0;

    cmd[pos++] = 'S';
    cmd[pos++] = 'E';
    cmd[pos++] = 'L';
    cmd[pos++] = 'L';
    cmd[pos++] = ':';

    if (idx >= 10) cmd[pos++] = '0' + (idx / 10);
    cmd[pos++] = '0' + (idx % 10);
    cmd[pos++] = ':';

    const char *s = side;
    while (*s) cmd[pos++] = *s++;
    cmd[pos++] = ':';

    if (count == 0) {
        cmd[pos++] = 'A';
        cmd[pos++] = 'L';
        cmd[pos++] = 'L';
    } else {
        if (count >= 10) cmd[pos++] = '0' + (count / 10);
        cmd[pos++] = '0' + (count % 10);
    }
    cmd[pos] = '\0';

    kalshi_send(cmd);

    char resp[24];
    kalshi_recv_str(resp, 23);
    return (resp[0] == 'O' && resp[1] == 'K');
}

unsigned kalshi_pick_random(void) {
    kalshi_send("GRANDOM");
    return (unsigned)kalshi_recv_int();
}

unsigned kalshi_get_streak(char *type) {
    kalshi_send("STREAK");
    char buf[8];
    kalshi_recv_str(buf, 7);
    *type = buf[0];  // 'W', 'L', or 'N'
    unsigned val = 0;
    const char *s = &buf[1];
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}