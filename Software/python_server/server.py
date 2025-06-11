from flask import Flask, request, jsonify, render_template_string
from collections import deque
import threading
import datetime

# --- Configuration ---
# Use a deque to store a fixed number of recent data points
MAX_DATA_POINTS = 50
MAX_LOG_ENTRIES = 100

# --- In-memory Data Storage ---
# Thread-safe data structures
data_lock = threading.Lock()
sensor_data = deque(maxlen=MAX_DATA_POINTS)
esp_logs = deque(maxlen=MAX_LOG_ENTRIES)
command_to_send = ""

# --- Flask App Initialization ---
app = Flask(__name__)

# --- HTML & JavaScript Template ---
# Using render_template_string to keep everything in one file for simplicity
HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Sensor Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f4f4f4; }
        .container { max-width: 1000px; margin: auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1, h2 { text-align: center; color: #333; }
        .chart-container { position: relative; height: 40vh; width: 100%; }
        .monitor-container { display: flex; gap: 20px; margin-top: 30px; }
        .serial-output, .command-input { flex: 1; }
        #serial-monitor { width: 100%; height: 200px; background-color: #222; color: #0f0; font-family: 'Courier New', monospace; padding: 10px; border-radius: 4px; overflow-y: scroll; box-sizing: border-box;}
        form { display: flex; }
        input[type="text"] { flex-grow: 1; padding: 8px; border: 1px solid #ccc; border-radius: 4px 0 0 4px; }
        button { padding: 8px 15px; border: none; background-color: #007BFF; color: white; border-radius: 0 4px 4px 0; cursor: pointer; }
        button:hover { background-color: #0056b3; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Live Sensor Data</h1>
        <div class="chart-container">
            <canvas id="sensorChart"></canvas>
        </div>

        <div class="monitor-container">
            <div class="serial-output">
                <h2>ESP32 Output Log</h2>
                <pre id="serial-monitor">Waiting for logs...</pre>
            </div>
            <div class="command-input">
                <h2>Send Command to ESP32</h2>
                <form id="command-form">
                    <input type="text" id="command" name="command" placeholder="Type command and press Enter">
                    <button type="submit">Send</button>
                </form>
            </div>
        </div>
    </div>

    <script>
        // --- Chart.js Setup ---
        const ctx = document.getElementById('sensorChart').getContext('2d');
        const sensorChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [], // Timestamps
                datasets: [{
                    label: 'Sensor Value',
                    data: [], // Sensor readings
                    borderColor: 'rgba(75, 192, 192, 1)',
                    backgroundColor: 'rgba(75, 192, 192, 0.2)',
                    borderWidth: 2,
                    tension: 0.4,
                    fill: true
                }]
            },
            options: {
                scales: { y: { beginAtZero: false } },
                maintainAspectRatio: false
            }
        });

        const serialMonitor = document.getElementById('serial-monitor');

        // --- Function to fetch data and update UI ---
        async function updateData() {
            try {
                const response = await fetch('/graph-data');
                if (!response.ok) {
                    console.error("Failed to fetch data");
                    return;
                }
                const data = await response.json();

                // Update Chart
                sensorChart.data.labels = data.labels;
                sensorChart.data.datasets[0].data = data.values;
                sensorChart.update();

                // Update Serial Monitor
                serialMonitor.textContent = data.logs.join('\\n');
                serialMonitor.scrollTop = serialMonitor.scrollHeight; // Auto-scroll to bottom
            } catch (error) {
                console.error("Error updating data:", error);
            }
        }
        
        // --- Event Listener for Command Form ---
        document.getElementById('command-form').addEventListener('submit', async function(event) {
            event.preventDefault(); // Prevent page reload
            const commandInput = document.getElementById('command');
            const command = commandInput.value;
            
            if (command) {
                await fetch('/send-command', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: `command=${encodeURIComponent(command)}`
                });
                commandInput.value = ''; // Clear input field
            }
        });

        // --- Initial data load and periodic updates ---
        updateData();
        setInterval(updateData, 2000); // Update every 2 seconds
    </script>
</body>
</html>
"""

# --- Helper Functions ---;
def add_log(message):
    """Adds a timestamped log message."""
    timestamp = datetime.datetime.now().strftime('%H:%M:%S')
    esp_logs.append(f"[{timestamp}] {message}")

# --- API Endpoints / Routes ---

@app.route('/')
def index():
    """Serves the main HTML page."""
    return render_template_string(HTML_TEMPLATE)

@app.route('/data', methods=['POST'])
def receive_data():
    """Endpoint for the ESP32 to send data to."""
    global command_to_send
    try:
        data = request.get_json()
        value = data['value']
        
        with data_lock:
            # Store sensor data
            timestamp = datetime.datetime.now().strftime('%H:%M:%S')
            sensor_data.append({'time': timestamp, 'value': value})
            add_log(f"Received data from ESP32: {value}")
            
            # Prepare command to send back to ESP32
            response_command = command_to_send
            command_to_send = "" # Clear command after sending

        # Return the command in the response to the ESP32
        return jsonify({"status": "success", "command": response_command})

    except Exception as e:
        with data_lock:
            add_log(f"Error receiving data: {e}")
        return jsonify({"status": "error", "message": str(e)}), 400

@app.route('/graph-data', methods=['GET'])
def get_graph_data():
    """Endpoint for the web page to fetch data for the graph."""
    with data_lock:
        labels = [d['time'] for d in sensor_data]
        values = [d['value'] for d in sensor_data]
        logs = list(esp_logs)
    return jsonify({'labels': labels, 'values': values, 'logs': logs})

@app.route('/send-command', methods=['POST'])
def send_command():
    """Endpoint for the web page to send a command."""
    global command_to_send
    command = request.form.get('command')
    if command:
        with data_lock:
            command_to_send = command
            add_log(f"Command queued for ESP32: '{command}'")
    return jsonify({"status": "command queued"})


# --- Main Execution ---
if __name__ == '__main__':
    # Running on 0.0.0.0 makes the server accessible from other devices on your network
    app.run(host='0.0.0.0', port=5000, debug=True)