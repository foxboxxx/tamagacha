"""
Kalshi <-> Pi UART Bridge
=========================
Do in terminal before running:
pip install requests cryptography pyserial
export KALSHI_API_KEY_ID='your-key-id'
export KALSHI_PRIVATE_KEY_PATH='/path/to/kalshi-key.txt'

All indexes reference HOLDINGS sorted by position size (biggest first).
"""

import serial, signal, sys, time, os, uuid, random, base64, datetime, requests
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.hazmat.backends import default_backend

SERIAL_PORT = "/dev/cu.usbserial-210"
BAUD_RATE = 921600
PROD_BASE_URL = "https://api.elections.kalshi.com"
DEMO_BASE_URL = "https://demo-api.kalshi.co"
API_PATH_PREFIX = "/trade-api/v2"

# API commands
class KalshiClient:
    def __init__(self, demo=False):
        self.api_key_id = os.environ.get("KALSHI_API_KEY_ID", "")
        self.base_url = (DEMO_BASE_URL if demo else PROD_BASE_URL) + API_PATH_PREFIX
        self._session = requests.Session()
        with open(os.environ.get("KALSHI_PRIVATE_KEY_PATH", ""), "rb") as f:
            self._private_key = serialization.load_pem_private_key(f.read(), password=None, backend=default_backend())

    def _sign(self, text):
        return base64.b64encode(self._private_key.sign(text.encode("utf-8"),
            padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=padding.PSS.DIGEST_LENGTH),
            hashes.SHA256())).decode("utf-8")

    def _headers(self, method, path):
        ts = str(int(datetime.datetime.now().timestamp() * 1000))
        return {"KALSHI-ACCESS-KEY": self.api_key_id, "KALSHI-ACCESS-TIMESTAMP": ts,
                "KALSHI-ACCESS-SIGNATURE": self._sign(ts + method.upper() + path.split("?")[0]),
                "Content-Type": "application/json"}

    def _get(self, path, params=None):
        r = self._session.get(self.base_url + path, headers=self._headers("GET", API_PATH_PREFIX + path), params=params)
        r.raise_for_status(); return r.json() if r.text else {}

    def _post(self, path, json=None):
        r = self._session.post(self.base_url + path, headers=self._headers("POST", API_PATH_PREFIX + path), json=json)
        r.raise_for_status(); return r.json() if r.text else {}

    @staticmethod
    def _dollar_to_pct(val):
        try: return int(round(float(val) * 100))
        except: return 0

    def get_balance(self):
        return self._get("/portfolio/balance").get("balance", 0)

    def get_positions(self):
        positions = self._get("/portfolio/positions", params={"settlement_status": "unsettled", "limit": 200}).get("market_positions", [])
        # Filter out ghost positions with 0 contracts
        positions = [p for p in positions if abs(float(p.get("position_fp", "0"))) > 0]
        positions.sort(key=lambda p: abs(float(p.get("position_fp", "0"))), reverse=True)
        return positions

    def get_total_profit(self):
        pnl, fees, cursor = 0.0, 0.0, None
        while True:
            params = {"limit": 200}
            if cursor: params["cursor"] = cursor
            d = self._get("/portfolio/positions", params=params)
            for p in d.get("market_positions", []):
                pnl += float(p.get("realized_pnl_dollars", "0"))
                fees += float(p.get("fees_paid_dollars", "0"))
            cursor = d.get("cursor")
            if not cursor or not d.get("market_positions"): break
        return int((pnl - fees) * 100)

    def get_total_winnings(self):
        rev, cost, cursor = 0, 0, None
        while True:
            params = {"limit": 200}
            if cursor: params["cursor"] = cursor
            d = self._get("/portfolio/settlements", params=params)
            for s in d.get("settlements", []):
                r, c = s.get("revenue", 0), s.get("cost", 0) or s.get("total_cost", 0)
                rev += float(r) if isinstance(r, str) else r
                cost += float(c) if isinstance(c, str) else c
            cursor = d.get("cursor")
            if not cursor or not d.get("settlements"): break
        return {"revenue_cents": int(rev), "cost_cents": int(cost), "profit_cents": int(rev - cost)}
    
    def get_streak(self):
        d = self._get("/portfolio/settlements", params={"limit": 200})
        settlements = d.get("settlements", [])
        if not settlements:
            return {"streak": 0, "type": "none"}
        streak = 0
        streak_type = None
        for s in settlements:
            rev = float(s.get("revenue", 0)) if isinstance(s.get("revenue", 0), str) else s.get("revenue", 0)
            cost = float(s.get("cost", 0) or s.get("total_cost", 0))
            if isinstance(cost, str): cost = float(cost)
            won = rev > cost
            if streak_type is None:
                streak_type = "win" if won else "loss"
                streak = 1
            elif (won and streak_type == "win") or (not won and streak_type == "loss"):
                streak += 1
            else:
                break
        return {"streak": streak, "type": streak_type or "none"}

    def get_top_by_24h_volume(self, count=10):
        """Fetch up to 3000 open markets (paginated), sort by 24h volume, return top count."""
        all_mkts = []
        cursor = None
        for _ in range(3):  # 3 pages x 1000 = up to 3000 markets
            params = {"status": "open", "limit": 1000, "mve_filter": "exclude"}
            if cursor:
                params["cursor"] = cursor
            d = self._get("/markets", params=params)
            page = d.get("markets", [])
            all_mkts.extend(page)
            cursor = d.get("cursor")
            if not cursor or not page:
                break
        all_mkts.sort(key=lambda m: float(m.get("volume_24h_fp", "0") or "0"), reverse=True)
        return all_mkts[:count]

    def get_market(self, ticker):
        d = self._get(f"/markets/{ticker}"); return d.get("market", d)

    def buy(self, ticker, side="yes", contracts=1):
        mkt = self.get_market(ticker)
        ask = int(round(float(mkt.get("yes_ask_dollars" if side == "yes" else "no_ask_dollars", "0.50")) * 100))
        if ask <= 0: ask = 50
        body = {"ticker": ticker, "side": side, "action": "buy", "client_order_id": str(uuid.uuid4()),
                "count": contracts, "type": "limit", ("yes_price" if side == "yes" else "no_price"): ask}
        return self._post("/portfolio/orders", json=body)

    def sell(self, ticker, side="yes", contracts=1):
        mkt = self.get_market(ticker)
        bid = int(round(float(mkt.get("yes_bid_dollars" if side == "yes" else "no_bid_dollars", "0.50")) * 100))
        if bid <= 0: bid = 50
        body = {"ticker": ticker, "side": side, "action": "sell", "client_order_id": str(uuid.uuid4()),
                "count": contracts, "type": "limit", ("yes_price" if side == "yes" else "no_price"): bid}
        return self._post("/portfolio/orders", json=body)

    def sell_all(self, ticker, side="yes"):
        for p in self.get_positions():
            if p.get("ticker") == ticker:
                c = int(float(p.get("position_fp", "0")))
                if c > 0: return self.sell(ticker, side, c)
        return {"error": "no position"}

