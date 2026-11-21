#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>

const char* apSSID     = "ESP32-Lidar-AP";
const char* apPassword = "folklore"; // must be 8+ characters, or the AP will be open

HardwareSerial lidarSerial(2);
#define RXD2 16   // ESP32 RX2  <- TF-Luna pin 3 (TXD)
#define TXD2 17   // ESP32 TX2  -> TF-Luna pin 2 (RXD)

WebServer server(80);

uint16_t latestDistance = 0;   // cm
uint16_t latestStrength = 0;
float    latestTemp     = 0;   // deg C
bool     latestValid    = false;

const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>TF-Luna Direct AP</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; text-align: center; margin: 40px 10px; background-color: #0f172a; color: #f8fafc; }
    .card { background: #1e293b; padding: 25px 20px; border-radius: 16px; display: inline-block; box-shadow: 0 10px 25px rgba(0,0,0,0.4); width: 90%; max-width: 380px; }
    h1 { font-size: 1.25rem; color: #38bdf8; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 20px; }
    .metric { font-size: 3rem; font-weight: 800; margin: 10px 0; color: #4ade80; }
    .metric.weak { color: #f87171; }
    .grid { display: flex; justify-content: space-around; margin-top: 20px; padding-top: 15px; border-top: 1px solid #334155; font-size: 0.9rem; color: #94a3b8; }
    .badge { background: #0f172a; padding: 6px 12px; border-radius: 8px; font-weight: 600; color: #e2e8f0; }
  </style>
  <script>
    setInterval(function() {
      fetch('/data').then(res => res.json()).then(data => {
        var d = document.getElementById('dist');
        if (data.valid) {
          d.innerText = data.distance + " cm";
          d.className = "metric";
        } else {
          d.innerText = "-- cm";
          d.className = "metric weak";
        }
        document.getElementById('str').innerText = data.strength;
        document.getElementById('tmp').innerText = data.temp.toFixed(1) + " °C";
      }).catch(e => {});
    }, 200);
  </script>
</head>
<body>
  <div class="card">
    <h1>TF-Luna Live Telemetry</h1>
    <div class="metric" id="dist">-- cm</div>
    <div class="grid">
      <div>Signal<br><span class="badge" id="str">--</span></div>
      <div>Chip Temp<br><span class="badge" id="tmp">-- °C</span></div>
    </div>
  </div>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
}

void handleData() {
  String json = "{";
  json += "\"distance\":" + String(latestDistance) + ",";
  json += "\"strength\":" + String(latestStrength) + ",";
  json += "\"temp\":"     + String(latestTemp, 1)  + ",";
  json += "\"valid\":"    + String(latestValid ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// Read every complete frame waiting in the UART buffer and keep the newest.
// Frame: 0x59 0x59 distL distH strL strH tempL tempH checksum
void readLidar() {
  while (lidarSerial.available() >= 9) {
    if (lidarSerial.peek() != 0x59) {   // resync on header
      lidarSerial.read();
      continue;
    }

    uint8_t f[9];
    lidarSerial.readBytes(f, 9);
    if (f[1] != 0x59) continue;

    uint8_t sum = 0;
    for (int i = 0; i < 8; i++) sum += f[i];
    if (sum != f[8]) continue;           // bad checksum, drop frame

    latestDistance = f[2] | (f[3] << 8);
    latestStrength = f[4] | (f[5] << 8);
    latestTemp     = (int16_t)(f[6] | (f[7] << 8)) / 100.0f;
    latestValid    = (latestStrength >= 100 && latestStrength != 65535);
  }
}

void setup() {
  Serial.begin(115200);
  lidarSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);

  WiFi.mode(WIFI_AP);
  // SSID, password, channel, hidden, max connections
  WiFi.softAP(apSSID, apPassword, 1, 0, 4);

  Serial.println("\n--- Wi-Fi Access Point Active (Secured) ---");
  Serial.print("SSID: ");     Serial.println(apSSID);
  Serial.print("Password: "); Serial.println(apPassword);
  Serial.print("Gateway IP: http://"); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  readLidar();
  server.handleClient();
  readLidar();   // drain again in case handleClient() took a while
}
