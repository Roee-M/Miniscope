#include "controller.hpp"

Controller::Controller() : running(false), lastLogTime(0), sampleCount(0), startTime(0) {}

void Controller::begin() {
    Serial.println("[Controller] Ready for commands (START/STOP/STAT).");
}

void Controller::handleInput() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "START") {
            running = true;
            sampleCount = 0;
            startTime = millis();
            Serial.println("[Controller] Acquisition started.");
        } else if (cmd == "STOP") {
            running = false;
            Serial.println("[Controller] Acquisition stopped.");
        } else if (cmd == "STAT") {
            logStatus();
        } else {
            Serial.print("[Controller] Unknown command: ");
            Serial.println(cmd);
        }
    }
}

bool Controller::isRunning() const {
    return running;
}

void Controller::logStatus() {
    unsigned long now = millis();
    float elapsed = (now - startTime) / 1000.0f;
    float rate = elapsed > 0 ? sampleCount / elapsed : 0;
    Serial.printf("[Controller] Elapsed: %.2fs, Samples: %lu, Rate: %.2f Hz\n", elapsed, sampleCount, rate);
}