# Keep track of holdings to limit API sends
class HoldingsCache:
    def __init__(self): self.holdings = []

    def load(self, client):
        self.holdings = []
        for p in client.get_positions()[:10]:
            tk = p.get("ticker", "")
            try: mkt = client.get_market(tk)
            except: mkt = {"title": tk, "ticker": tk}
            self.holdings.append((p, mkt))
        return len(self.holdings)

    def _mkt(self, i): return self.holdings[i][1] if i < len(self.holdings) else None
    def _pos(self, i): return self.holdings[i][0] if i < len(self.holdings) else None

    def ticker(self, i):
        m = self._mkt(i); return m.get("ticker", "") if m else ""
    @staticmethod
    def _clean_ascii(s):
        """Replace non-ASCII chars with closest ASCII equivalent or strip them."""
        replacements = {'°': ' deg ', '–': '-', '—': '-', ''': "'", ''': "'",
                        '"': '"', '"': '"', '…': '...', '±': '+/-', '×': 'x',
                        '€': 'EUR', '£': 'GBP', '¥': 'JPY'}
        for k, v in replacements.items():
            s = s.replace(k, v)
        return ''.join(c if ord(c) < 128 else '' for c in s)

    def ticker_chunk(self, i, c):
        return self.ticker(i).ljust(16)[(c-1)*8:(c-1)*8+8].ljust(8)
    def name_chunk(self, i, c):
        m = self._mkt(i)
        title = self._clean_ascii(m.get("title", "") if m else "")
        padded = title.ljust(c * 8)
        return padded[(c-1)*8:(c-1)*8+8].ljust(8)
    def yes_pct(self, i):
        m = self._mkt(i)
        if not m: return 0
        last, bid = float(m.get("last_price_dollars","0") or "0"), float(m.get("yes_bid_dollars","0") or "0")
        return int(round((last if last > 0 else bid) * 100))
    def no_pct(self, i):
        y = self.yes_pct(i); return 100 - y if y > 0 else 0
    def contracts(self, i):
        p = self._pos(i); return int(float(p.get("position_fp", "0"))) if p else 0
    def pnl(self, i):
        p = self._pos(i); return p.get("realized_pnl_dollars", "0.00") if p else "0.00"
    def name_length(self, i):
        m = self._mkt(i); return len(self._clean_ascii(m.get("title", ""))) if m else 0
    def ask_price(self, i, side="yes"):
        """Current ask price in cents for buying. This is what you'd pay per contract."""
        m = self._mkt(i)
        if not m: return 0
        field = "yes_ask_dollars" if side == "yes" else "no_ask_dollars"
        return int(round(float(m.get(field, "0") or "0") * 100))
    def bid_price(self, i, side="yes"):
        """Current bid price in cents for selling. This is what you'd get per contract."""
        m = self._mkt(i)
        if not m: return 0
        field = "yes_bid_dollars" if side == "yes" else "no_bid_dollars"
        return int(round(float(m.get(field, "0") or "0") * 100))

    @staticmethod
    def _is_yesno_title(title):
        """
        Filter for markets that read as natural yes/no questions.
        Keeps: "Will X happen?", "X above 54?"
        Rejects: "What will X say...", "Who will win...", statement-style
        """
        t = title.lower().strip()
        if "?" not in t:
            return False
        # Reject "what/who/which/where/when/how many" .... multi-outcome questions (we need binary yes/no)
        bad_starts = ("what ", "who ", "which ", "where ", "when ", "how many ",
                      "how much ", "how long ", "how far ")
        if any(t.startswith(s) for s in bad_starts):
            return False
        # Good; starts with will/can/does/is etc
        good_starts = ("will ", "can ", "does ", "is ", "are ", "has ", "have ",
                       "do ", "did ", "would ", "should ", "could ")
        if any(t.startswith(s) for s in good_starts):
            return True
        # Good; contains threshold language (e.g. "S&P 500 above 6000?")
        threshold_words = ("above ", "below ", "over ", "under ", "at least ",
                           "more than", "less than", "higher than", "lower than",
                           "between ", "or more", "or fewer")
        if any(w in t for w in threshold_words):
            return True
        return False

    def pick_random(self, client):
        """
        Pick a random yes/no market from top by 24h volume and store at index 7.
        Filters for markets with question-style titles (real yes/no bets).
        Returns yes_pct of the picked market.
        """
        mkts = client.get_top_by_24h_volume(500)  # big pool, filter will narrow it
        pool = []
        for m in mkts:
            last = float(m.get("last_price_dollars", "0") or "0")
            bid = float(m.get("yes_bid_dollars", "0") or "0")
            pct = int(round((last if last > 0 else bid) * 100))
            title = m.get("title", "")
            if 1 <= pct <= 99 and self._is_yesno_title(title):
                pool.append(m)
        if not pool:
            return 0
        pick = random.choice(pool)
        dummy_pos = {"ticker": pick.get("ticker", ""), "position_fp": "0", "realized_pnl_dollars": "0"}
        while len(self.holdings) < 8:
            self.holdings.append(({}, {}))
        self.holdings[7] = (dummy_pos, pick)
        last = float(pick.get("last_price_dollars", "0") or "0")
        bid = float(pick.get("yes_bid_dollars", "0") or "0")
        return int(round((last if last > 0 else bid) * 100))

