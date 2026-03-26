"""
Kalshi API Client
=================
pip install requests cryptography

export KALSHI_API_KEY_ID='your-key-id'
export KALSHI_PRIVATE_KEY_PATH='/path/to/kalshi-key.txt'
"""

import os
import uuid
import random
import base64
import datetime
import requests
from typing import Optional

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.hazmat.backends import default_backend


PROD_BASE_URL = "https://api.elections.kalshi.com"
DEMO_BASE_URL = "https://demo-api.kalshi.co"
API_PATH_PREFIX = "/trade-api/v2"


class KalshiClient:

    def __init__(
        self,
        api_key_id: Optional[str] = None,
        private_key_path: Optional[str] = None,
        demo: bool = False,
    ):
        self.api_key_id = api_key_id or os.environ.get("KALSHI_API_KEY_ID", "")
        self.base_url = (DEMO_BASE_URL if demo else PROD_BASE_URL) + API_PATH_PREFIX
        self._session = requests.Session()

        key_path = private_key_path or os.environ.get("KALSHI_PRIVATE_KEY_PATH", "")
        with open(key_path, "rb") as f:
            pem_bytes = f.read()

        self._private_key = serialization.load_pem_private_key(
            pem_bytes, password=None, backend=default_backend(),
        )

    # --- Auth ---

    def _sign(self, text: str) -> str:
        sig = self._private_key.sign(
            text.encode("utf-8"),
            padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=padding.PSS.DIGEST_LENGTH),
            hashes.SHA256(),
        )
        return base64.b64encode(sig).decode("utf-8")

    def _headers(self, method: str, path: str) -> dict:
        ts = str(int(datetime.datetime.now().timestamp() * 1000))
        msg = ts + method.upper() + path.split("?")[0]
        return {
            "KALSHI-ACCESS-KEY": self.api_key_id,
            "KALSHI-ACCESS-TIMESTAMP": ts,
            "KALSHI-ACCESS-SIGNATURE": self._sign(msg),
            "Content-Type": "application/json",
        }

    # --- HTTP ---

    def _get(self, path: str, params: Optional[dict] = None) -> dict:
        full = API_PATH_PREFIX + path
        r = self._session.get(self.base_url + path, headers=self._headers("GET", full), params=params)
        r.raise_for_status()
        return r.json() if r.text else {}

    def _post(self, path: str, json: Optional[dict] = None) -> dict:
        full = API_PATH_PREFIX + path
        r = self._session.post(self.base_url + path, headers=self._headers("POST", full), json=json)
        r.raise_for_status()
        return r.json() if r.text else {}

    # --- Helpers for parsing API dollar strings ---

    @staticmethod
    def _dollar_to_pct(dollar_str) -> int:
        """Convert dollar string like '0.5600' to percentage like 56."""
        try:
            return int(round(float(dollar_str) * 100))
        except (ValueError, TypeError):
            return 0

    @staticmethod
    def _parse_volume(m: dict) -> float:
        """Parse volume_fp string like '1523.00' to float."""
        try:
            return float(m.get("volume_fp", "0") or "0")
        except (ValueError, TypeError):
            return 0.0

    # --- Portfolio ---

    def get_balance(self) -> dict:
        d = self._get("/portfolio/balance")
        return {
            "balance_cents": d.get("balance", 0),
            "portfolio_value_cents": d.get("portfolio_value", 0),
        }

    def get_positions(self) -> list:
        d = self._get("/portfolio/positions", params={"settlement_status": "unsettled", "limit": 200})
        return d.get("market_positions", [])

    def get_total_winnings(self) -> dict:
        rev, cost, cursor = 0, 0, None
        while True:
            params = {"limit": 200}
            if cursor:
                params["cursor"] = cursor
            d = self._get("/portfolio/settlements", params=params)
            for s in d.get("settlements", []):
                r = s.get("revenue", 0)
                c = s.get("cost", 0) or s.get("total_cost", 0)
                rev += float(r) if isinstance(r, str) else r
                cost += float(c) if isinstance(c, str) else c
            cursor = d.get("cursor")
            if not cursor or not d.get("settlements"):
                break
        return {"revenue_cents": int(rev), "cost_cents": int(cost), "profit_cents": int(rev - cost)}

    def get_total_profit(self) -> dict:
        pnl, fees, cursor = 0.0, 0.0, None
        while True:
            params = {"limit": 200}
            if cursor:
                params["cursor"] = cursor
            d = self._get("/portfolio/positions", params=params)
            for p in d.get("market_positions", []):
                pnl += float(p.get("realized_pnl_dollars", "0"))
                fees += float(p.get("fees_paid_dollars", "0"))
            cursor = d.get("cursor")
            if not cursor or not d.get("market_positions"):
                break
        return {"pnl_dollars": round(pnl, 4), "fees_dollars": round(fees, 4), "net_dollars": round(pnl - fees, 4)}

    # --- Markets ---
    # KEY: mve_filter=exclude removes all combo/parlay junk

    def get_markets(self, status: str = "open", limit: int = 1000, series_ticker: Optional[str] = None) -> list:
        """
        Get real individual markets (excludes combo/parlay MVE markets).
        Returns raw list of market dicts.
        """
        params = {"status": status, "limit": limit, "mve_filter": "exclude"}
        if series_ticker:
            params["series_ticker"] = series_ticker
        d = self._get("/markets", params=params)
        return d.get("markets", [])

    def get_trending(self, count: int = 10) -> list:
        """Random `count` from top 100 real markets sorted by volume."""
        mkts = self.get_markets(status="open", limit=1000)
        mkts.sort(key=lambda m: self._parse_volume(m), reverse=True)
        top = mkts[:100]
        return random.sample(top, min(count, len(top)))

    def get_market(self, ticker: str) -> dict:
        d = self._get(f"/markets/{ticker}")
        return d.get("market", d)

    def get_events(self, status: str = "open", with_markets: bool = True, limit: int = 200) -> list:
        """
        Get events (already excludes multivariate/combo).
        With with_markets=True, each event includes its nested markets.
        """
        params = {"status": status, "limit": limit, "with_nested_markets": str(with_markets).lower()}
        d = self._get("/events", params=params)
        return d.get("events", [])

    # --- Buy ---

    def buy(self, ticker: str, side: str = "yes", contracts: int = 1, price_cents: int = 50) -> dict:
        body = {
            "ticker": ticker,
            "side": side,
            "action": "buy",
            "client_order_id": str(uuid.uuid4()),
            "count": contracts,
            "type": "limit",
        }
        if side == "yes":
            body["yes_price"] = price_cents
        else:
            body["no_price"] = price_cents
        return self._post("/portfolio/orders", json=body)

    def buy_1(self, ticker: str, side: str = "yes", price_cents: int = 50) -> dict:
        return self.buy(ticker, side, contracts=1, price_cents=price_cents)

    def buy_5(self, ticker: str, side: str = "yes", price_cents: int = 50) -> dict:
        return self.buy(ticker, side, contracts=5, price_cents=price_cents)

    def buy_10(self, ticker: str, side: str = "yes", price_cents: int = 50) -> dict:
        return self.buy(ticker, side, contracts=10, price_cents=price_cents)

    # --- Sell ---

    def sell(self, ticker: str, side: str = "yes", contracts: int = 1, price_cents: int = 50) -> dict:
        body = {
            "ticker": ticker,
            "side": side,
            "action": "sell",
            "client_order_id": str(uuid.uuid4()),
            "count": contracts,
            "type": "limit",
        }
        if side == "yes":
            body["yes_price"] = price_cents
        else:
            body["no_price"] = price_cents
        return self._post("/portfolio/orders", json=body)

    def sell_1(self, ticker: str, side: str = "yes", price_cents: int = 50) -> dict:
        return self.sell(ticker, side, contracts=1, price_cents=price_cents)

    def sell_5(self, ticker: str, side: str = "yes", price_cents: int = 50) -> dict:
        return self.sell(ticker, side, contracts=5, price_cents=price_cents)

    def sell_10(self, ticker: str, side: str = "yes", price_cents: int = 50) -> dict:
        return self.sell(ticker, side, contracts=10, price_cents=price_cents)

    def sell_all(self, ticker: str, side: str = "yes", price_cents: int = 50) -> dict:
        positions = self.get_positions()
        count = 0
        for p in positions:
            if p.get("ticker") == ticker:
                count = int(float(p.get("position_fp", "0")))
                break
        if count <= 0:
            return {"error": "no position found", "ticker": ticker}
        return self.sell(ticker, side, contracts=count, price_cents=price_cents)


