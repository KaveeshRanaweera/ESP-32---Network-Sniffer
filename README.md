# ESP32 WiFi & BLE Scanner with LCD

A portable scanner that detects WiFi networks and BLE devices, displaying information on an LCD with signal strength visualization.

## Features
- Scan and list nearby WiFi networks
- Scan and list BLE devices
- Visual signal strength indicators
- Detailed device information
- Simple 4-button navigation
- Automatic refresh every 10 seconds

## Hardware Requirements
- ESP32 Dev Board
- 16x2 I2C LCD (Address 0x27)
- 4x Tactile buttons
- Breadboard and jumper wires

## Pin Connections
| ESP32 Pin | Connection     |
|-----------|---------------|
| GPIO 32   | Button UP     |
| GPIO 33   | Button DOWN   |
| GPIO 25   | Button SELECT |
| GPIO 26   | Button BACK   |
| GPIO 21   | LCD SDA       |
| GPIO 22   | LCD SCL       |
| 3.3V      | LCD VCC       |
| GND       | LCD GND       |

## Installation
1. Connect hardware as per pin table
2. Install required libraries:
   ```bash
   pio lib install "liquidcrystal_i2c"
   pio lib install "ESP32 BLE Arduino"
