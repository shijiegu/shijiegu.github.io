// ESP32 #2: two ghost winches (2x TB6600 + NEMA 17) + flickering lights.
// Joins the Wi-Fi of ESP32 #1 (esp1_sensor_sound_dual.ino) and polls /data:
//   trig     changes -> GHOST 2 drops 1 m, holds 5 s, rewinds 1 m
//   near     true    -> flicker the lights at 4 Hz
//   jog      changes -> nudge ghost 1, ghost 2, or both (jogmotor = 1, 2 or 0)
//
// GHOST 1 ignores the sensor: it runs its own random routine forever:
//   down - up - down - up - REST, repeat (random distances, speeds and pauses)
//
// Each motor has its own hardware timer, so they move independently.
// Needs ESP32 Arduino core 3.x.

#include <WiFi.h>
#include <HTTPClient.h>
#include <soc/gpio_struct.h>

// ---------- Wi-Fi / polling ----------
const char* LIDAR_SSID = "ESP32-Lidar-AP";
const char* LIDAR_PASS = "folklore";
const char* DATA_URL   = "http://192.168.4.1/data";
const uint32_t POLL_MS = 200;

// ---------- Lights ----------
const uint8_t LIGHT_IDLE   = 0;      // brightness when nobody is near
const uint8_t LIGHT_ACTIVE = 255;    // brightness during the flicker "on" phase
const float   FLICKER_HZ   = 4.0;

// ---------- Shared motion settings (same speeds as your working version) ----------
// Vibration depends on STEPS PER SECOND, not on spool size, so these stay as they were.
const int      STEPS_PER_REV = 1600;       // 1/8 microstep (must match the TB6600 DIP switches)
const float    MIN_SPS       = 800;        // start/stop speed (steps per second)
const float    MAX_SPS       = 2000;       // top speed
const long     RAMP_STEPS    = 1500;       // steps used to speed up / slow down
const float    JOG_MM        = 20.0;       // how far one Up/Down button press moves

// Spool diameters (core + line). Change these if your two spools differ.
const float SPOOL_DIAMETER_MM_1 = 60.4;    // ghost 1 (6 mm bore spool)
const float SPOOL_DIAMETER_MM_2 = 60.4;    // ghost 2 (5 mm bore spool)

// If a ghost goes UP when it should go DOWN, flip that motor's value.
const bool DOWN_IS_HIGH_1 = true;
const bool DOWN_IS_HIGH_2 = true;

// ---------- Ghost 2: sensor-triggered drop ----------
const float    DROP2_MM = 1000.0;          // 1 m
const uint32_t HOLD2_MS = 5000;            // pause at the bottom before rewinding
const uint32_t REARM2_MS = 2000;           // after it is back up, ignore triggers for this long

// ---------- Ghost 1: random down-up-down-up-rest routine ----------
const float    M1_MAX_DEPTH_MM   = 700.0;  // never go lower than this below the top
const float    M1_DOWN_MIN_MM    = 150.0;  // each "down" move is a random distance in this range
const float    M1_DOWN_MAX_MM    = 450.0;
const float    M1_UP_PARTIAL_MIN = 0.30;   // first "up" climbs back 30-70% of the current depth
const float    M1_UP_PARTIAL_MAX = 0.70;   // (the second "up" always goes all the way to the top)
const float    M1_MIN_MOVE_MM    = 30.0;   // ignore moves shorter than this
const float    M1_CRUISE_MIN_SPS = 1200;   // random top speed per move (stay within MIN_SPS..MAX_SPS)
const float    M1_CRUISE_MAX_SPS = 2000;
const uint32_t M1_PAUSE_MIN_MS   = 400;    // short pause between moves
const uint32_t M1_PAUSE_MAX_MS   = 1500;
const uint32_t M1_REST_MIN_MS    = 5000;   // rest after finishing a full cycle
const uint32_t M1_REST_MAX_MS    = 12000;
const uint32_t M1_START_DELAY_MS = 3000;   // wait after power-up before the first move
const uint32_t M1_JOG_HOLD_MS    = 15000;  // routine waits this long after you jog ghost 1

// ---------- Pins ----------
#define LIGHT_PIN 18   // -> MOSFET module TRIG/PWM
#define STEP1_PIN 27   // -> TB6600 #1 PUL+
#define DIR1_PIN  32   // -> TB6600 #1 DIR+
#define STEP2_PIN 26   // -> TB6600 #2 PUL+
#define DIR2_PIN  33   // -> TB6600 #2 DIR+
// PUL- and DIR- of BOTH drivers go to this ESP32's GND.

