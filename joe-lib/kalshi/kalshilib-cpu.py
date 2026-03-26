'''
handles the computer side of pi-computer communication for the Kalshi interface
'''

from kalshi_client import KalshiClient
import os
import serial
import signal
import sys

def signal_handler(sig, frame):
    print("\nCtrl+c received, closing connection")
    sys.exit(0)

'''
Expected input format (TODO: refine this)
bottom 2 bits of first number represent function:
    00 = buy
    01 = sell
    etc

if op is buy/sell, next 2 bits = quantity:
    00 = 1
    01 = 5
    10 = 10
    11 = all (only for sell)
'''
def handle_pi_input(client, line):
    # init conversion dicts
    # hardcode ticker for now
    # todo: should probably make this a type, but use a string for now
    find_op = {0: "buy", 1: "sell"}
    find_qty = {0: 1, 1: 5, 2: 10, 3:-1}
    first_ch = int(line[0])
    op = find_op[first_ch & 0b11]
    qty = find_qty[(first_ch >> 2) & 0b11]
    if op == "buy":
        response = client.buy(ticker="KXNASDAQ100POS-26DEC31H1600-T25249.85", side="yes", contracts=qty, price_cents=58)
        order_info = response.get("order", {})
        status = order_info.get("status")
        filled = order_info.get("fill_count_fp")
        if status == "filled":
            print(f"Success! Bought {filled} contracts.")
        elif status == "executed":
            print(f"Order executed successfully!")
        elif status == "resting":
            print(f"Order is 'Resting'. Waiting for a seller at your price.")
        else:
            print(f"Order accepted but status is: {status}")
    elif op == "sell":
        response = client.sell(ticker="KXNASDAQ100POS-26DEC31H1600-T25249.85", side="yes", contracts=qty, price_cents=60)
        order_info = response.get("order", {})
        status = order_info.get("status")
        filled = order_info.get("fill_count_fp")
        if status == "filled":
            print(f"Success! Sold {filled} contracts.")
        elif status == "executed":
            print(f"Order executed successfully!")
        elif status == "resting":
            print(f"Order is 'Resting'. Waiting for a seller at your price.")
        else:
            print(f"Order accepted but status is: {status}")
    
'''
TODO: make a client class which tracks infomation about the tamagotchi.
For example, every time a new ticker is added, we should update the class.
This way, can just send index number of ticker from pi side, and we would
know how to understand it
'''

def main():
    if not (os.environ.get("KALSHI_API_KEY_ID") and os.environ.get("KALSHI_PRIVATE_KEY_PATH")):
        print("No credentials. Set:")
        print("  export KALSHI_API_KEY_ID='your-key-id'")
        print("  export KALSHI_PRIVATE_KEY_PATH='/path/to/kalshi-key.txt'")
        return

    client = KalshiClient()
    print("Initialized client")

    port = "/dev/cu.usbserial-130" # me because thats me device, old julian device is 1130
    baud = 921600
    signal.signal(signal.SIGINT, signal_handler)
    ser = serial.Serial(port, baud, timeout=None)
    print(f"Connected to {port}...")

    # start listening to pi commands
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8').strip()
            # print(f"received: {line}, but sending 0 for buy")
            # line = "0"
            print(f"received: {line}, but sending 1 for sell")
            line = "1"
            handle_pi_input(client, line)
            return

if __name__=='__main__':
    main()
