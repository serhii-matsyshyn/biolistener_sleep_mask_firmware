import socket
import json
import time
import threading

def mock_mask():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    
    print("Mock Mask started. Sending pings...")
    while True:
        ping = {"type": "ping", "mask": 255}
        sock.sendto(json.dumps(ping).encode(), ('127.0.0.1', 20000))
        time.sleep(1)

if __name__ == "__main__":
    mock_mask()