# Print out
def print_console():
    if not (os.environ.get("KALSHI_API_KEY_ID") and os.environ.get("KALSHI_PRIVATE_KEY_PATH")):
        print("Set KALSHI_API_KEY_ID and KALSHI_PRIVATE_KEY_PATH"); return
    client = KalshiClient()

    print("Balance:")
    print(f"  Available: ${client.get_balance() / 100:.2f}\n")
    print("Profit:")
    print(f"  Net profit: ${client.get_total_profit() / 100:.2f}\n")
    print("Winnings:")
    w = client.get_total_winnings()
    print(f"  Revenue: ${w['revenue_cents'] / 100:.2f}")
    print(f"  Cost:    ${w['cost_cents'] / 100:.2f}")
    print(f"  Profit:  ${w['profit_cents'] / 100:.2f}\n")

    print("Current Holdings (sorted by position size):")
    print("-" * 70)
    cache = HoldingsCache()
    count = cache.load(client)
    if count > 0:
        for i in range(count):
            m = cache._mkt(i)
            print(f"  {i}. {m.get('title', '?') if m else '?'}")
            print(f"     Ticker: {cache.ticker(i)}")
            print(f"     Contracts: {cache.contracts(i)}  Yes: {cache.yes_pct(i)}%  No: {cache.no_pct(i)}%  P&L: ${cache.pnl(i)}")
            print()
        print(f"  Total holdings: {count}")
    else:
        print("  None")
    print()

    print("Top 10 by 24h Volume:")
    print("-" * 70)
    for i, m in enumerate(client.get_top_by_24h_volume(10), 1):
        last = KalshiClient._dollar_to_pct(m.get("last_price_dollars", "0"))
        bid = KalshiClient._dollar_to_pct(m.get("yes_bid_dollars", "0"))
        yes = last if last > 0 else bid
        print(f"  {i:>2}. {m.get('title', 'N/A')}")
        print(f"      Ticker: {m.get('ticker', '')}")
        print(f"      Yes: {yes}%  No: {100-yes if yes>0 else 0}%  24h Volume: {m.get('volume_24h_fp', '0')}")
        print()


