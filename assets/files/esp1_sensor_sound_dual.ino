// ESP32 #1: TF-Luna lidar + DFPlayer Pro sound + web page.
// No lights, no motor, no high voltage. ESP32 #2 (motor + lights) joins this
// board's Wi-Fi and polls /data.
//
// /data  -> {"distance":..,"strength":..,"temp":..,"valid":..,"near":..,
//            "trig":N,"jog":N,"jogdir":"up|down|none","jogmotor":0|1|2}
//   trig   : counts up on every trigger (ESP #2 starts the ghost drop when it changes)
//   near   : true while someone is within range (ESP #2 flickers the lights)
//   jog    : counts up on every Up/Down button press (ESP #2 moves the motor a little)
//   jogdir : direction of the last button press
//   jogmotor: which motor that press was for (1, 2, or 0 = both)

#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include <DFRobot_DF1201S.h>   // DFPlayer Pro library

// ---------- Settings you can change ----------
const char* apSSID     = "ESP32-Lidar-AP";
const char* apPassword = "folklore";        // 8+ characters

const int      TRIGGER_CM    = 100;   // trigger when closer than this
const int      RELEASE_CM    = 130;   // re-arm after going farther than this
const uint32_t COOLDOWN_MS   = 3000;  // minimum time between triggers

const uint16_t TRACK_NUMBER  = 1;     // first file on the DFPlayer Pro
const uint8_t  VOLUME        = 15;    // 0 - 30 (25 was very loud on a speaker)
// ---------------------------------------------

// Pins
#define LIDAR_RX 16   // ESP32 RX2 <- TF-Luna pin 3 (TXD)
#define LIDAR_TX 17   // ESP32 TX2 -> TF-Luna pin 2 (RXD)
#define DF_RX    25   // ESP32 RX  <- DFPlayer Pro TX
#define DF_TX    26   // ESP32 TX  -> DFPlayer Pro RX

HardwareSerial lidarSerial(2);
HardwareSerial dfSerial(1);
DFRobot_DF1201S dfp;
WebServer server(80);

bool     dfReady = false;
uint16_t latestDistance = 0;
uint16_t latestStrength = 0;
float    latestTemp     = 0;
bool     latestValid    = false;

bool     armed          = true;
bool     nearState      = false;   // true while someone is within range
uint32_t lastTrigger    = 0;
uint32_t triggerCount   = 0;

