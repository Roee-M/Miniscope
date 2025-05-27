#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <Arduino.h>

class Controller {
public:
    Controller();
    void begin();
    void handleInput();
    bool isRunning() const;
    void logStatus();

private:
    bool running;
    unsigned long lastLogTime;
    uint32_t sampleCount;
    unsigned long startTime;
};

#endif // CONTROLLER_HPP
