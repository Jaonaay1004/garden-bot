// ============================================================
//  PlantSense IoT  —  WebSocket Server
//  Deploy on Railway | github.com/yourname/plantsense-iot
// ============================================================
const express   = require("express");
const http      = require("http");
const WebSocket = require("ws");
const path      = require("path");

const app    = express();
const server = http.createServer(app);

// ── WebSocket Server ──────────────────────────────────────────
const wss = new WebSocket.Server({ server, path: "/ws" });

// เก็บ client แยกประเภท
const clients = {
  esp32:     new Set(),   // ESP32 ส่งข้อมูลมา
  dashboard: new Set(),   // Browser รับข้อมูล
};

// เก็บค่าล่าสุด (ส่งให้ client ใหม่ที่เพิ่งเชื่อมต่อ)
let latestSensorData = null;

wss.on("connection", (ws, req) => {
  // ตรวจ query param: ?type=esp32 หรือ ?type=dashboard
  const url    = new URL(req.url, "http://localhost");
  const type   = url.searchParams.get("type") || "dashboard";
  const token  = url.searchParams.get("token") || "";

  // ── ตรวจ token สำหรับ ESP32 (ป้องกัน spam) ──────────────
  const ESP32_TOKEN = process.env.ESP32_TOKEN || "plantsense2024";
  if (type === "esp32" && token !== ESP32_TOKEN) {
    console.log(`[WS] ESP32 rejected — invalid token`);
    ws.close(4001, "Invalid token");
    return;
  }

  clients[type]?.add(ws);
  console.log(`[WS] ${type.toUpperCase()} connected | total esp32=${clients.esp32.size} dash=${clients.dashboard.size}`);

  // ส่งค่าล่าสุดให้ dashboard ที่เพิ่งเชื่อมต่อ
  if (type === "dashboard" && latestSensorData) {
    ws.send(JSON.stringify({ type: "sensor", data: latestSensorData }));
  }

  // ส่ง ACK กลับไปให้รู้ว่าเชื่อมต่อสำเร็จ
  ws.send(JSON.stringify({ type: "connected", role: type }));

  ws.on("message", (raw) => {
    let msg;
    try { msg = JSON.parse(raw); }
    catch { console.warn("[WS] Invalid JSON:", raw.toString()); return; }

    if (type === "esp32" && msg.type === "sensor") {
      // บันทึกเวลาที่รับ
      msg.data.serverTs = new Date().toISOString();
      latestSensorData  = msg.data;

      // Broadcast ไปยัง dashboard ทุกตัว
      const payload = JSON.stringify({ type: "sensor", data: msg.data });
      for (const dash of clients.dashboard) {
        if (dash.readyState === WebSocket.OPEN) dash.send(payload);
      }
      console.log(`[Sensor] soil=${msg.data.soil?.toFixed(1)}% temp=${msg.data.temp?.toFixed(1)}°C hum=${msg.data.hum?.toFixed(1)}% light=${msg.data.light?.toFixed(1)}%`);
    }

    // Ping-Pong keepalive
    if (msg.type === "ping") {
      ws.send(JSON.stringify({ type: "pong", ts: Date.now() }));
    }
  });

  ws.on("close", () => {
    clients[type]?.delete(ws);
    console.log(`[WS] ${type.toUpperCase()} disconnected | total esp32=${clients.esp32.size} dash=${clients.dashboard.size}`);
  });

  ws.on("error", (err) => {
    console.error(`[WS] Error (${type}):`, err.message);
    clients[type]?.delete(ws);
  });
});

// ── HTTP POST endpoint สำหรับ ESP32 ─────────────────────────
app.use(express.json());

app.post("/api/data", (req, res) => {
  const token = req.headers["x-token"] || req.query.token || "";
  const ESP32_TOKEN = process.env.ESP32_TOKEN || "plantsense2024";

  if (token !== ESP32_TOKEN) {
    return res.status(401).json({ error: "Invalid token" });
  }

  const data = req.body;
  if (!data || typeof data.temp === "undefined") {
    return res.status(400).json({ error: "Invalid data" });
  }

  data.serverTs = new Date().toISOString();
  latestSensorData = data;

  // Broadcast ไปยัง dashboard ทุกตัว
  const payload = JSON.stringify({ type: "sensor", data });
  for (const dash of clients.dashboard) {
    if (dash.readyState === WebSocket.OPEN) dash.send(payload);
  }

  console.log(`[POST] soil=${data.soil}% temp=${data.temp}°C hum=${data.hum}% light=${data.light}%`);
  res.json({ ok: true, ts: data.serverTs });
});

app.get("/api/health", (_, res) => res.json({
  status:  "ok",
  uptime:  process.uptime(),
  esp32:   clients.esp32.size,
  dashboards: clients.dashboard.size,
  lastSensor: latestSensorData ? { ts: latestSensorData.serverTs } : null,
}));

app.get("/api/latest", (_, res) => {
  if (!latestSensorData) return res.status(204).end();
  res.json(latestSensorData);
});

// ── Serve Static Dashboard ────────────────────────────────────
app.use(express.static(path.join(__dirname, "public")));
app.get("*", (_, res) => res.sendFile(path.join(__dirname, "public", "index.html")));

// ── Start Server ──────────────────────────────────────────────
const PORT = process.env.PORT || 3000;
  server.listen(PORT, () => {
    console.log(`
    ╔══════════════════════════════════════════╗
    ║   🌱  PlantSense IoT Server Started     ║
    ║   Port  : ${PORT.toString().padEnd(32)}║
    ║   WS    : ws://localhost:${PORT}/ws           ║
    ╚══════════════════════════════════════════╝
    ESP32_TOKEN = ${process.env.ESP32_TOKEN || "plantsense2024 (default)"}`);
  });
