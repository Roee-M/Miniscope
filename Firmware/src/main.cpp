// gemini version of the server- using python server not base 64 nice page
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// IMPORTANT: REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "Miniscope";
const char* password = "Miniscope";

// IMPORTANT: REPLACE WITH YOUR COMPUTER'S IP ADDRESS
// Find this on Windows with `ipconfig` or on macOS/Linux with `ifconfig` or `ip a`
const char* serverName = "http://192.168.65.192:5000/data";

unsigned long lastTime = 0;
unsigned long timerDelay = 5000; // Send data every 5 seconds

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
}

void loop() {
  // Send data only every timerDelay milliseconds
  if ((millis() - lastTime) > timerDelay) {
    if(WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      // Start the connection
      http.begin(serverName);
      http.addHeader("Content-Type", "application/json");

      // --- 1. Prepare Data ---
      // This is a simulated sensor value. Replace with your actual sensor reading.
      int sensorValue = random(200, 800);
      
      // Create JSON document
      StaticJsonDocument<128> doc;
      doc["sensor"] = "simulated_temp";
      doc["value"] = sensorValue;

      // Serialize JSON to a string
      String jsonPayload;
      serializeJson(doc, jsonPayload);

      Serial.print("Sending data: ");
      Serial.println(jsonPayload);
      
      // --- 2. Send POST Request ---
      int httpResponseCode = http.POST(jsonPayload);

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        // --- 3. Receive Command in Response ---
        String responsePayload = http.getString();
        
        // Parse the response JSON to get the command
        StaticJsonDocument<128> responseDoc;
        deserializeJson(responseDoc, responsePayload);
        const char* command = responseDoc["command"];

        if (command && strlen(command) > 0) {
            Serial.print("Received command from server: ");
            Serial.println(command);
            // Here you could add if/else statements to act on the command
            // e.g., if (strcmp(command, "LED_ON") == 0) { ... }
        }

      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
      }
      
      http.end(); // Free resources
    } else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}


// websocket interface from base44 
// This code is a simple WebSocket server for the ESP32-S3 Miniscope that sends mock ADC data to connected clients.
// #include <Arduino.h>
// #include <WiFi.h>
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>

// // CHANGE THESE TO YOUR WI-FI CREDENTIALS
// const char* ssid = "Miniscope";
// const char* password = "Miniscope";

// // WebSocket server on port 81
// AsyncWebServer server(80);
// AsyncWebSocket ws("/ws");

// // Data generation variables
// unsigned long lastDataSend = 0;
// const unsigned long DATA_INTERVAL = 10; // Send data every 10ms (100Hz)
// uint8_t dataCounter = 0;

// // WebSocket event handler
// void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
//                AwsEventType type, void *arg, uint8_t *data, size_t len) {
//   switch (type) {
//     case WS_EVT_CONNECT:
//       Serial.printf("WebSocket client connected: %u from %s\n", 
//                    client->id(), client->remoteIP().toString().c_str());
//       break;
      
//     case WS_EVT_DISCONNECT:
//       Serial.printf("WebSocket client disconnected: %u\n", client->id());
//       break;
      
//     case WS_EVT_DATA:
//       // Handle incoming data from web app (if needed)
//       Serial.printf("Received data from client %u\n", client->id());
//       break;
      
//     default:
//       break;
//   }
// }

// void setup() {
//   Serial.begin(115200);
//   delay(1000);
//   Serial.println("ESP32-S3 Miniscope WebSocket Server Starting...");
  
//   // Connect to Wi-Fi
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);
  
//   Serial.print("Connecting to Wi-Fi");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
  
//   Serial.println();
//   Serial.println("WiFi connected successfully!");
//   Serial.print("IP address: ");
//   Serial.println(WiFi.localIP());
//   Serial.print("WebSocket URL: ws://");
//   Serial.print(WiFi.localIP());
//   Serial.println(":80/ws");
  
//   // Set up WebSocket
//   ws.onEvent(onWsEvent);
//   server.addHandler(&ws);
  