def send(ser, data):
    ser.write((str(data) + "\n").encode("utf-8")); print(f"  -> {repr(str(data))}")

# Available commands to be read from Pi
def run_uart_bridge():
    if not (os.environ.get("KALSHI_API_KEY_ID") and os.environ.get("KALSHI_PRIVATE_KEY_PATH")):
        print("Set KALSHI_API_KEY_ID and KALSHI_PRIVATE_KEY_PATH"); return
    client, cache = KalshiClient(), HoldingsCache()
    print(f"Opening {SERIAL_PORT} @ {BAUD_RATE}...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    print("Connected.\n")
    signal.signal(signal.SIGINT, lambda s, f: (print("\nClosing..."), ser.close(), sys.exit(0)))

    print("=" * 55)
    print("  KALSHI UART BRIDGE")
    print("  idx 0 = biggest holding")
    print("=" * 55)
    print("  Listening for Pi...\n")

    while True:
        # Read anything the Pi sends
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode("utf-8").strip()
            except:
                continue
            if not line:
                continue

            parts = line.split(":")

            # Check if it's a known command
            if line == "HOLDINGS":
                print(f"  <- {line}")
                send(ser, cache.load(client))
            elif line == "BALANCE":
                print(f"  <- {line}")
                send(ser, client.get_balance())
            elif line == "PROFIT":
                print(f"  <- {line}")
                send(ser, client.get_total_profit())
            elif parts[0] == "T" and len(parts) == 3:
                print(f"  <- {line}")
                send(ser, cache.ticker_chunk(int(parts[1]), int(parts[2])))
            elif parts[0] == "N" and len(parts) == 3:
                print(f"  <- {line}")
                send(ser, cache.name_chunk(int(parts[1]), int(parts[2])))
            elif parts[0] == "P" and len(parts) == 3:
                print(f"  <- {line}")
                send(ser, cache.yes_pct(int(parts[1])) if parts[2] == "y" else cache.no_pct(int(parts[1])))
            elif parts[0] == "C" and len(parts) == 2:
                print(f"  <- {line}")
                send(ser, cache.contracts(int(parts[1])))
            elif parts[0] == "L" and len(parts) == 2:
                print(f"  <- {line}")
                send(ser, cache.name_length(int(parts[1])))
            elif parts[0] == "A" and len(parts) == 3:
                # A:idx:y or A:idx:n -> ask price in cents (cost to buy)
                print(f"  <- {line}")
                side = "yes" if parts[2] == "y" else "no"
                send(ser, cache.ask_price(int(parts[1]), side))
            elif parts[0] == "D" and len(parts) == 3:
                # D:idx:y or D:idx:n -> bid price in cents (what you get selling)
                print(f"  <- {line}")
                side = "yes" if parts[2] == "y" else "no"
                send(ser, cache.bid_price(int(parts[1]), side))
            elif parts[0] == "BUY" and len(parts) == 4:
                print(f"  <- {line}")
                ticker = cache.ticker(int(parts[1]))
                if ticker:
                    try: send(ser, f"OK:{client.buy(ticker, parts[2], int(parts[3])).get('order',{}).get('status','ok')}")
                    except Exception as e: send(ser, f"ERR:{str(e)[:20]}")
                else: send(ser, "ERR:bad_idx")
            elif parts[0] == "SELL" and len(parts) == 4:
                print(f"  <- {line}")
                ticker = cache.ticker(int(parts[1]))
                if not ticker: send(ser, "ERR:bad_idx"); continue
                cnt = cache.contracts(int(parts[1])) if parts[3] == "ALL" else int(parts[3])
                if cnt <= 0: send(ser, "ERR:no_pos"); continue
                try: send(ser, f"OK:{client.sell(ticker, parts[2], cnt).get('order',{}).get('status','ok')}")
                except Exception as e: send(ser, f"ERR:{str(e)[:20]}")
            # --- GRANDOM: pick random market, store at idx 7 ---
            elif line == "GRANDOM":
                print(f"  <- {line}")
                send(ser, cache.pick_random(client))
            elif line == "STREAK":
                print(f"  <- {line}")
                s = client.get_streak()
                prefix = s["type"][0].upper()  # W, L, or N
                send(ser, f"{prefix}{s['streak']}")
            else:
                # Not command -- Print whatever the Pi sent (debug output, printk, etc)
                print(line)

        time.sleep(0.01)

if __name__ == "__main__":
    run_uart_bridge() if "--uart" in sys.argv else print_console()