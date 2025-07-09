#ifndef WIFI_SENDER_H
#define WIFI_SENDER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

class WifiSender {
public:
  void begin(const char* ssid, const char* password);
  void sendBuffer(const uint16_t* buffer, size_t size);

private:
  WiFiServer _server = WiFiServer(80);
};

#endif