const float PI_F = 3.14159265f;

long mmToSteps(float mm, float spoolMm) {
  return (long)(mm / (PI_F * spoolMm) * STEPS_PER_REV + 0.5f);
}

const long DROP2_STEPS          = mmToSteps(DROP2_MM,        SPOOL_DIAMETER_MM_2);
const long JOG1_STEPS           = mmToSteps(JOG_MM,          SPOOL_DIAMETER_MM_1);
const long JOG2_STEPS           = mmToSteps(JOG_MM,          SPOOL_DIAMETER_MM_2);
const long M1_MAX_DEPTH_STEPS   = mmToSteps(M1_MAX_DEPTH_MM, SPOOL_DIAMETER_MM_1);
const long M1_MIN_MOVE_STEPS    = mmToSteps(M1_MIN_MOVE_MM,  SPOOL_DIAMETER_MM_1);

// ---------- One motor = one timer ----------
struct Motor {
  uint32_t     stepMask;
  uint8_t      dirPin;
  bool         downIsHigh;
  hw_timer_t*  timer;
  volatile long remaining;
  volatile bool stepHigh;
  long         moveTotal;
  float        moveMaxSps;
  uint32_t     lastRamp;
};
Motor mot[2];   // mot[0] = ghost 1, mot[1] = ghost 2

void IRAM_ATTR stepISR(int i) {
  Motor& m = mot[i];
  if (m.remaining <= 0) return;
  if (!m.stepHigh) {
    GPIO.out_w1ts = m.stepMask;
    m.stepHigh = true;
  } else {
    GPIO.out_w1tc = m.stepMask;
    m.stepHigh = false;
    m.remaining--;
  }
}
void IRAM_ATTR onStep1() { stepISR(0); }
void IRAM_ATTR onStep2() { stepISR(1); }

void setRate(int i, float sps) {
  timerAlarm(mot[i].timer, (uint32_t)(500000.0f / sps), true, 0);
}

bool motorBusy(int i) { return mot[i].remaining > 0; }

void startMove(int i, long steps, bool down, float maxSps) {
  Motor& m = mot[i];
  digitalWrite(m.dirPin, (down ? m.downIsHigh : !m.downIsHigh) ? HIGH : LOW);
  delayMicroseconds(50);               // let DIR settle before stepping
  m.moveTotal  = steps;
  m.moveMaxSps = maxSps;
  setRate(i, MIN_SPS);
  m.remaining = steps;
}

// Speed up, cruise, slow down (called often from loop)
void rampMotor(int i) {
  Motor& m = mot[i];
  long remaining = m.remaining;
  if (remaining <= 0) return;
  if (millis() - m.lastRamp < 5) return;
  m.lastRamp = millis();
  long done = m.moveTotal - remaining;
  long ramp = min(min(done, remaining), RAMP_STEPS);
  float sps = MIN_SPS + (m.moveMaxSps - MIN_SPS) * (float)ramp / (float)RAMP_STEPS;
  setRate(i, sps);
}

// ---------- random helpers ----------
float rndF(float lo, float hi) { return lo + (hi - lo) * (random(0, 10001) / 10000.0f); }
uint32_t rndMs(uint32_t lo, uint32_t hi) { return lo + (uint32_t)random(0, (long)(hi - lo) + 1); }

// ---------- Ghost 2: drop / hold / rewind ----------
enum Ghost2State { G2_IDLE, G2_DROPPING, G2_HOLD, G2_RAISING };
Ghost2State g2State = G2_IDLE;
uint32_t    g2HoldUntil = 0;
uint32_t    g2LockUntil = 0;

void startDrop() {
  if (g2State != G2_IDLE || motorBusy(1) || (int32_t)(millis() - g2LockUntil) < 0) {
    Serial.println("Trigger ignored: ghost 2 is busy");
    return;
  }
  Serial.printf("Ghost 2 dropping (%ld steps)\n", DROP2_STEPS);
  startMove(1, DROP2_STEPS, true, MAX_SPS);
  g2State = G2_DROPPING;
}

