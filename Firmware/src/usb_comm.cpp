#include "usb_comm.hpp"

USBComm::USBComm() {}

void USBComm::begin() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    Serial.println("[USBComm] USB CDC Ready.");
}

void USBComm::sendData(const uint8_t* data, size_t len) {
    Serial.write(data, len);
}

void USBComm::sendSample(uint16_t sample) {
    uint8_t payload[2];
    payload[0] = sample & 0xFF;
    payload[1] = (sample >> 8) & 0x0F;
    sendData(payload, sizeof(payload));
}


