# Local BLE Edge Gateway Architecture

An offline-first, highly resilient IoT Smart Home monitoring system. This project bypasses local ISP router instability by using a **Bluetooth Low Energy (BLE) Star Topology** to transmit environmental and audio data to a central, hardwired Linux Edge Gateway.

## 🚀 Project Overview
Instead of relying on fragile Wi-Fi connections for microcontrollers, the system aggregates data via BLE to a central Linux single-board computer (Radxa/Raspberry Pi). The Gateway stores data locally in SQLite and serves it via a REST API to a containerized, responsive web dashboard.

### Key Features
* **100% Offline Capable:** Does not require an active internet connection to function.
* **Non-Blocking Audio Processing:** Uses I2S hardware and custom algorithms to calculate room noise levels without crashing the microcontroller.
* **Asynchronous BLE Handling:** The Python backend concurrently maintains multiple BLE connections without dropping packets.
* **Live Web Dashboard:** Features live polling and dynamic historical charting using Chart.js.

---

## 🛠️ System Architecture

### 1. The Edge Nodes (Hardware / C++)
The hardware workload is distributed across two separate ESP32 microcontrollers to ensure stable digital signal processing (DSP):
* **Node 1: Climate (ESP32-C3):** 
  * Reads an AHT10 sensor (I2C) for Temperature and Humidity.
  * Reads an analog LDR sensor for Light levels.
  * Broadcasts data via a BLE Notify Characteristic every 5 seconds.
* **Node 2: Audio & Control (ESP32-32D):** 
  * Runs a continuous, non-blocking I2S stream reading an INMP441 MEMS microphone.
  * Calculates the Absolute Average (RMS alternative) of the sound waves to prevent memory overflow.
  * Maps the highest volume peak over a 2-second window to a 0-100% scale.
  * Features a BLE Write Characteristic to toggle the onboard LED (`LED_BUILTIN`).

### 2. The Edge Gateway (Backend / Python)
A Radxa Linux board serves as the brain, running two `systemd` managed asynchronous Python scripts:
* **BLE Manager (`bleak`):** Uses `asyncio.gather` to maintain constant connections with both ESP32 nodes. It reads the incoming BLE strings and writes them directly into a local SQLite database (`sensors.db`). It also checks the DB every second for pending LED commands and pushes them to the ESP32.
* **REST API (`Flask`):** Runs on port `9001` and acts as a bridge between the SQLite database and the Web UI.

### 3. The Web Dashboard (Frontend / HTML & JS)
A responsive, dark-mode web application served via an Nginx Docker container on port `8080`.
* **Standard HTTP Polling:** Avoids MQTT/WebSockets entirely for ultimate stability. Uses native JS `fetch()` to hit the API every 5 seconds.
* **Data Visualization:** Uses `Chart.js` to render four distinct, live-updating line charts for historical trends (Temperature, Humidity, Light, and Noise).
* **Control:** Interactive UI buttons send `POST` requests to the API to trigger physical hardware changes.

---

## ⚙️ Setup & Deployment Instructions

### Hardware Nodes
1. Open the `.ino` files in `hardware_nodes/` using the Arduino IDE.
2. Install required libraries: `Adafruit AHTX0`, `ESP32 BLE Arduino`.
3. Flash the Climate code to the ESP32-C3 and the Audio code to the ESP32-32D.

### Edge Gateway Backend (Linux)
1. Install Python dependencies: `pip install -r gateway_backend/requirements.txt`
2. Update the `DB_PATH` in both Python scripts to reflect the absolute path on your specific machine (e.g., `/home/user/gateway_backend/sensors.db`).
3. Run the scripts as system services or manually:
   ```bash
   python3 ble_reader.py &
   python3 api_server.py &