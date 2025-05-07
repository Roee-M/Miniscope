# Miniscope

**Miniscope** is a low-cost, handheld oscilloscope designed for embedded electronics diagnostics and signal analysis. It is powered by an ESP32-S3-WROOM-1-N16R8 microcontroller and features a custom-designed analog frontend using an external high-speed ADC, all integrated into a compact, portable device.

---

## 🔧 Features

- 📟 **Real-time signal visualization** on Android via USB-C
- 🎯 **Trigger support**: Rising edge, falling edge, and level-based
- ⚡ **Sampling rate**: 1–3 MHz (12-bit resolution)
- 🔌 **Direct USB-C communication** with Android devices
- 📦 **Custom PCB design** with external ADC and analog frontend
- 🧠 **Dual-core ESP32** for optimized data acquisition and transmission
- 🧰 **Data logging** and human-readable exports

---

## 📐 Hardware Overview

| Component               | Description                                  |
|------------------------|----------------------------------------------|
| MCU                    | ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM) |
| ADC                    |    |
| Analog Frontend        | Custom-designed: buffering, scaling, filtering |
| Power                  | USB-C powered (5V input, onboard regulators) |
| Display                | Android app (USB-C connected)                |
| PCB                    | Custom 2-layer design                        |

---