void updateGhost2() {
  switch (g2State) {
    case G2_IDLE:
      return;
    case G2_DROPPING:
      if (!motorBusy(1)) {
        g2State = G2_HOLD;
        g2HoldUntil = millis() + HOLD2_MS;
        Serial.println("Ghost 2 down, holding");
      }
      break;
    case G2_HOLD:
      if (millis() >= g2HoldUntil) {
        startMove(1, DROP2_STEPS, false, MAX_SPS);
        g2State = G2_RAISING;
        Serial.println("Ghost 2 rewinding");
      }
      break;
    case G2_RAISING:
      if (!motorBusy(1)) {
        g2State = G2_IDLE;
        g2LockUntil = millis() + REARM2_MS;
        Serial.println("Ghost 2 back up");
      }
      break;
  }
}

// ---------- Ghost 1: random down-up-down-up-rest ----------
enum M1Phase { P_DOWN1, P_UP1, P_DOWN2, P_UP2 };   // the NEXT move to run
M1Phase  m1Phase   = P_DOWN1;
long     m1Pos     = 0;          // steps below the top (0 = fully raised)
bool     m1Moving  = false;
uint32_t m1NextAt  = 0;

void updateGhost1() {
  if (motorBusy(0)) return;

  if (m1Moving) {                // a move just finished: schedule the pause after it
    m1Moving = false;
    // m1Phase has already advanced; P_DOWN1 means we just finished the last "up"
    m1NextAt = millis() + (m1Phase == P_DOWN1 ? rndMs(M1_REST_MIN_MS, M1_REST_MAX_MS)
                                              : rndMs(M1_PAUSE_MIN_MS, M1_PAUSE_MAX_MS));
    if (m1Phase == P_DOWN1) Serial.println("Ghost 1 resting");
  }
  if ((int32_t)(millis() - m1NextAt) < 0) return;

  M1Phase thisPhase = m1Phase;
  bool down = (thisPhase == P_DOWN1 || thisPhase == P_DOWN2);
  long steps;
  if (down) {
    steps = mmToSteps(rndF(M1_DOWN_MIN_MM, M1_DOWN_MAX_MM), SPOOL_DIAMETER_MM_1);
    steps = min(steps, M1_MAX_DEPTH_STEPS - m1Pos);
  } else if (thisPhase == P_UP1) {
    steps = (long)(m1Pos * rndF(M1_UP_PARTIAL_MIN, M1_UP_PARTIAL_MAX));
  } else {
    steps = m1Pos;               // final up: all the way back to the top
  }

  // advance to the next phase (wraps back to the first down)
  m1Phase = (M1Phase)((thisPhase + 1) % 4);

  if (steps < M1_MIN_MOVE_STEPS) {   // nothing sensible to do: skip this move
    m1NextAt = millis() + (m1Phase == P_DOWN1 ? rndMs(M1_REST_MIN_MS, M1_REST_MAX_MS) : 0);
    return;
  }

  float cruise = rndF(M1_CRUISE_MIN_SPS, M1_CRUISE_MAX_SPS);
  Serial.printf("Ghost 1 %s %ld steps @ %.0f sps\n", down ? "down" : "up", steps, cruise);
  m1Pos += down ? steps : -steps;
  startMove(0, steps, down, cruise);
  m1Moving = true;
}

// ---------- Jog buttons (from the web page on ESP32 #1) ----------
void jogOne(int idx, bool down) {
  if (motorBusy(idx)) return;
  if (idx == 1 && g2State != G2_IDLE) return;
  long s = (idx == 0) ? JOG1_STEPS : JOG2_STEPS;
  Serial.printf("Jog ghost %d %s\n", idx + 1, down ? "down" : "up");
  startMove(idx, s, down, MIN_SPS);
  if (idx == 0) {
    m1Pos += down ? s : -s;                       // keep the routine's position in sync
    m1NextAt = millis() + M1_JOG_HOLD_MS;         // give time to adjust before it resumes
    m1Moving = false;
  }
}

void startJog(int motorSel, bool down) {          // 0 = both, 1 = ghost 1, 2 = ghost 2
  if (motorSel == 0 || motorSel == 1) jogOne(0, down);
  if (motorSel == 0 || motorSel == 2) jogOne(1, down);
}

// ---------- Lights ----------
bool nearState = false;

void updateLight() {
  if (nearState) {
    uint32_t halfPeriod = (uint32_t)(500.0f / FLICKER_HZ);   // 125 ms at 4 Hz
    bool on = ((millis() / halfPeriod) % 2) == 0;
    ledcWrite(LIGHT_PIN, on ? LIGHT_ACTIVE : 0);
  } else {
    ledcWrite(LIGHT_PIN, LIGHT_IDLE);
  }
}

