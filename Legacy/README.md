# Legacy UDP code for Miniscope Project

This file lists the main components of the Miniscope UDP Communication project and describes their function.

serverUDP_firmware.cpp
  - Type: C++ Source File
  - Function: The firmware code for the ESP32-based Miniscope. This file contains the logic for establishing the UDP socket connection, sending Miniscope data, and receiving control commands on the embedded device.

app.py
  - Type: Python Source File
  - Function: The main application for the Python server. It acts as a receiver/sender for UDP packets, typically used to process data from the ESP32 or relay it to a client application (like a phone).

python_server/
  - Type: File Folder
  - Function: Contains auxiliary files, libraries, or modules necessary for the operation of the Python server application (app.py).

templates/
  - Type: File Folder
  - Function: Holds HTML templates or static content, the Python server can host a simple web interface for monitoring or control.