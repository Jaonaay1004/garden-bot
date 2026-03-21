// ============================================================
//  PlantSense IoT — ESP32 Firmware
//  ส่งข้อมูลเซนเซอร์ผ่าน WebSocket → Railway Server
//
//  Libraries needed (Arduino Library Manager):
//    - ArduinoWebsockets  by Gil Maimon
//    - ArduinoJson        by Benoit Blanchon
//    - DHT sensor library by Adafruit
//    - Adafruit Unified Sensor
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <DHT.h>

using namespace websockets;

// ================================================================
//  ⚙️  ตั้งค่าตรงนี้ก่อน Upload
// ================================================================

// ── WiFi ────────────────────────────────────────────────────────
#define WIFI_SSID   "Test123"
#define WIFI_PASS   "12345678"

// ── Railway Server ───────────────────────────────────────────────
//  เปลี่ยน your-app.railway.app เป็น URL จริงจาก Railway
#define SERVER_HOST   "your-app.railway.app"
#define SERVER_PATH   "/ws?type=esp32&token=plantsense2024"
//  ถ้า Railway ให้ https → ใช้ WSS (port 443)
#define USE_SSL       true
#define SERVER_PORT   443

// ── Sensor Pins ──────────────────────────────────────────────────
#define SOIL_PIN  34
#define DHTPIN    25
#define LDR_PIN   32
#define DHTTYPE   DHT11

// ── Soil Calibration (ปรับให้ตรงกับ sensor จริง) ─────────────────
const int VERY_DRY = 2910;   // ค่า ADC ตอนดินแห้ง
const int VERY_WET = 925;    // ค่า ADC ตอนดินเปียก

// ── LDR Calibration ──────────────────────────────────────────────
const int LDR_DARK   = 4095;
const int LDR_BRIGHT = 0;

// ── Interval ─────────────────────────────────────────────────────
const unsigned long SEND_INTERVAL_MS = 4000;   // ส่งทุก 4 วินาที

// ================================================================
//  Globals
// ================================================================
DHT dht(DHTPIN, DHTTYPE);
WebsocketsClient wsClient;

bool      wsConnected  = false;
uint32_t  lastSend     = 0;
uint32_t  lastPing     = 0;
uint32_t  lastReconnect= 0;
String    lastSoilState = "";

// ================================================================
//  WiFi
// ================================================================
void setupWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Failed — rebooting in 5s...");
    delay(5000);
    ESP.restart();
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost connection, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
      delay(500);
      retry++;
    }
  }
}

// ================================================================
//  WebSocket Callbacks
// ================================================================
void onWSMessage(WebsocketsMessage msg) {
  Serial.println("[WS] Server: " + msg.data());
}

void onWSEvent(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    wsConnected = true;
    Serial.println("[WS] ✅ Connected to server!");
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    wsConnected = false;
    Serial.println("[WS] ❌ Disconnected from server");
  } else if (event == WebsocketsEvent::GotPing) {
    wsClient.pong();
  }
}

// ================================================================
//  WebSocket Connect
// ================================================================
void connectWebSocket() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("[WS] Connecting to ");
  Serial.println(String(USE_SSL ? "wss" : "ws") + "://" + SERVER_HOST + SERVER_PATH);

  wsClient.onMessage(onWSMessage);
  wsClient.onEvent(onWSEvent);

  bool ok;
  if (USE_SSL) {
    // Railway ใช้ HTTPS → WSS
    wsClient.setInsecure();   // ข้าม cert check (ปลอดภัยพอสำหรับ IoT)
    ok = wsClient.connectSSL(SERVER_HOST, SERVER_PORT, SERVER_PATH);
  } else {
    ok = wsClient.connect(SERVER_HOST, SERVER_PORT, SERVER_PATH);
  }

  if (ok) {
    Serial.println("[WS] Handshake OK");
  } else {
    Serial.println("[WS] Connection failed — will retry...");
  }
}

// ================================================================
//  Read Sensors & Build JSON
// ================================================================
String buildSensorJSON() {
  // ── Soil ──
  int soilRaw  = analogRead(SOIL_PIN);
  int soilPct  = map(soilRaw, VERY_WET, VERY_DRY, 100, 0);
      soilPct  = constrain(soilPct, 0, 100);
  String soilState;
  if (soilRaw > 2600)      soilState = "Dry";
  else if (soilRaw > 1500) soilState = "Moist";
  else                     soilState = "Wet";

  // ── LDR ──
  int ldrRaw   = analogRead(LDR_PIN);
  int ldrPct   = map(ldrRaw, LDR_DARK, LDR_BRIGHT, 0, 100);
      ldrPct   = constrain(ldrPct, 0, 100);
  String lightState;
  if (ldrPct < 30)      lightState = "Dark";
  else if (ldrPct < 70) lightState = "Dim";
  else                  lightState = "Bright";

  // ── DHT11 ──
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  if (isnan(temp) || isnan(hum)) {
    Serial.println("[DHT] Read error!");
    return "";
  }
  String tempState;
  if (temp < 20)      tempState = "Cold";
  else if (temp < 35) tempState = "Normal";
  else                tempState = "Hot";

  // ── Serial Monitor ──
  Serial.printf(
    "[Sensor] Soil: %d (%d%%) [%s] | Light: %d (%d%%) [%s] | Temp: %.2f°C [%s] | Hum: %.2f%%\n",
    soilRaw, soilPct, soilState.c_str(),
    ldrRaw, ldrPct, lightState.c_str(),
    temp, tempState.c_str(), hum
  );

  // ── JSON Payload ──
  StaticJsonDocument<256> doc;
  doc["type"]       = "sensor";
  JsonObject data   = doc.createNestedObject("data");
  data["soil"]      = soilPct;
  data["soilRaw"]   = soilRaw;
  data["soilState"] = soilState;
  data["temp"]      = round(temp * 100) / 100.0;
  data["hum"]       = round(hum  * 100) / 100.0;
  data["tempState"] = tempState;
  data["light"]     = ldrPct;
  data["lightRaw"]  = ldrRaw;
  data["lightState"]= lightState;
  data["uptime"]    = millis() / 1000;

  String out;
  serializeJson(doc, out);
  return out;
}

// ================================================================
//  Setup
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== PlantSense IoT v2.0 ===");

  pinMode(SOIL_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  dht.begin();
  delay(500);

  setupWiFi();
  connectWebSocket();
}

// ================================================================
//  Loop
// ================================================================
void loop() {
  // ── WiFi keepalive ──────────────────────────────────────────
  checkWiFi();

  // ── WS poll (รับ/ส่ง messages) ─────────────────────────────
  if (wsConnected) {
    wsClient.poll();
  }

  uint32_t now = millis();

  // ── Reconnect ───────────────────────────────────────────────
  if (!wsConnected && now - lastReconnect > 8000) {
    lastReconnect = now;
    Serial.println("[WS] Attempting reconnect...");
    connectWebSocket();
  }

  // ── Send sensor data ────────────────────────────────────────
  if (wsConnected && now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;
    String payload = buildSensorJSON();
    if (payload.length() > 0) {
      bool ok = wsClient.send(payload);
      Serial.println(ok ? "[WS] ✅ Sent" : "[WS] ❌ Send failed");
    }
  }

  // ── Ping keepalive (ทุก 30s) ────────────────────────────────
  if (wsConnected && now - lastPing > 30000) {
    lastPing = now;
    StaticJsonDocument<32> ping;
    ping["type"] = "ping";
    String s;
    serializeJson(ping, s);
    wsClient.send(s);
  }
}