// ---------- reading ESP32 #1 ----------
bool     seeded    = false;
long     lastTrig  = 0;
long     lastJog   = 0;
int      failCount = 0;
uint32_t lastPoll  = 0;
uint32_t lastWifiTry = 0;

long getLong(const String& j, const char* key) {
  String k = String("\"") + key + "\":";
  int i = j.indexOf(k);
  if (i < 0) return -1;
  return j.substring(i + k.length()).toInt();
}

bool getBool(const String& j, const char* key) {
  String k = String("\"") + key + "\":";
  int i = j.indexOf(k);
  if (i < 0) return false;
  return j.startsWith("true", i + k.length());
}

bool jogIsDown(const String& j) {
  String k = "\"jogdir\":\"";
  int i = j.indexOf(k);
  if (i < 0) return false;
  return j.charAt(i + k.length()) == 'd';
}

void pollSensorBoard() {
  HTTPClient http;
  http.setConnectTimeout(400);
  http.setTimeout(400);
  http.begin(DATA_URL);
  int code = http.GET();

  if (code == 200) {
    String p = http.getString();
    failCount = 0;
    nearState = getBool(p, "near");
    long trig = getLong(p, "trig");
    long jog  = getLong(p, "jog");
    long jm   = getLong(p, "jogmotor");
    if (jm < 0 || jm > 2) jm = 0;

    if (!seeded) {                         // first reading: just remember the counters
      if (trig >= 0 && jog >= 0) { lastTrig = trig; lastJog = jog; seeded = true; }
    } else {
      if (trig >= 0) {
        if (trig < lastTrig) lastTrig = trig;          // sensor board rebooted: re-sync
        else if (trig > lastTrig) { lastTrig = trig; startDrop(); }
      }
      if (jog >= 0) {
        if (jog < lastJog) lastJog = jog;
        else if (jog > lastJog) { lastJog = jog; startJog((int)jm, jogIsDown(p)); }
      }
    }
  } else {
    failCount++;
    if (failCount > 15) nearState = false;   // lost the sensor board: don't leave lights flickering
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  // Lights
  ledcAttach(LIGHT_PIN, 20000, 8);   // 20 kHz: above hearing, no hum
  ledcWrite(LIGHT_PIN, LIGHT_IDLE);

  // Motors (ghosts must be fully raised when you power on)
  pinMode(STEP1_PIN, OUTPUT); pinMode(DIR1_PIN, OUTPUT);
  pinMode(STEP2_PIN, OUTPUT); pinMode(DIR2_PIN, OUTPUT);
  digitalWrite(STEP1_PIN, LOW);
  digitalWrite(STEP2_PIN, LOW);

  mot[0].stepMask = 1UL << STEP1_PIN; mot[0].dirPin = DIR1_PIN; mot[0].downIsHigh = DOWN_IS_HIGH_1;
  mot[1].stepMask = 1UL << STEP2_PIN; mot[1].dirPin = DIR2_PIN; mot[1].downIsHigh = DOWN_IS_HIGH_2;

  mot[0].timer = timerBegin(1000000);              // 1 MHz timer per motor
  timerAttachInterrupt(mot[0].timer, &onStep1);
  timerAlarm(mot[0].timer, 500, true, 0);
  mot[1].timer = timerBegin(1000000);
  timerAttachInterrupt(mot[1].timer, &onStep2);
  timerAlarm(mot[1].timer, 500, true, 0);

  m1NextAt = millis() + M1_START_DELAY_MS;

  // Join the sensor board's Wi-Fi
  Serial.print("Connecting to "); Serial.println(LIDAR_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(LIDAR_SSID, LIDAR_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected, IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNot connected yet, will keep retrying.");
  }
  Serial.printf("Ghost 2 drop = %ld steps, jog1 = %ld, jog2 = %ld, ghost 1 max depth = %ld steps\n",
                DROP2_STEPS, JOG1_STEPS, JOG2_STEPS, M1_MAX_DEPTH_STEPS);
}

void loop() {
  rampMotor(0);
  rampMotor(1);
  updateGhost1();      // runs on its own, even without Wi-Fi
  updateGhost2();
  updateLight();

  if (WiFi.status() != WL_CONNECTED) {
    nearState = false;
    if (millis() - lastWifiTry > 3000) {
      lastWifiTry = millis();
      WiFi.disconnect();
      WiFi.begin(LIDAR_SSID, LIDAR_PASS);
    }
    return;
  }

  if (millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();
    pollSensorBoard();
  }
}
