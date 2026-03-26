import serial
import signal
import sys

port = "/dev/cu.usbserial-210" # me because thats me device, old julian device is 1130
baud = 921600

def signal_handler(sig, frame):
    print("\nCtrl+c received, closing connection")
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

ser = serial.Serial(port, baud, timeout=None)
print(f"Connected to {port}...")

counter = 0
last_sent = -1 # Track the last counter value we sent a message for

while True:
    if ser.in_waiting > 0:
        line = ser.readline().decode('utf-8').strip()
        print(line)
        counter += 1
        
    # Only send if counter is a multiple of 5, AND greater than 0, AND we haven't sent it yet
    if counter > 0 and (counter % 5) == 0 and counter != last_sent:
        # Add a newline so the Pi knows the message is over!
        # msg = f"{counter}: Hello from computer!\n"
        msg = "1234567\n"
        ser.write(msg.encode('utf-8'))
        last_sent = counter