//   // Serve a simple info page
//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//     String html = "<!DOCTYPE html><html data-filename='pages/ESP32Setup' data-linenumber='142' data-visual-selector-id='pages/ESP32Setup142'><head data-filename='pages/ESP32Setup' data-linenumber='142' data-visual-selector-id='pages/ESP32Setup142'><title data-filename='pages/ESP32Setup' data-linenumber='142' data-visual-selector-id='pages/ESP32Setup142'>ESP32-S3 Miniscope</title></head>";
//     html += "<body data-filename='pages/ESP32Setup' data-linenumber='143' data-visual-selector-id='pages/ESP32Setup143' style='font-family: Arial; padding: 20px; background: #1a1a1a; color: white;'>";
//     html += "<h1>ESP32-S3 Miniscope Server</h1>";
//     html += "<p data-filename='pages/ESP32Setup' data-linenumber='145' data-visual-selector-id='pages/ESP32Setup145'>WebSocket server is running!</p>";
//     html += "<p data-filename='pages/ESP32Setup' data-linenumber='146' data-visual-selector-id='pages/ESP32Setup146'><strong data-filename='pages/ESP32Setup' data-linenumber='146' data-visual-selector-id='pages/ESP32Setup146'>WebSocket URL:</strong> <code data-filename='pages/ESP32Setup' data-linenumber='146' data-visual-selector-id='pages/ESP32Setup146'>ws://" + WiFi.localIP().toString() + "/ws</code></p>";
//     html += "<p data-filename='pages/ESP32Setup' data-linenumber='147' data-visual-selector-id='pages/ESP32Setup147'>Use this IP address in your web application.</p>";
//     html += "<p data-filename='pages/ESP32Setup' data-linenumber='148' data-visual-selector-id='pages/ESP32Setup148'>Status: <span data-filename='pages/ESP32Setup' data-linenumber='148' data-visual-selector-id='pages/ESP32Setup148' style='color: #4ade80;'>Online</span></p>";
//     html += "</body></html>";
//     request->send(200, "text/html", html);
//   });
  
//   server.begin();
//   Serial.println("HTTP server started");
//   Serial.println("Ready to send data!");
// }

// void loop() {
//   // Clean up disconnected clients
//   ws.cleanupClients();
  
//   // Send data at regular intervals
//   if (millis() - lastDataSend >= DATA_INTERVAL) {
//     if (ws.count() > 0) { // Only send if clients are connected
//       // Generate mock ADC data (you can replace this with real sensor readings)
//       uint8_t mockData[8];
      
//       // Generate different patterns for testing
//       for (int i = 0; i < 8; i++) {
//         // Mix of sine wave, square wave, and noise
//         float sineWave = sin((dataCounter + i) * 0.1) * 127 + 128;
//         uint8_t squareWave = ((dataCounter + i) % 50 < 25) ? 200 : 50;
//         uint8_t noise = random(0, 256);
        
//         // Combine patterns: 50% sine, 30% square, 20% noise
//         mockData[i] = (uint8_t)(sineWave * 0.5 + squareWave * 0.3 + noise * 0.2);
//       }
      
//       // Send binary data to all connected WebSocket clients
//       ws.binaryAll(mockData, sizeof(mockData));
      
//       dataCounter++;
//     }
    
//     lastDataSend = millis();
//   }
  
//   // Small delay to prevent watchdog issues
//   delay(1);
// }

// // last base44 using base64 encoding for sending data over HTTP stream

// #include <Arduino.h>
// #include <WiFi.h>
// #include <ESPAsyncWebServer.h>
// #include <ArduinoJson.h> // For Base64 encoding
// #include "base64.h"


// // --- CHANGE THESE TO YOUR WI-FI CREDENTIALS ---
// const char* ssid = "Miniscope";
// const char* password = "Miniscope";
// // ----------------------------------------------

// AsyncWebServer server(80);
// AsyncEventSource events("/events"); // Create an EventSource at /events

