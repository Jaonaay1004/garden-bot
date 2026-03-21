# 🌱 PlantSense IoT Dashboard

ระบบ IoT ตรวจวัดสภาพแวดล้อมต้นไม้แบบ Real-time  
**ESP32** → **WebSocket** → **Railway Server** → **Browser Dashboard**

---

## 📁 โครงสร้างไฟล์

```
plantsense-iot/
├── server.js              ← Node.js WebSocket + Express Server
├── package.json
├── .gitignore
├── public/
│   └── index.html         ← Dashboard Frontend
└── arduino/
    └── plant_monitor.ino  ← ESP32 Firmware
```

---

## 🚀 Deploy บน Railway

### ขั้นตอน 1 — Push ขึ้น GitHub
```bash
git init
git add .
git commit -m "Initial PlantSense IoT"
git remote add origin https://github.com/YOUR_USERNAME/plantsense-iot.git
git push -u origin main
```

### ขั้นตอน 2 — สร้าง Project บน Railway
1. ไปที่ [railway.app](https://railway.app) → **New Project**
2. เลือก **Deploy from GitHub repo**
3. เลือก repo `plantsense-iot`
4. Railway จะ detect `package.json` และ deploy อัตโนมัติ

### ขั้นตอน 3 — ตั้ง Environment Variables (optional)
ใน Railway Dashboard → **Variables**:
```
ESP32_TOKEN=your_secret_token_here
```
> ถ้าไม่ตั้ง จะใช้ค่า default `plantsense2024`

### ขั้นตอน 4 — เปิด Public URL
Railway → **Settings** → **Networking** → **Generate Domain**  
จะได้ URL เช่น: `https://plantsense-iot-production.up.railway.app`

---

## 🔧 ตั้งค่า Arduino (ESP32)

เปิดไฟล์ `arduino/plant_monitor.ino` แก้ไขส่วนนี้:

```cpp
#define WIFI_SSID   "ชื่อ WiFi ของคุณ"
#define WIFI_PASS   "รหัส WiFi ของคุณ"
#define SERVER_HOST "your-app.railway.app"   // ← ใส่ URL จาก Railway
#define USE_SSL     true                      // Railway ใช้ HTTPS → true
```

### Libraries ที่ต้องติดตั้ง (Arduino Library Manager)
| Library | Author |
|---------|--------|
| ArduinoWebsockets | Gil Maimon |
| ArduinoJson | Benoit Blanchon |
| DHT sensor library | Adafruit |
| Adafruit Unified Sensor | Adafruit |

---

## 📡 วงจร ESP32

| Sensor | Pin |
|--------|-----|
| Capacitive Soil Moisture | GPIO 34 |
| DHT11 Data | GPIO 25 |
| LDR (แสง) | GPIO 32 |

---

## 🌐 WebSocket Protocol

ESP32 เชื่อมต่อที่:
```
wss://your-app.railway.app/ws?type=esp32&token=plantsense2024
```

JSON ที่ ESP32 ส่ง:
```json
{
  "type": "sensor",
  "data": {
    "soil": 55.0,
    "soilRaw": 1800,
    "soilState": "Moist",
    "temp": 28.5,
    "hum": 65.2,
    "tempState": "Normal",
    "light": 70.0,
    "lightRaw": 1200,
    "lightState": "Bright",
    "uptime": 3600
  }
}
```

---

## 🔍 API Endpoints

| Endpoint | Description |
|----------|-------------|
| `GET /` | Dashboard UI |
| `GET /api/health` | Server status + client count |
| `GET /api/latest` | ค่าเซนเซอร์ล่าสุด (JSON) |
| `WS /ws?type=dashboard` | Browser WebSocket |
| `WS /ws?type=esp32&token=...` | ESP32 WebSocket |
