import socket
import threading
from flask import Flask, render_template
from flask_socketio import SocketIO, emit

ESP_IP = "192.168.4.1"
ESP_PORT = 4210
BUFFER_SIZE = 1500
DATA_SIZE = 6 * 1000 * 1000  # 6MB

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="eventlet")

def udp_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', 0))  # any local port
    sock.sendto(b't', (ESP_IP, ESP_PORT))

    received = bytearray()
    sock.settimeout(10)

    try:
        while len(received) < DATA_SIZE:
            data, _ = sock.recvfrom(BUFFER_SIZE)
            received.extend(data)
    except socket.timeout:
        print("UDP receive timeout.")

    # Stream chunks to the browser
    for i in range(0, len(received), 2):
        raw = received[i] | (received[i+1] << 8)
        voltage = (raw / 4095.0) * 3.3
        socketio.emit('adc_data', {'v': voltage}, namespace='/scope')

@app.route('/')
def index():
    return render_template('index.html')  # Template folder must exist

@socketio.on('start_trigger', namespace='/scope')
def handle_trigger():
    threading.Thread(target=udp_receiver).start()

if __name__ == '__main__':
    socketio.run(app, debug=True, port=8000)