// // Data generation variables
// const int SAMPLES_PER_PACKET = 64;
// uint8_t dataBuffer[SAMPLES_PER_PACKET];
// unsigned long lastDataSendTime = 0;
// const unsigned long DATA_INTERVAL_MS = 10; // Send data every 10ms

// void setup() {
//   Serial.begin(115200);
//   delay(1000);
//   Serial.println("ESP32 HTTP Stream Server Starting...");

//   // Connect to Wi-Fi
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);
//   Serial.print("Connecting to Wi-Fi");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("\nWiFi connected successfully!");
//   Serial.print("IP address: ");
//   Serial.println(WiFi.localIP());
//   Serial.print("Stream URL: http://");
//   Serial.print(WiFi.localIP());
//   Serial.println("/events");

//   // Set up Server-Sent Events
//   events.onConnect([](AsyncEventSourceClient *client){
//     Serial.printf("Client connected from: %s\n", client->client()->remoteIP().toString().c_str());
//     client->send("hello!", NULL, millis(), 1000);
//   });
//   server.addHandler(&events);

//   // Add a simple info page at root
//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//     request->send(200, "text/plain", "ESP32 HTTP Stream Server is running. Connect to /events");
//   });

//   server.begin();
//   Serial.println("HTTP server started.");
// }

// void loop() {
//   if (millis() - lastDataSendTime >= DATA_INTERVAL_MS) {
//     if (events.count() > 0) { // Only generate and send if a client is connected
//       // 1. Generate mock data (e.g., a sine wave)
//       for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
//         float sineValue = sin((millis() / 50.0) + i * 0.5) * 127 + 128;
//         dataBuffer[i] = (uint8_t)sineValue;
//       }

//       // 2. Base64 encode the binary data
//     //   char base64_output[base64_predict_encoded_size(SAMPLES_PER_PACKET) + 1];
//     //   base64_encode(base64_output, (char*)dataBuffer, SAMPLES_PER_PACKET);
//     // char base64_output[base64_enc_len(SAMPLES_PER_PACKET) + 1];
//     // base64_encode((char*)base64_output, (char*)dataBuffer, SAMPLES_PER_PACKET);
//     String base64_output = base64::encode(dataBuffer, SAMPLES_PER_PACKET);



//       // 3. Send the Base64 string to all connected clients
//       events.send(base64_output.c_str(), "message", millis());
//     //   events.send(base64_output, "message", millis());
//     }
//     lastDataSendTime = millis();
//   }
  
//   // A small delay can help with stability on some boards
//   delay(1);
// }

// // from chatgpt- base44
// #include <Arduino.h>
// #include "Adafruit_TinyUSB.h"

// // Create a TinyUSB WebUSB serial interface
// Adafruit_USBD_WebUSB webUSB; 
// bool webConnected = false;

// void setup() {
//   Serial.begin(115200);
//   delay(1000);

//   // Set a landing page (optional, for Chrome WebUSB)
//   webUSB.setLandingPage("https://app--mini-scope-web-copy-69dab43b.base44.app/"); 
//   webUSB.begin();

//   Serial.println("TinyUSB WebUSB example started.");
// }

// void loop() {
//   if (webUSB.connected()) {
//     if (!webConnected) {
//       Serial.println("WebUSB Connected!");
//       webConnected = true;
//     }

//     // Send raw data
//     // uint8_t value = random(0, 255);
//     uint8_t value [sizeof(uint8_t)] = {0xAA}; // Example value to send
//     webUSB.write(value, sizeof(value)); // Send a single byte
//     webUSB.flush();

//     delay(100);  // ~10Hz transmission
//   } else {
//     if (webConnected) {
//       Serial.println("WebUSB Disconnected!");
//       webConnected = false;
//     }
//   }
// }

// // end of chatgpt-base44

// // ----LED EXAMPLE----
// // This example demonstrates how to use WebUSB with an ESP32 to control an LED

// #include <Arduino.h>
// # define LED 2 // Define the LED pin (change as needed)
// // #include "webusb.h"  // Use the correct header for WebUSB device

