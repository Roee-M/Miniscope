import socket
import threading
from flask import Flask, render_template, send_file, make_response
from flask_socketio import SocketIO
import io

ESP_IP = "192.168.4.1"
ESP_PORT = 4210
BUFFER_SIZE = 1500
DATA_SIZE = 6 * 1000 * 1000  # 6 MB

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="eventlet")

# Store the last capture in memory
last_capture = None

def udp_receiver():
    global last_capture
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', 0))  # bind to any free port

    print("Sending reset signal...")
    sock.sendto(b'r', (ESP_IP, ESP_PORT))

    socketio.sleep(0.2)

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

    # Save to memory for HTTP transfer
    last_capture = bytes(received)

    # Notify client we're done capturing
    socketio.emit('done', namespace='/scope')

@app.route('/')
def index():
    return render_template('index.html')  # index.html in /templates

@app.route('/capture.bin')
def get_capture():
    global last_capture
    if last_capture is None:
        return "No capture available yet", 404

    buf = io.BytesIO(last_capture)
    response = make_response(send_file(buf, mimetype='application/octet-stream', as_attachment=False, download_name='capture.bin'))
    return response

@socketio.on('start_trigger', namespace='/scope')
def handle_trigger():
    print("Trigger requested from browser")
    socketio.start_background_task(udp_receiver)

if __name__ == '__main__':
    socketio.run(app, debug=True, port=8000)
