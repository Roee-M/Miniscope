import socket

ESP_IP = "192.168.4.1"      # ESP32 AP IP address
ESP_PORT = 4210             # UDP port to send trigger and receive data
BUFFER_SIZE = 1500          # UDP packet max size (bytes)
DATA_SIZE = 6 * 1000 * 1000 # 6 MB expected data size

def send_trigger(sock):
    print("Sending trigger to ESP32...")
    sock.sendto(b"trigger", (ESP_IP, ESP_PORT))

def receive_data(sock):
    received_data = bytearray()
    print("Receiving data...")
    sock.settimeout(10)  # seconds timeout for waiting data

    try:
        while len(received_data) < DATA_SIZE:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            received_data.extend(data)
            print(f"Received {len(data)} bytes, total: {len(received_data)}")
    except socket.timeout:
        print("Socket timeout, stopping reception.")

    return received_data

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', 0))  # Bind to any free port

    send_trigger(sock)
    data = receive_data(sock)

    print(f"Total bytes received: {len(data)}")
    with open("miniscope_capture.bin", "wb") as f:
        f.write(data)
    print("Data saved to miniscope_capture.bin")

if __name__ == "__main__":
    main()