// // WebUSB WebUSBSerial;
// uint8_t data = 0; // Example data to send

// void setup() {
//     Serial.begin(115200);
//     // while (!Serial) delay(10);

//     Serial.println("Starting ESP32 TinyUSB WebUSB Example...");
//     pinMode(LED, OUTPUT); // Initialize the LED pin as output
//     digitalWrite(LED, HIGH); // Turn on the LED to indicate setup is complete
//     delay(1000); // Wait for a second to indicate the setup is complete

//     // Initialize WebUSB
//     // WebUSBSerial.begin();
//     Serial.println("WebUSB initialized. Open your WebUSB frontend.");
// }

// void loop() {

//     // Blink the LED to indicate the loop is running
//     digitalWrite(LED, LOW); // Turn off the LED
//     delay(500); // Wait for 500 milliseconds
//     digitalWrite(LED, HIGH); // Turn on the LED
//     delay(500); // Wait for 500 milliseconds

//     Serial.println(data);

    

    // // Send a byte every second to the WebUSB frontend
    // static uint8_t data = 0;
    // WebUSBSerial.write(&data, 1);
    // WebUSBSerial.flush();
    // Serial.print("Sent to WebUSB: ");
    // Serial.println(data);
    // data++;
    // delay(1000);

    // // Check if data is received from WebUSB frontend
    // if (WebUSBSerial.available()) {
    //     uint8_t rx = WebUSBSerial.read();
    //     Serial.print("Received from WebUSB: ");
    //     Serial.println(rx);
    //     // Optionally, echo back to frontend
    //     WebUSBSerial.write(&rx, 1);
    //     WebUSBSerial.flush();
    // }
// }


// /*
//     the code below is an old main from 27/05/2025 inorder to create basic connection between esp32
//     and the computer using usb, it is not used anymore but kept for reference
//     specificly webUSB
// */
// // #include <Arduino.h>
// // #include "adc_mock.hpp"
// // #include "usb_comm.hpp"
// // #include "controller.hpp"


// // #define SAMPLE_RATE 1000000 // 1 MSPS
// // #define WINDOW_SEC 0.5
// // #define SAMPLES_PER_WINDOW (size_t)(SAMPLE_RATE * WINDOW_SEC)
// // #define BUFFER_SIZE (SAMPLES_PER_WINDOW * 2) // 2 bytes per sample

// // #define CHUNK_SAMPLES 1024  // Number of samples per chunk (adjust for RAM)
// // #define CHUNK_SIZE (CHUNK_SAMPLES * 2) // 2 bytes per sample

// // uint8_t buffer[CHUNK_SIZE];


// // volatile size_t buffer_index = 0;
// // volatile bool buffer_ready = false;


// // ADCMock adc;
// // USBComm usb;
// // Controller controller;


// // unsigned long lastSampleTime = 0;

// // void setup() {
// //     usb.begin();
// //     adc.begin();
// //     controller.begin();
// // }


// // void loop() {
// //     controller.handleInput();

// //     if (controller.isRunning()) {
// //         // Simulate continuous DMA-style streaming
// //         static size_t sample_offset = 0;
// //         // Fill buffer with square wave chunk
// //         adc.fillSquareWaveBuffer(buffer, CHUNK_SAMPLES, 0x0FFF, 0x0000, 40, sample_offset);
// //         // Send chunk via USB
// //         usb.sendData(buffer, CHUNK_SIZE);
// //         // Advance offset for waveform continuity
// //         sample_offset += CHUNK_SAMPLES;
// //         // Keep offset within one period for correct waveform
// //         if (sample_offset > 1000000) sample_offset = 0; // Prevent overflow

// //         // Wait for the time it would take to acquire this chunk at 2 MSPS
// //         delayMicroseconds((CHUNK_SAMPLES * 1000000) / SAMPLE_RATE);

// //         unsigned long now = millis();
// //         if (now - lastSampleTime >= 1000) {
// //             controller.logStatus();
// //             lastSampleTime = now;
// //         }
// //     }
// // }