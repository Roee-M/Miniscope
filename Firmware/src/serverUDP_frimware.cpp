// this is the las working firmware to handle the UDP server communication with the app.py server - 15/07/2025


// #include <Arduino.h>
// #include <WiFi.h>
// #include <WiFiUdp.h>

// const char* ssid = "MINISCOPE_ESP32";
// const char* password = "12345678";

// WiFiUDP udp;
// const int localPort = 4210;

// IPAddress remoteIP;
// uint16_t remotePort;
// bool clientKnown = false;

// #define BUFFER_SIZE (1 * 1000 * 1000 * 2)  // 6 MB for 3MSPS * 2 bytes/sample
// // #define BUFFER_SIZE (1 * 1000 * 2)  // 6 MB for 3MSPS * 2 bytes/sample
// uint8_t* adcBuffer = nullptr;

// bool triggered = false;
// bool dataSent = false;

// void fillBuffer() {
//   Serial.println("Filling buffer with simulated data...");
//   for (size_t i = 0; i < BUFFER_SIZE; i += 2) {
//     uint16_t sample = (i / 2) % 4096;  // ramp 0–4095
//     adcBuffer[i] = sample & 0xFF;        // LSB
//     adcBuffer[i+1] = (sample >> 8) & 0xFF; // MSB
//   }
//   Serial.println("Buffer filled");
// }

// void setup() {
//   Serial.begin(115200);
//   delay(1000);
//   Serial.println("Starting MiniScope UDP Server");

//   adcBuffer = (uint8_t*)ps_malloc(BUFFER_SIZE);
//   if (!adcBuffer) {
//     Serial.println("Failed to allocate buffer in PSRAM!");
//     while (true) delay(1000);
//   }
//   Serial.println("Buffer allocated in PSRAM");

//   WiFi.softAP(ssid, password);
//   IPAddress IP = WiFi.softAPIP();
//   Serial.print("Access Point IP address: ");
//   Serial.println(IP);

//   udp.begin(localPort);
//   Serial.printf("UDP server listening on port %d\n", localPort);
// }

// void loop() {
//   int packetSize = udp.parsePacket();
//   if (packetSize > 0) {
//     char incoming[packetSize + 1];
//     udp.read(incoming, packetSize);
//     incoming[packetSize] = '\0';

//     remoteIP = udp.remoteIP();
//     remotePort = udp.remotePort();
//     clientKnown = true;

//     Serial.printf("Received command: '%s' from %s:%d\n", incoming, remoteIP.toString().c_str(), remotePort);

//     if (strcmp(incoming, "t") == 0 && !triggered){ //Trigger
//       Serial.println("Trigger activated - filling buffer");
//       fillBuffer();
//       triggered = true;
//       dataSent = false;
//       Serial.println("Trigger activated - ready to send data");
//     }
//     else if (strcmp(incoming, "r") == 0) { // Reset
//       triggered = false;
//       dataSent = false;
//       Serial.println("Reset command received - ready for new trigger");
//     }
//     else{

//     }
//   }

//   if (clientKnown && triggered && !dataSent) {
//     const size_t chunkSize = 1400;
//     size_t sent = 0;

//     Serial.println("Sending data buffer over UDP...");
//     while (sent < BUFFER_SIZE) {
//       Serial.print(".");
//       size_t toSend = min(chunkSize, BUFFER_SIZE - sent);
//       udp.beginPacket(remoteIP, remotePort);
//       udp.write(adcBuffer + sent, toSend);
//       udp.endPacket();
//       sent += toSend;
//       delayMicroseconds(200);  // tweak as needed
//     }
//     Serial.printf("Finished sending %u bytes over UDP\n", BUFFER_SIZE);
//     dataSent = true;
//   }
// }



// // TODO
// // Other ESP:
// //   Connect LB
//   //? read_ADC() (-> API, implement as actual ADC SPI reads later)
//   //? store in ESP memory - fill out buffer
//   //? Add trigger functionality
//   //? send over Wifi/USB/mail
//   //? simple server to plot incoming data

// // Miniscope itself:  
//   //? implement as actual ADC SPI reads
//   //? Print the reads (Serial)

// // Frontend:
//   //? Upgrade debug server to be able to display and plot the data over the phone/web
//   //? Add trigger functionality via phone