# ---------------------------------------------------------------------------
# Helpers for printing market info
# ---------------------------------------------------------------------------

def print_market(m: dict, index: int = 0):
    """Print one market nicely. Works with raw API market dict."""
    title = m.get("title", "N/A")
    ticker = m.get("ticker", "")
    yes = KalshiClient._dollar_to_pct(m.get("yes_bid_dollars", "0"))
    yes_ask = KalshiClient._dollar_to_pct(m.get("yes_ask_dollars", "0"))
    last = KalshiClient._dollar_to_pct(m.get("last_price_dollars", "0"))
    vol = m.get("volume_fp", "0")
    vol_24h = m.get("volume_24h_fp", "0")

    # Best price to show: last_price if available, else yes_bid
    best_yes = last if last > 0 else yes
    best_no = 100 - best_yes if best_yes > 0 else 0

    prefix = f"  {index:>2}. " if index else "  "
    print(f"{prefix}{title}")
    print(f"      Ticker: {ticker}")
    print(f"      Yes: {best_yes}%  No: {best_no}%  |  Bid: {yes}c  Ask: {yes_ask}c  Last: {last}c")
    print(f"      Volume: {vol}  (24h: {vol_24h})")
    print()


# ---------------------------------------------------------------------------
# Example main
# ---------------------------------------------------------------------------

