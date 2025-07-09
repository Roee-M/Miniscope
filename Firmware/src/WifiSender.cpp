#include "WifiSender.h"

void WifiSender::begin(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  _server.begin();
}

void WifiSender::sendBuffer(const uint16_t* buffer, size_t size) {
  WiFiClient client = _server.available();
  if (client) {
    Serial.println("Client connected. Sending data...");
    for (size_t i = 0; i < size; ++i) {
      client.println(buffer[i]);
    }
    client.stop();
    Serial.println("Client disconnected.");
  }
}