uint32_t jogCount       = 0;       // increments on every Up/Down press
const char* jogDir      = "none";  // "up" or "down"
int         jogMotor    = 0;       // 0 = both, 1 = ghost 1, 2 = ghost 2

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
    .jog { margin-top: 22px; padding-top: 15px; border-top: 1px solid #334155; font-size: 0.9rem; color: #94a3b8; }
    .jog .row { display: flex; gap: 12px; margin-top: 8px; }
    .jog .lbl { margin-top: 14px; font-weight: 700; color: #e2e8f0; }
    .jog button { flex: 1; padding: 18px 0; border: 0; border-radius: 12px; background: #38bdf8; color: #0f172a; font-size: 1.1rem; font-weight: 800; }
    .jog button:active { background: #0ea5e9; transform: scale(0.98); }
    #jogmsg { margin-top: 10px; min-height: 1.2em; }
  </style>
  <script>
    function jog(m, dir) {
      fetch('/jog?m=' + m + '&dir=' + dir).then(function(r) {
        document.getElementById('jogmsg').innerText = 'Sent: ' + (m == 0 ? 'both' : 'ghost ' + m) + ' ' + dir;
      }).catch(function() {
        document.getElementById('jogmsg').innerText = 'Failed to send';
      });
    }
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
    <div class="jog">
      Ghost motors (small manual move)
      <div class="lbl">Ghost 1</div>
      <div class="row">
        <button onclick="jog(1,'up')">&#9650; Up</button>
        <button onclick="jog(1,'down')">&#9660; Down</button>
      </div>
      <div class="lbl">Ghost 2</div>
      <div class="row">
        <button onclick="jog(2,'up')">&#9650; Up</button>
        <button onclick="jog(2,'down')">&#9660; Down</button>
      </div>
      <div class="lbl">Both together</div>
      <div class="row">
        <button onclick="jog(0,'up')">&#9650; Up</button>
        <button onclick="jog(0,'down')">&#9660; Down</button>
      </div>
      <div id="jogmsg"></div>
    </div>
  </div>
</body>
</html>
)rawliteral";

void handleRoot() { server.send_P(200, "text/html", MAIN_page); }

void handleData() {
  String json = "{";
  json += "\"distance\":" + String(latestDistance) + ",";
  json += "\"strength\":" + String(latestStrength) + ",";
  json += "\"temp\":"     + String(latestTemp, 1)  + ",";
  json += "\"valid\":"    + String(latestValid ? "true" : "false") + ",";
  json += "\"near\":"     + String(nearState ? "true" : "false") + ",";
  json += "\"trig\":"     + String(triggerCount) + ",";
  json += "\"jog\":"      + String(jogCount) + ",";
  json += "\"jogdir\":\"" + String(jogDir) + "\",";
  json += "\"jogmotor\":" + String(jogMotor);
  json += "}";
  server.send(200, "application/json", json);
}

// /jog?m=1&dir=up   (m = 0 both, 1 ghost 1, 2 ghost 2; dir = up or down)
void handleJog() {
  String dir = server.arg("dir");
  int m = server.arg("m").toInt();
  if (m < 0 || m > 2) m = 0;
  if (dir == "up")        jogDir = "up";
  else if (dir == "down") jogDir = "down";
  else { server.send(400, "text/plain", "bad dir"); return; }
  jogMotor = m;
  jogCount++;
  Serial.printf("JOG motor %d %s (#%u)\n", jogMotor, jogDir, jogCount);
  server.send(200, "text/plain", "ok");
}

// Drain the UART buffer and keep the newest valid frame.
void readLidar() {
  while (lidarSerial.available() >= 9) {
    if (lidarSerial.peek() != 0x59) { lidarSerial.read(); continue; }

    uint8_t f[9];
    lidarSerial.readBytes(f, 9);
    if (f[1] != 0x59) continue;

    uint8_t sum = 0;
    for (int i = 0; i < 8; i++) sum += f[i];
    if (sum != f[8]) continue;

    latestDistance = f[2] | (f[3] << 8);
    latestStrength = f[4] | (f[5] << 8);
    latestTemp     = (int16_t)(f[6] | (f[7] << 8)) / 100.0f;
    latestValid    = (latestStrength >= 100 && latestStrength != 65535);
  }
}

void checkTrigger() {
  if (!latestValid) return;

  if (latestDistance > RELEASE_CM) { armed = true; nearState = false; }
  if (latestDistance < TRIGGER_CM) nearState = true;

  if (armed && latestDistance < TRIGGER_CM && millis() - lastTrigger > COOLDOWN_MS) {
    Serial.printf(">>> TRIGGER at %u cm\n", latestDistance);
    if (dfReady) dfp.playFileNum(TRACK_NUMBER);
    lastTrigger = millis();
    armed = false;
    triggerCount++;
  }
}

void setup() {
  Serial.begin(115200);

  // Lidar
  lidarSerial.begin(115200, SERIAL_8N1, LIDAR_RX, LIDAR_TX);

  // DFPlayer Pro (runs at 115200)
  dfSerial.begin(115200, SERIAL_8N1, DF_RX, DF_TX);
  delay(1000);
  for (int i = 0; i < 5 && !dfReady; i++) {
    dfReady = dfp.begin(dfSerial);
    if (!dfReady) { Serial.println("DFPlayer Pro not found, retrying..."); delay(1000); }
  }
  if (dfReady) {
    dfp.setVol(VOLUME);
    dfp.switchFunction(dfp.MUSIC);
    delay(2000);
    dfp.setPlayMode(dfp.SINGLE);
    Serial.println("DFPlayer Pro ready.");
  } else {
    Serial.println("DFPlayer Pro not responding. Continuing without sound.");
  }

  // Wi-Fi access point + web page
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword, 1, 0, 4);
  Serial.print("Web page: http://"); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/jog", handleJog);
  server.begin();
}

void loop() {
  readLidar();
  checkTrigger();
  server.handleClient();
  readLidar();
}
