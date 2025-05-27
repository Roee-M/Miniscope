#ifndef USB_COMM_HPP
#define USB_COMM_HPP

#include <Arduino.h>

class USBComm {
public:
    USBComm();
    void begin();
    void sendData(const uint8_t* data, size_t len);
    void sendSample(uint16_t sample);

};

#endif // USB_COMM_HPP