def main():
    if not (os.environ.get("KALSHI_API_KEY_ID") and os.environ.get("KALSHI_PRIVATE_KEY_PATH")):
        print("No credentials. Set:")
        print("  export KALSHI_API_KEY_ID='your-key-id'")
        print("  export KALSHI_PRIVATE_KEY_PATH='/path/to/kalshi-key.txt'")
        return

    client = KalshiClient()

    # Balance
    print("Balance:")
    bal = client.get_balance()
    print(f"  Available: ${bal['balance_cents'] / 100:.2f}")
    print(f"  Portfolio: ${bal['portfolio_value_cents'] / 100:.2f}")
    print()

    # Profit
    print("Profit:")
    profit = client.get_total_profit()
    print(f"  Realized P&L: ${profit['pnl_dollars']:.2f}")
    print(f"  Fees paid:    ${profit['fees_dollars']:.2f}")
    print(f"  Net profit:   ${profit['net_dollars']:.2f}")
    print()

    # Winnings
    print("Winnings:")
    w = client.get_total_winnings()
    print(f"  Revenue: ${w['revenue_cents'] / 100:.2f}")
    print(f"  Cost:    ${w['cost_cents'] / 100:.2f}")
    print(f"  Profit:  ${w['profit_cents'] / 100:.2f}")
    print()

    # Holdings
    print("Current Holdings:")
    positions = client.get_positions()
    if positions:
        for i, p in enumerate(positions, 1):
            tk = p.get("ticker", "?")
            pos = p.get("position_fp", "0")
            pnl = p.get("realized_pnl_dollars", "0")
            try:
                mkt = client.get_market(tk)
                title = mkt.get("title", tk)
            except Exception:
                title = tk
            print(f"  {i}. {title}")
            print(f"     Ticker: {tk}")
            print(f"     Position: {pos} contracts  P&L: ${pnl}")
        print(f"  Total open: {len(positions)}")
    else:
        print("  None")
    print()

    # --- 10 random trending (sorted by volume, mve_filter=exclude) ---
    print("=" * 70)
    print("10 RANDOM TRENDING MARKETS (by volume)")
    print("=" * 70)
    trending = client.get_trending(10)
    if trending:
        for i, m in enumerate(trending, 1):
            print_market(m, i)
    else:
        print("  No markets found.")
    print()

    # --- Sort by volume: top 10 ---
    print("=" * 70)
    print("TOP 10 BY VOLUME (highest all-time volume)")
    print("=" * 70)
    all_mkts = client.get_markets(status="open", limit=1000)
    by_volume = sorted(all_mkts, key=lambda m: KalshiClient._parse_volume(m), reverse=True)
    for i, m in enumerate(by_volume[:10], 1):
        print_market(m, i)
    print()

    # --- Sort by 24h volume ---
    print("=" * 70)
    print("TOP 10 BY 24H VOLUME (most active today)")
    print("=" * 70)
    by_24h = sorted(all_mkts, key=lambda m: float(m.get("volume_24h_fp", "0") or "0"), reverse=True)
    for i, m in enumerate(by_24h[:10], 1):
        print_market(m, i)
    print()

    # --- Sort by closest to 50/50 ---
    print("=" * 70)
    print("TOP 10 CLOSEST TO 50/50 (most contested)")
    print("=" * 70)
    def distance_from_50(m):
        last = KalshiClient._dollar_to_pct(m.get("last_price_dollars", "0"))
        bid = KalshiClient._dollar_to_pct(m.get("yes_bid_dollars", "0"))
        price = last if last > 0 else bid
        if price == 0:
            return 999  # no price = push to bottom
        return abs(price - 50)
    by_5050 = sorted(all_mkts, key=distance_from_50)
    for i, m in enumerate(by_5050[:10], 1):
        print_market(m, i)
    print()

    # --- Sort by most volatile (biggest spread between bid and ask) ---
    print("=" * 70)
    print("TOP 10 WIDEST SPREAD (volatile / illiquid)")
    print("=" * 70)
    def spread(m):
        bid = KalshiClient._dollar_to_pct(m.get("yes_bid_dollars", "0"))
        ask = KalshiClient._dollar_to_pct(m.get("yes_ask_dollars", "0"))
        if bid == 0 and ask == 0:
            return -1
        return ask - bid
    by_spread = sorted(all_mkts, key=spread, reverse=True)
    for i, m in enumerate(by_spread[:10], 1):
        print_market(m, i)
    print()

    # --- Sort by open interest ---
    print("=" * 70)
    print("TOP 10 BY OPEN INTEREST (most money at stake)")
    print("=" * 70)
    by_oi = sorted(all_mkts, key=lambda m: float(m.get("open_interest_fp", "0") or "0"), reverse=True)
    for i, m in enumerate(by_oi[:10], 1):
        print_market(m, i)
    print()

    # --- Buy/sell reference ---
    print("=" * 70)
    print("BUY / SELL REFERENCE")
    print("=" * 70)
    print("  Buy:")
    print("    client.buy_1('TICKER', 'yes', price_cents=50)")
    print("    client.buy_5('TICKER', 'yes', price_cents=50)")
    print("    client.buy_10('TICKER', 'no', price_cents=30)")
    print()
    print("  Sell:")
    print("    client.sell_1('TICKER', 'yes', price_cents=60)")
    print("    client.sell_5('TICKER', 'yes', price_cents=60)")
    print("    client.sell_10('TICKER', 'no', price_cents=40)")
    print("    client.sell_all('TICKER', 'yes', price_cents=60)")


if __name__ == "__main__":
    main()