import socket
import threading
from flask import Flask, render_template
from flask_socketio import SocketIO, emit

ESP_IP = "192.168.4.1"
ESP_PORT = 4210
BUFFER_SIZE = 1500
DATA_SIZE = 6 * 1000 * 1000  # 6 MB

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="eventlet")

def udp_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', 0))  # bind to any free port

    # Send 'r' to reset ESP32 state
    print("Sending reset signal...")
    sock.sendto(b'r', (ESP_IP, ESP_PORT))

    # Short wait before sending trigger
    socketio.sleep(0.2)

    # Send 't' to trigger capture
    print("Sending trigger signal...")
    sock.sendto(b't', (ESP_IP, ESP_PORT))

    received = bytearray()
    sock.settimeout(10)

    try:
        while len(received) < DATA_SIZE:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            print(f"Received {len(data)} bytes from {addr}, total: {len(received)}")
            received.extend(data)
    except socket.timeout:
        print("UDP receive timeout.")

    print(f"Total received: {len(received)} bytes")
    sock.close()

    # Stream each 2-byte sample to the browser
    for i in range(0, len(received) - 1, 200):  # 100 samples = 200 bytes
        voltages = []
        for j in range(0, 200, 2):
            if i + j + 1 >= len(received):
                break
            raw = received[i + j] | (received[i + j + 1] << 8)
            voltage = (raw / 4095.0) * 3.3
            voltages.append(voltage)
        socketio.emit('adc_data_batch', {'v': voltages}, namespace='/scope')
        socketio.sleep(0.005)  # Yield control
    
@app.route('/')
def index():
    return render_template('index.html')  # Make sure index.html is in /templates

@socketio.on('start_trigger', namespace='/scope')
def handle_trigger():
    print("Trigger requested from browser")
    socketio.start_background_task(udp_receiver)

if __name__ == '__main__':
    socketio.run(app, debug=True, port=8000)
