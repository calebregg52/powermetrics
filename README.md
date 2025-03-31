# ⚡ Powermetrics

A real-time voltage and current monitoring web application powered by an ESP32 and dual I2C ADCs. This dashboard visualizes voltage and current data, allows WiFi and channel configuration, and logs data via WebSocket.

## 📦 Project Overview

This project includes:
- **ESP32 Firmware** (`main.cpp`) that reads ADC values and hosts a web server/websocket.
- **Web Interface** (`index.html`, `style.css`, `script.js`) for live monitoring, configuration, and data visualization.
- **WebSocket Communication** for real-time data updates.
- **Chart.js**-powered graphs.
- **Dark mode toggle** and **trigger-based recording**.

## 🚀 Getting Started

### 📁 File Structure

```
powermetrics/
├── main.cpp            # ESP32 Firmware (Arduino)
├── index.html          # Frontend HTML
├── style.css           # Custom styling
├── script.js           # Chart logic & WebSocket handling
├── logo.png            # Optional logo for branding
```

## 🔌 Hardware Setup

- **ESP32 Development Board**
- **2x MCP3426 ADCs** over I2C
- **Wiring**:
  - `SDA` → GPIO 21
  - `SCL` → GPIO 22
- **Voltage Divider** circuit for proper scaling
- **Current Sensors**

## 🧠 ESP32 Firmware (main.cpp)

The firmware performs:
- Periodic reads of ADC channels
- JSON formatting of readings
- WebSocket broadcasting to frontend
- WiFiManager setup for captive portal fallback
- Optional support for WPA2-Enterprise WiFi

### 🔧 Configure WiFi on First Boot

```cpp
WiFiManager wifiManager;
wifiManager.autoConnect("ESP32-Setup");
```

### 📡 WebSocket Broadcast Snippet

```cpp
String jsonResponse = buildJsonResponse(csvLine);
webSocket.broadcastTXT(jsonResponse);
```

## 🖥️ Frontend Features

### 🌐 Live Dashboard

Powered by Bootstrap + Chart.js:

```html
<canvas id="voltageGraph"></canvas>
<canvas id="currentGraph"></canvas>
```

### 📊 Real-Time Charts

```javascript
new Chart(ctx, {
  type: 'line',
  data: { labels, datasets },
  options: getChartOptions()
});
```

### 💡 Dark Mode Toggle

```javascript
document.getElementById('darkModeToggle').addEventListener('change', () => {
  document.body.classList.toggle('dark-mode');
});
```

### 📡 WebSocket Live Feed

```javascript
const socket = new WebSocket(`ws://${window.location.hostname}:81/`);
socket.onmessage = (event) => {
  const data = JSON.parse(event.data);
  // Update gauges and charts
};
```

## ⚙️ Channel & WiFi Configuration

### ADC Channel Toggle

```javascript
fetch('/get-channels')
  .then(response => response.json())
  .then(channels => {
    // Dynamically render channel switches
  });
```

### WPA2-Enterprise Support

```json
{
  "useEnterprise": true,
  "enterpriseSsid": "EDU-SSID",
  "enterpriseUsername": "user@school.edu",
  "enterprisePassword": "password123"
}
```

## 📁 Data Logging

- CSV logs are saved to SPIFFS (`/adc_log.csv`)
- Download via browser with the **Download Log** button

```cpp
logToCsv(csvLine);
```

## 🛠️ Build & Flash (Arduino / PlatformIO)

### 🧰 Required Libraries
- `WiFiManager`
- `WebSocketsServer`
- `SPIFFS`
- `Wire`

### 🧪 PlatformIO Build Command

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    bblanchon/ArduinoJson
    tzapu/WiFiManager
```
## 👨‍💻 Authors

- Caleb Regg  
- Cade Schapel  
- Logan Howard
