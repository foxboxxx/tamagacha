// EXAMPLE FUNCTION USAGE:
//   unsigned count = kalshi_load_top10();    // loads top 10 bets by 24h vol
//   kalshi_get_ticker(0, 1, buf);           // first 8 chars of ticker
//   kalshi_get_ticker(0, 2, buf);           // next 8 chars of ticker
//   kalshi_get_name(0, 1, buf);             // name chars 0-7
//   kalshi_get_name(0, 2, buf);             // name chars 8-15
//   kalshi_get_name(0, 3, buf);             // name chars 16-23...
//   unsigned yes = kalshi_get_yes(0);       // yes %
//   unsigned no  = kalshi_get_no(0);        // no %
//   unsigned bal = kalshi_get_balance();     // balance in cents
//   int profit   = kalshi_get_profit();       // net profit in cents (THIS IS AT ASKING PRICE!!!)
//   unsigned ok  = kalshi_buy(0, "yes", 5);   // buy 5 contracts
//   unsigned ok  = kalshi_sell(0, "yes", 0);   // sell ALL (0 = all)
#pragma once

// Send a command string (adds \n)
void kalshi_send(const char *cmd);

// Receive newline-terminated string into buf, up to max chars. Returns length.
unsigned kalshi_recv_str(char *buf, unsigned max);

// Receive an integer response
int kalshi_recv_int(void);

// Load top 10 markets by 24h volume. Returns count.
unsigned kalshi_load_top10(void);

// Load current holdings. Returns count.
unsigned kalshi_load_holdings(void);

// Get balance in cents
unsigned kalshi_get_balance(void);

// Get net profit in cents (can be negative)
int kalshi_get_profit(void);

// Get 8-char chunk of ticker. chunk=1 -> bytes 0-7, chunk=2 -> bytes 8-15
void kalshi_get_ticker(unsigned idx, unsigned chunk, char *buf);

// Get 8-char chunk of name. chunk=1 -> 0-7, chunk=2 -> 8-15, chunk=3 -> 16-23
void kalshi_get_name(unsigned idx, unsigned chunk, char *buf);

// Get the length of a name.
unsigned kalshi_get_name_length(unsigned idx);

// Ask price in cents (cost per contract to buy). side = "yes" or "no"
unsigned kalshi_get_ask(unsigned idx, const char *side);
 
// Bid price in cents (what you get per contract selling). side = "yes" or "no"
unsigned kalshi_get_bid(unsigned idx, const char *side);

// How many contracts you hold at this index
unsigned kalshi_get_contracts(unsigned idx);

// Get yes probability (0-100)
unsigned kalshi_get_yes(unsigned idx);

// Get no probability (0-100)
unsigned kalshi_get_no(unsigned idx);

// Buy contracts at ask price. count = 1, 5, or 10. Returns 1 on OK, 0 on error.
unsigned kalshi_buy(unsigned idx, const char *side, unsigned count);

// Sell contracts at bid price. count = 1, 5, 10, or 0 for ALL. Returns 1 on OK, 0 on error.
unsigned kalshi_sell(unsigned idx, const char *side, unsigned count);

// Pick a random market from top 100 by 24h volume, store at index 7.
// Returns yes %. Then use kalshi_get_name(7, ...), kalshi_get_yes(7), etc.
unsigned kalshi_pick_random(void);

// Return current streak, of type type (W,L).
unsigned kalshi_get_streak(char *type);