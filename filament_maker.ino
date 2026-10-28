#include <math.h>
#include <errno.h>
#include <float.h>
#include <atomic>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "dashboard.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

Preferences prefs;

// ================= THERMISTOR =================
const int THERM_PIN = 15;

const float SERIES_RESISTOR = 10000.0;
const float NOMINAL_RESISTANCE = 100000.0;
const float NOMINAL_TEMPERATURE = 25.0;
const float BETA_COEFFICIENT = 3950.0;
const float MIN_SAFE_TEMP = 0.0;     // Below this = disconnected or open circuit
const float MAX_SAFE_TEMP = 280.0;   // Above this = overtemperature hazard
const float MAX_SETPOINT = 260.0;    // Maximum user-configurable setpoint
const float MIN_SETPOINT = 0.0;
const float MIN_RPM = 0.1;
const float MAX_RPM = 60.0;

// ================= BTS7960 =================
const int ENABLE_PIN = 4;
const int RPWM_PIN = 5;
const int LPWM_PIN = 6;

// ================= STEPPER =================
#define STEP_PIN 18
#define DIR_PIN 17

const int stepsPerRevolution = 3200;
const float GEAR_RATIO = 25.0; // Stepper turns 25x per 1 wheel revolution

const float STABLE_BAND = 4.0; // Used to show stable/heat indicator

// ================= FREERTOS SHARED STATE =================
// ponytail: one mutex serializes state, PID calculations and output writes.
// Keep sensor reads, LCD, flash writes and network/serial I/O outside the lock.
SemaphoreHandle_t stateMutex = NULL;
#define LOCK_STATE()   xSemaphoreTake(stateMutex, portMAX_DELAY)
#define UNLOCK_STATE() xSemaphoreGive(stateMutex)

float currentTemp = 0;
float setpoint = 210.0;
float rpm = 15.0; // Output wheel RPM
int pwmOutput = 0;
bool heaterEnabled = false;
bool motorRunning = false;
bool direction = HIGH;
int currentStep = 0; // 0=idle, 1=Step 1, 2=Step 2, 3=Step 3
float step1Setpoint = 100.0;
float step2Setpoint = 200.0;

bool thermalFault = false;
String faultReason = "";

float Kp = 19.0;
float Ki = 0.45;
float Kd = 1.5;

bool verbose = false;

// PID state and resets are protected by stateMutex.
float integral = 0;
float lastTemp = 0;
bool pidResetRequested = true;
// Filter state is accessed only by TaskThermal.
float adcFiltered = 0;

// ================= WIFI + OTA + WEB SERVER =================
WebServer server(80);

const char* ap_ssid = "Filament-Maker-AP";
const char* ap_pass = WIFI_AP_PASSWORD;

// LAN / Station Wi-Fi credentials
const char* sta_ssid = WIFI_STA_SSID;
const char* sta_pass = WIFI_STA_PASSWORD;
const char* mdns_hostname = "filament-maker";

IPAddress AP_IP(192,168,1,1);
IPAddress AP_GATEWAY(192,168,1,1);
IPAddress AP_SUBNET(255,255,255,0);

// ================= LCD + ENCODER =================
// LCD 2004 I2C: SDA=GPIO8, SCL=GPIO9
// Encoder KY-040: CLK=GPIO11, DT=GPIO12, SW=GPIO13
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

#define ENC_CLK_PIN 11
#define ENC_DT_PIN 12
#define ENC_SW_PIN 13

LiquidCrystal_I2C lcd(0x27, 20, 4);

enum UiMode { UI_STATUS, UI_MENU, UI_EDIT };
enum EditTarget { EDIT_NONE, EDIT_S1SP, EDIT_S2SP, EDIT_RPM };

UiMode uiMode = UI_STATUS;
EditTarget editTarget = EDIT_NONE;
float editValue = 0;

const int MENU_COUNT = 10;
int menuIndex = 0;

unsigned long lastLcdUpdate = 0;
const unsigned long LCD_INTERVAL_MS = 250;
std::atomic<bool> lcdNeedsRefresh{false};

void lcdForce() {
  lcdNeedsRefresh = true;
}

// Encoder ISR state
volatile long encDelta = 0;
volatile uint8_t encPrevState = 0;

void IRAM_ATTR encISR() {
  uint8_t clk = digitalRead(ENC_CLK_PIN);
  uint8_t dt = digitalRead(ENC_DT_PIN);
  uint8_t cur = (clk << 1) | dt;
  uint8_t transition = (encPrevState << 2) | cur;
  if (transition == 0b0001 || transition == 0b0111 ||
      transition == 0b1000 || transition == 0b1110) {
    encDelta++;
  } else if (transition == 0b0010 || transition == 0b0100 ||
             transition == 0b1101 || transition == 0b1011) {
    encDelta--;
  }
  encPrevState = cur;
}

// Button debounce / long-press state
int lastSwReading = HIGH;
int btnState = HIGH;
unsigned long lastBtnDebounce = 0;
unsigned long btnPressStart = 0;
bool longFired = false;

// =====================================================
// SENSOR & HARDWARE HELPERS
// =====================================================
int readADC() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(THERM_PIN);
  }
  return sum / 10;
}

float readTempC() {
  int adc = readADC();

  if (adcFiltered == 0)
    adcFiltered = adc;

  adcFiltered = 0.8 * adcFiltered + 0.2 * adc;

  if (adcFiltered <= 15 || adcFiltered >= 4080)
    return -100;

  float resistance = SERIES_RESISTOR * adcFiltered / (4095.0 - adcFiltered);
  float tempK = 1.0 / (log(resistance / NOMINAL_RESISTANCE) / BETA_COEFFICIENT + 1.0 / (NOMINAL_TEMPERATURE + 273.15));

  return tempK - 273.15;
}

// Caller holds stateMutex: never write an output from an unlocked snapshot.
void updateStepperOutputLocked() {
  if (motorRunning) {
    float motorRPM = rpm * GEAR_RATIO;
    float stepFreq = (motorRPM / 60.0) * stepsPerRevolution;
    ledcWriteTone(STEP_PIN, (uint32_t)stepFreq);
  } else {
    ledcWriteTone(STEP_PIN, 0);
  }
}

void updateStepperOutput() {
  LOCK_STATE();
  updateStepperOutputLocked();
  UNLOCK_STATE();
}

bool parseNumber(const String& text, float minimum, float maximum, float& value) {
  String trimmed = text;
  trimmed.trim();
  char* end;
  errno = 0;
  float parsed = strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || *end != '\0' || errno == ERANGE ||
      !isfinite(parsed) || parsed < minimum || parsed > maximum) return false;
  value = parsed;
  return true;
}

float boundedSetting(float value, float minimum, float maximum, float fallback) {
  return isfinite(value) && value >= minimum && value <= maximum ? value : fallback;
}

// Caller holds stateMutex, including the fault check and physical shutoff.
bool setHeaterEnabledLocked(bool on) {
  if (on && thermalFault) return false;
  heaterEnabled = on;
  pidResetRequested = true;
  integral = 0;
  if (!on) {
    pwmOutput = 0;
    ledcWrite(RPWM_PIN, 0);
    ledcWrite(LPWM_PIN, 0);
    digitalWrite(ENABLE_PIN, LOW);
  }
  return true;
}

// =====================================================
// SAFETY FAULT HELPERS
// =====================================================
// Thermal task only; caller holds stateMutex. Logging happens after unlocking.
void tripThermalFaultLocked(const String& reason) {
  thermalFault = true;
  faultReason = reason;
  setHeaterEnabledLocked(false);
  currentStep = 0;
  lcdForce();
}

bool resetFault() {
  bool ok = false;
  LOCK_STATE();
  if (currentTemp >= MIN_SAFE_TEMP && currentTemp <= MAX_SAFE_TEMP) {
    thermalFault = false;
    faultReason = "";
    integral = 0;
    pidResetRequested = true;
    ok = true;
  }
  UNLOCK_STATE();

  if (ok) {
    lcdForce();
  }
  return ok;
}

// =====================================================
// PROCESS STEP HELPERS (Thread-safe)
// =====================================================
void startStep1() {
  LOCK_STATE();
  if (thermalFault) { UNLOCK_STATE(); return; }
  currentStep = 1;
  setpoint = step1Setpoint;
  setHeaterEnabledLocked(true);
  UNLOCK_STATE();

  lcdForce();
}

void stopStep1() {
  LOCK_STATE();
  if (currentStep == 1) currentStep = 0;
  setHeaterEnabledLocked(false);
  UNLOCK_STATE();

  lcdForce();
}

void startStep2() {
  LOCK_STATE();
  if (thermalFault) { UNLOCK_STATE(); return; }
  currentStep = 2;
  setpoint = step2Setpoint;
  setHeaterEnabledLocked(true);
  UNLOCK_STATE();

  lcdForce();
}

void stopStep2() {
  LOCK_STATE();
  if (currentStep == 2) currentStep = 0;
  setHeaterEnabledLocked(false);
  UNLOCK_STATE();

  lcdForce();
}

void startStep3() {
  LOCK_STATE();
  currentStep = 3;
  motorRunning = true;
  updateStepperOutputLocked();
  UNLOCK_STATE();

  lcdForce();
}

void stopStep3() {
  LOCK_STATE();
  motorRunning = false;
  if (currentStep == 3) {
    currentStep = heaterEnabled ? 2 : 0;
  }
  updateStepperOutputLocked();
  UNLOCK_STATE();

  lcdForce();
}

void stopAll() {
  LOCK_STATE();
  currentStep = 0;
  setHeaterEnabledLocked(false);
  motorRunning = false;
  updateStepperOutputLocked();
  UNLOCK_STATE();

  lcdForce();
}

void toggleHeater() {
  LOCK_STATE();
  setHeaterEnabledLocked(!heaterEnabled);
  UNLOCK_STATE();

  lcdForce();
}

void toggleMotor() {
  LOCK_STATE();
  motorRunning = !motorRunning;
  updateStepperOutputLocked();
  UNLOCK_STATE();

  lcdForce();
}

// =====================================================
// LCD RENDERING (TaskUI exclusive — eliminates I2C contention)
// =====================================================
void lcdLine(int row, String s) {
  if ((int)s.length() < 20) {
    for (int i = s.length(); i < 20; i++) s += " ";
  } else if ((int)s.length() > 20) {
    s = s.substring(0, 20);
  }
  lcd.setCursor(0, row);
  lcd.print(s);
}

String menuLabel(int idx) {
  LOCK_STATE();
  int step = currentStep;
  float s1sp = step1Setpoint;
  float s2sp = step2Setpoint;
  bool mot = motorRunning;
  float r = rpm;
  bool heat = heaterEnabled;
  UNLOCK_STATE();

  switch (idx) {
    case 0: return String(step == 1 ? "Stop S1 " : "Start S1 ") + String(s1sp, 0) + "C";
    case 1: return String(step == 2 ? "Stop S2 " : "Start S2 ") + String(s2sp, 0) + "C";
    case 2: return String(mot ? "Stop STEPPER" : "Start STEPPER");
    case 3: return "Set S1SP:" + String(s1sp, 0) + "C";
    case 4: return "Set S2SP:" + String(s2sp, 0) + "C";
    case 5: return "Set RPM:" + String(r, 1);
    case 6: return String("Heater:") + String(heat ? "ON" : "OFF");
    case 7: return String("Motor:") + String(mot ? "ON" : "OFF");
    case 8: return "STOP ALL";
    case 9: return "IP:" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "192.168.1.1");
    default: return "";
  }
}

void lcdShowStatus() {
  LOCK_STATE();
  bool fault = thermalFault;
  String reason = faultReason;
  float temp = currentTemp;
  float sp = setpoint;
  bool heat = heaterEnabled;
  bool mot = motorRunning;
  int pwm = pwmOutput;
  int step = currentStep;
  float s1sp = step1Setpoint;
  float s2sp = step2Setpoint;
  float r = rpm;
  UNLOCK_STATE();

  if (fault) {
    lcdLine(0, "! SAFETY FAULT !");
    lcdLine(1, reason.substring(0, 20));
    lcdLine(2, "T:" + (temp <= -50 ? "ERR" : String(temp, 1)) + "C PWM:0");
    lcdLine(3, "Hold btn to reset");
    return;
  }

  String suffix;
  if (!heat) suffix = " OFF";
  else if (abs(sp - temp) <= STABLE_BAND) suffix = " OK";
  else suffix = " HEAT";

  String l0 = "T:" + String(temp, 1) + "->" + String(sp, 1);
  if (l0.length() + suffix.length() > 20) l0 = l0.substring(0, 20 - suffix.length());
  l0 += suffix;

  String l1 = "PWM:" + String(pwm) + " H:" + String(heat ? "ON" : "OFF") + " M:" + String(mot ? "ON" : "OFF");
  String l2 = "St" + String(step) + " S1:" + String(s1sp, 0) + " S2:" + String(s2sp, 0);
  String l3 = "RPM:" + String(r, 1) + " clk=menu";

  lcdLine(0, l0);
  lcdLine(1, l1);
  lcdLine(2, l2);
  lcdLine(3, l3);
}

void lcdShowMenu() {
  int top = (menuIndex / 4) * 4;
  for (int r = 0; r < 4; r++) {
    int idx = top + r;
    if (idx >= MENU_COUNT) {
      lcdLine(r, "");
    } else {
      String prefix = (idx == menuIndex) ? ">" : " ";
      lcdLine(r, prefix + menuLabel(idx));
    }
  }
}

void lcdShowEdit() {
  String title = "";
  String unit = "";
  int decimals = 0;
  if (editTarget == EDIT_S1SP) { title = "Edit Step1 SP"; unit = "C"; decimals = 0; }
  else if (editTarget == EDIT_S2SP) { title = "Edit Step2 SP"; unit = "C"; decimals = 0; }
  else if (editTarget == EDIT_RPM) { title = "Edit Wheel RPM"; unit = ""; decimals = 1; }

  lcdLine(0, title);
  lcdLine(1, String(editValue, decimals) + unit);
  lcdLine(2, "rot=chg click=save");
  lcdLine(3, "hold=back no-save");
}

void lcdRefresh(bool force) {
  unsigned long now = millis();
  bool requested = lcdNeedsRefresh.exchange(false);
  if (!force && !requested && (now - lastLcdUpdate < LCD_INTERVAL_MS)) return;
  lastLcdUpdate = now;

  if (uiMode == UI_STATUS) lcdShowStatus();
  else if (uiMode == UI_MENU) lcdShowMenu();
  else if (uiMode == UI_EDIT) lcdShowEdit();
}

// =====================================================
// ENCODER & BUTTON LOGIC
// =====================================================
void enterEdit(int t) {
  editTarget = (EditTarget)t;
  LOCK_STATE();
  if (t == EDIT_S1SP) editValue = step1Setpoint;
  else if (t == EDIT_S2SP) editValue = step2Setpoint;
  else if (t == EDIT_RPM) editValue = rpm;
  UNLOCK_STATE();

  uiMode = UI_EDIT;
  lcd.clear();
  lcdForce();
}

void saveEditAndToMenu() {
  if (editTarget == EDIT_S1SP) {
    LOCK_STATE();
    step1Setpoint = editValue;
    if (currentStep == 1) { setpoint = step1Setpoint; }
    pidResetRequested = true;
    UNLOCK_STATE();
    prefs.putFloat("step1sp", editValue);
  } else if (editTarget == EDIT_S2SP) {
    LOCK_STATE();
    step2Setpoint = editValue;
    if (currentStep == 2) { setpoint = step2Setpoint; }
    pidResetRequested = true;
    UNLOCK_STATE();
    prefs.putFloat("step2sp", editValue);
  } else if (editTarget == EDIT_RPM) {
    float v = constrain(editValue, MIN_RPM, MAX_RPM);
    LOCK_STATE();
    rpm = v;
    updateStepperOutputLocked();
    UNLOCK_STATE();
    prefs.putFloat("rpm", v);
  }
  editTarget = EDIT_NONE;
  uiMode = UI_MENU;
  lcd.clear();
  lcdForce();
}

void onEncoderTurn(int dir) {
  LOCK_STATE();
  bool verb = verbose;
  UNLOCK_STATE();
  if (verb) {
    Serial.print("ENC turn dir=");
    Serial.println(dir);
  }

  if (uiMode == UI_STATUS) {
    uiMode = UI_MENU;
    lcd.clear();
  }
  if (uiMode == UI_MENU) {
    menuIndex += dir;
    if (menuIndex < 0) menuIndex = MENU_COUNT - 1;
    if (menuIndex >= MENU_COUNT) menuIndex = 0;
    lcdForce();
  } else if (uiMode == UI_EDIT) {
    if (editTarget == EDIT_S1SP || editTarget == EDIT_S2SP) {
      editValue += dir * 1.0;
      editValue = constrain(editValue, MIN_SETPOINT, MAX_SETPOINT);
    } else if (editTarget == EDIT_RPM) {
      editValue += dir * 0.5;
      editValue = constrain(editValue, MIN_RPM, MAX_RPM);
    }
    lcdForce();
  }
}

void activateMenuItem(int idx) {
  switch (idx) {
    case 0: {
      LOCK_STATE(); int st = currentStep; UNLOCK_STATE();
      if (st == 1) stopStep1(); else startStep1();
      break;
    }
    case 1: {
      LOCK_STATE(); int st = currentStep; UNLOCK_STATE();
      if (st == 2) stopStep2(); else startStep2();
      break;
    }
    case 2: {
      LOCK_STATE(); bool mot = motorRunning; UNLOCK_STATE();
      if (mot) stopStep3(); else startStep3();
      break;
    }
    case 3: enterEdit(EDIT_S1SP); return;
    case 4: enterEdit(EDIT_S2SP); return;
    case 5: enterEdit(EDIT_RPM); return;
    case 6: toggleHeater(); break;
    case 7: toggleMotor(); break;
    case 8:
      stopAll();
      uiMode = UI_STATUS;
      lcd.clear();
      lcdForce();
      return;
    case 9:
      // Informational IP item
      break;
  }
  lcdForce();
}

void onButtonShort() {
  LOCK_STATE();
  bool verb = verbose;
  UNLOCK_STATE();
  if (verb) Serial.println("BTN short press");

  if (uiMode == UI_STATUS) {
    uiMode = UI_MENU;
    menuIndex = 0;
    lcd.clear();
    lcdForce();
  } else if (uiMode == UI_MENU) {
    activateMenuItem(menuIndex);
  } else if (uiMode == UI_EDIT) {
    saveEditAndToMenu();
  }
}

void onButtonLong() {
  LOCK_STATE();
  bool verb = verbose;
  bool fault = thermalFault;
  UNLOCK_STATE();
  if (verb) Serial.println("BTN long press -> status");

  if (fault) {
    resetFault();
  }
  uiMode = UI_STATUS;
  editTarget = EDIT_NONE;
  lcd.clear();
  lcdForce();
}

void pollEncoder() {
  long steps = 0;
  noInterrupts();
  steps = encDelta;
  encDelta = 0;
  interrupts();

  static long acc = 0;
  acc += steps;
  while (acc >= 2) { onEncoderTurn(1); acc -= 2; }
  while (acc <= -2) { onEncoderTurn(-1); acc += 2; }
}

void pollButton() {
  int r = digitalRead(ENC_SW_PIN);
  if (r != lastSwReading) {
    lastBtnDebounce = millis();
  }
  lastSwReading = r;

  if ((millis() - lastBtnDebounce) > 50) {
    if (r != btnState) {
      btnState = r;
      if (btnState == LOW) {
        btnPressStart = millis();
        longFired = false;
      } else {
        if (!longFired) onButtonShort();
      }
    }
  }

  if (btnState == LOW && !longFired && (millis() - btnPressStart > 800)) {
    longFired = true;
    onButtonLong();
  }
}

// =====================================================
// WEB SERVER STATUS BUILDER
// =====================================================
String buildStatus() {
  LOCK_STATE();
  float t = currentTemp;
  float sp = setpoint;
  float r = rpm;
  int pwm = pwmOutput;
  bool heat = heaterEnabled;
  bool mot = motorRunning;
  int st = currentStep;
  float s1sp = step1Setpoint;
  float s2sp = step2Setpoint;
  bool fault = thermalFault;
  String fMsg = faultReason;
  UNLOCK_STATE();

  String json = "{";
  json += "\"temp\":" + String(t, 1) + ",";
  json += "\"setpoint\":" + String(sp, 1) + ",";
  json += "\"rpm\":" + String(r, 1) + ",";
  json += "\"pwm\":" + String(pwm) + ",";
  json += "\"heater\":" + String(heat ? "true" : "false") + ",";
  json += "\"stepper\":" + String(mot ? "true" : "false") + ",";
  json += "\"step\":" + String(st) + ",";
  json += "\"step1sp\":" + String(s1sp, 1) + ",";
  json += "\"step2sp\":" + String(s2sp, 1) + ",";
  json += "\"stable\":" + String((abs(sp - t) <= STABLE_BAND) ? "true" : "false") + ",";
  json += "\"fault\":" + String(fault ? "true" : "false") + ",";
  json += "\"faultMsg\":\"" + fMsg + "\",";
  json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "192.168.1.1") + "\",";
  json += "\"hostname\":\"" + String(mdns_hostname) + ".local\"";
  json += "}";
  return json;
}

// =====================================================
// SERIAL COMMANDS (Executed in loop() / Core 1)
// =====================================================
void handleSerial() {
  if (!Serial.available())
    return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.startsWith("T")) {
    float sp;
    if (!parseNumber(cmd.substring(1), MIN_SETPOINT, MAX_SETPOINT, sp)) {
      Serial.println("Invalid setpoint: expected 0-260 C");
      return;
    }
    LOCK_STATE();
    setpoint = sp;
    pidResetRequested = true;
    UNLOCK_STATE();
    Serial.print("Setpoint = ");
    Serial.println(sp);
    lcdForce();
  } else if (cmd.startsWith("P") || cmd.startsWith("I") || cmd.startsWith("D")) {
    float gain;
    if (!parseNumber(cmd.substring(1), 0.0f, FLT_MAX, gain)) {
      Serial.println("Invalid PID gain: expected a finite non-negative number");
      return;
    }
    LOCK_STATE();
    if (cmd.startsWith("P")) Kp = gain;
    else if (cmd.startsWith("I")) Ki = gain;
    else Kd = gain;
    pidResetRequested = true;
    UNLOCK_STATE();
    Serial.print(cmd.substring(0, 1) + " gain = ");
    Serial.println(gain);
  } else if (cmd == "R" || cmd == "reset") {
    LOCK_STATE(); pidResetRequested = true; UNLOCK_STATE();
    if (resetFault()) {
      Serial.println("Fault reset OK");
    } else {
      LOCK_STATE(); bool fault = thermalFault; UNLOCK_STATE();
      if (fault) {
        Serial.println("Cannot reset: Temp out of safe range");
      } else {
        Serial.println("PID Reset");
      }
    }
  } else if (cmd.startsWith("S")) {
    float r;
    if (!parseNumber(cmd.substring(1), MIN_RPM, MAX_RPM, r)) {
      Serial.println("Invalid RPM: expected 0.1-60");
      return;
    }
    LOCK_STATE();
    rpm = r;
    updateStepperOutputLocked();
    UNLOCK_STATE();
    Serial.print("RPM = ");
    Serial.println(r);
    lcdForce();
  } else if (cmd == "s") {
    startStep3();
    Serial.println("Motor started");
  } else if (cmd == "f") {
    stopStep3();
    Serial.println("Motor stopped");
  } else if (cmd == "d") {
    LOCK_STATE();
    direction = !direction;
    digitalWrite(DIR_PIN, direction);
    UNLOCK_STATE();
    Serial.println("Direction changed");
  } else if (cmd == "1") {
    startStep1();
    Serial.println("Step 1 started");
  } else if (cmd == "!1") {
    stopStep1();
    Serial.println("Step 1 stopped");
  } else if (cmd == "2") {
    startStep2();
    Serial.println("Step 2 started");
  } else if (cmd == "!2") {
    stopStep2();
    Serial.println("Step 2 stopped");
  } else if (cmd == "3") {
    startStep3();
    Serial.println("Step 3 started");
  } else if (cmd == "!3") {
    stopStep3();
    Serial.println("Step 3 stopped");
  } else if (cmd == "e") {
    Serial.print("CLK="); Serial.print(digitalRead(ENC_CLK_PIN));
    Serial.print(" DT="); Serial.print(digitalRead(ENC_DT_PIN));
    Serial.print(" SW="); Serial.print(digitalRead(ENC_SW_PIN));
    Serial.print(" delta="); Serial.println(encDelta);
  } else if (cmd == "E") {
    Serial.println("Watching ENC 11/12 + SW 13 for 5s...");
    Serial.println("Rotate slowly + press the knob now!");
    int lastC = digitalRead(ENC_CLK_PIN);
    int lastD = digitalRead(ENC_DT_PIN);
    int lastS = digitalRead(ENC_SW_PIN);
    long cEdges = 0, dEdges = 0, sEdges = 0;
    long startDelta;
    noInterrupts(); startDelta = encDelta; interrupts();
    unsigned long t0 = millis();
    while (millis() - t0 < 5000) {
      int c = digitalRead(ENC_CLK_PIN);
      int d = digitalRead(ENC_DT_PIN);
      int s = digitalRead(ENC_SW_PIN);
      if (c != lastC) { cEdges++; lastC = c; Serial.print("C->"); Serial.println(c); }
      if (d != lastD) { dEdges++; lastD = d; Serial.print("D->"); Serial.println(d); }
      if (s != lastS) { sEdges++; lastS = s; Serial.print("S->"); Serial.println(s); }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    long endDelta;
    noInterrupts(); endDelta = encDelta; interrupts();
    Serial.print("RESULT CLK edges="); Serial.print(cEdges);
    Serial.print(" DT edges="); Serial.print(dEdges);
    Serial.print(" SW edges="); Serial.print(sEdges);
    Serial.print(" ISR delta="); Serial.println(endDelta - startDelta);
  } else if (cmd == "m") {
    LOCK_STATE();
    verbose = !verbose;
    bool v = verbose;
    UNLOCK_STATE();
    prefs.putBool("verbose", v);
    Serial.println(v ? "log ON" : "log OFF");
  } else if (cmd == "v1" || cmd == "V1") {
    LOCK_STATE(); verbose = true; UNLOCK_STATE();
    prefs.putBool("verbose", true);
    Serial.println("log ON");
  } else if (cmd == "v0" || cmd == "V0") {
    LOCK_STATE(); verbose = false; UNLOCK_STATE();
    prefs.putBool("verbose", false);
    Serial.println("log OFF");
  }
}

// =====================================================
// FREERTOS TASKS
// =====================================================

// TaskThermal: Core 1, Priority 5 (nominal 100ms thermal loop)
void taskThermal(void* pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100);
  unsigned long lastCycle = millis();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    unsigned long now = millis();
    float dt = (now - lastCycle) / 1000.0;
    if (dt <= 0.001) dt = 0.1;
    lastCycle = now;

    float temp = readTempC();

    LOCK_STATE();
    currentTemp = temp;
    float sp = setpoint;
    bool heater = heaterEnabled;
    bool fault = thermalFault;
    float kp = Kp, ki = Ki, kd = Kd;
    int step = currentStep;
    bool verb = verbose;
    float wheelRpm = rpm;
    bool wasFault = fault;
    static unsigned long overtempStartTime = 0;
    static unsigned long riseCheckTime = 0;
    static float riseStartTemp = 0;
    static bool riseTracking = false;
    if (pidResetRequested) {
      integral = 0;
      lastTemp = temp;
      pidResetRequested = false;
    }

    // Safety check 1: Range limits (Sensor disconnect, short, or extreme overtemp)
    if (!isfinite(temp) || temp < MIN_SAFE_TEMP || temp > MAX_SAFE_TEMP) {
      if (!fault) {
        tripThermalFaultLocked(!isfinite(temp) || temp < MIN_SAFE_TEMP ? "SENSOR DISCONNECTED" : "OVERTEMP HAZARD");
        fault = true;
        heater = false;
      }
    }

    // Safety check 2: Overtemperature runaway (MOSFET short / stuck heat past setpoint)
    if (heater && (temp > sp + 15.0)) {
      if (overtempStartTime == 0) {
        overtempStartTime = now;
      } else if (now - overtempStartTime >= 10000) {
        if (!fault) {
          tripThermalFaultLocked("OVERTEMP RUNAWAY");
          fault = true;
          heater = false;
        }
      }
    } else {
      overtempStartTime = 0;
    }

    int outPwm = 0;

    if (fault || !heater) {
      outPwm = 0;
      integral = 0;
      riseTracking = false;
    } else {
      // Safety check 3: Heating rise-rate watchdog (detached cartridge or dangling sensor)
      // Only enforced when far from setpoint (> 20C below target) and demanding high power (> 180 PWM)
      int lastPwm = pwmOutput;

      if ((sp - temp > 20.0) && (lastPwm > 180)) {
        if (!riseTracking) {
          riseTracking = true;
          riseCheckTime = now;
          riseStartTemp = temp;
        } else if (now - riseCheckTime >= 40000) {
          if (temp - riseStartTemp < 2.0) {
            tripThermalFaultLocked("HEATING FAILED");
            fault = true;
            heater = false;
          } else {
            riseCheckTime = now;
            riseStartTemp = temp;
          }
        }
      } else if (lastPwm < 100 || (sp - temp <= 20.0)) {
        riseTracking = false;
      }

      if (!fault && heater) {
        float error = sp - temp;
        if (abs(error) < 30) {
          integral += error * dt;
        }
        integral = constrain(integral, -300, 300);

        float derivative = -(temp - lastTemp) / dt;

        float Pterm = kp * error;
        float Iterm = ki * integral;
        float Dterm = kd * derivative;

        float output = Pterm + Iterm + Dterm;
        if (isfinite(output)) {
          outPwm = constrain(output, 0, 255);
        } else {
          tripThermalFaultLocked("INVALID PID OUTPUT");
        }
      } else {
        outPwm = 0;
        integral = 0;
      }
    }

    lastTemp = temp;
    // The same lock covers the decision and hardware commit, so a stop cannot
    // be overwritten by a thermal calculation or motor snapshot from before it.
    digitalWrite(ENABLE_PIN, heaterEnabled && !thermalFault ? HIGH : LOW);
    ledcWrite(RPWM_PIN, outPwm);
    pwmOutput = outPwm;
    String newFaultReason = !wasFault && thermalFault ? faultReason : "";
    UNLOCK_STATE();

    if (newFaultReason.length()) {
      Serial.print("SAFETY FAULT: ");
      Serial.println(newFaultReason);
    }
    if (verb) {
      float err = sp - temp;
      Serial.print("Temp:"); Serial.print(temp, 1);
      Serial.print(" | Set:"); Serial.print(sp, 1);
      Serial.print(" | Step:"); Serial.print(step);
      Serial.print(" | PWM:"); Serial.print(outPwm);
      Serial.print(" | Error:"); Serial.print(err);
      Serial.print(" | RPM:"); Serial.print(wheelRpm);
      Serial.print(" | Kp:"); Serial.print(kp);
      Serial.print(" | Ki:"); Serial.print(ki);
      Serial.print(" | Kd:"); Serial.print(kd);
      Serial.println();
    }
  }
}

// TaskUI: Core 1, Priority 2 (LCD + Encoder, 20ms polling, exclusive I2C owner)
void taskUI(void* pvParameters) {
  for (;;) {
    pollEncoder();
    pollButton();
    lcdRefresh(false);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// TaskNetwork: Core 0, Priority 1 (WebServer + ElegantOTA + Wi-Fi monitoring)
void taskNetwork(void* pvParameters) {
  for (;;) {
    server.handleClient();
    ElegantOTA.loop();

    static bool lanConnected = false;
    if (!lanConnected && WiFi.status() == WL_CONNECTED) {
      lanConnected = true;
      Serial.print("Connected to LAN! IP: ");
      Serial.println(WiFi.localIP());
      Serial.printf("Web UI: http://%s.local or http://%s\n", mdns_hostname, WiFi.localIP().toString().c_str());
      lcdForce();
    } else if (lanConnected && WiFi.status() != WL_CONNECTED) {
      lanConnected = false;
      Serial.println("LAN Wi-Fi disconnected");
      lcdForce();
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);

  stateMutex = xSemaphoreCreateMutex();

  prefs.begin("settings", false);
  setpoint = boundedSetting(prefs.getFloat("setpoint", 210.0), MIN_SETPOINT, MAX_SETPOINT, 210.0);
  rpm = boundedSetting(prefs.getFloat("rpm", 15.0), MIN_RPM, MAX_RPM, 15.0);
  step1Setpoint = boundedSetting(prefs.getFloat("step1sp", 100.0), MIN_SETPOINT, MAX_SETPOINT, 100.0);
  step2Setpoint = boundedSetting(prefs.getFloat("step2sp", 200.0), MIN_SETPOINT, MAX_SETPOINT, 200.0);
  verbose = prefs.getBool("verbose", false);

  // BTS7960
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);
  ledcAttach(RPWM_PIN, 1000, 8);
  ledcAttach(LPWM_PIN, 1000, 8);
  ledcWrite(RPWM_PIN, 0);
  ledcWrite(LPWM_PIN, 0);

  // Thermistor ADC
  analogReadResolution(12);

  // Stepper
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(DIR_PIN, direction);
  ledcAttach(STEP_PIN, 800, 8);
  updateStepperOutput();

  // LCD + Encoder
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UPCYCLE Controller");
  lcd.setCursor(0, 1);
  lcd.print("FreeRTOS Ready");

  pinMode(ENC_CLK_PIN, INPUT_PULLUP);
  pinMode(ENC_DT_PIN, INPUT_PULLUP);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);
  encPrevState = (digitalRead(ENC_CLK_PIN) << 1) | digitalRead(ENC_DT_PIN);
  encDelta = 0;
  attachInterrupt(digitalPinToInterrupt(ENC_CLK_PIN), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT_PIN), encISR, CHANGE);

  // WIFI (AP + STA + Web UI + OTA + mDNS)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(ap_ssid, ap_pass);

  if (strlen(sta_ssid) > 0 && strcmp(sta_ssid, "YOUR_WIFI_SSID") != 0) {
    WiFi.begin(sta_ssid, sta_pass);
    Serial.print("Connecting to LAN Wi-Fi: ");
    Serial.println(sta_ssid);
  }

  if (MDNS.begin(mdns_hostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS active: http://%s.local\n", mdns_hostname);
  }

  // Web routes
  server.on("/", []() { server.send(200, "text/html", webpage); });
  server.on("/status", []() { server.send(200, "application/json", buildStatus()); });
  server.on("/stopall", HTTP_POST, []() {
    stopAll();
    server.send(200, "text/plain", "OK");
  });
  server.on("/resetfault", []() {
    if (resetFault()) {
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Cannot reset: temperature out of range");
    }
  });
  server.on("/heater", []() {
    bool on = (server.arg("state") == "on");
    LOCK_STATE();
    bool ok = setHeaterEnabledLocked(on);
    UNLOCK_STATE();
    if (!ok) {
      server.send(400, "text/plain", "Thermal Fault Active");
      return;
    }
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/stepper", []() {
    if (server.arg("state") == "on") startStep3(); else stopStep3();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step1", []() {
    if (server.arg("state") == "on") startStep1(); else stopStep1();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step2", []() {
    if (server.arg("state") == "on") startStep2(); else stopStep2();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step3", []() {
    if (server.arg("state") == "on") startStep3(); else stopStep3();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step1setpoint", []() {
    float val;
    if (!parseNumber(server.arg("value"), MIN_SETPOINT, MAX_SETPOINT, val)) {
      server.send(400, "text/plain", "Invalid setpoint: expected 0-260 C");
      return;
    }
    LOCK_STATE();
    step1Setpoint = val;
    if (currentStep == 1) setpoint = val;
    pidResetRequested = true;
    UNLOCK_STATE();
    prefs.putFloat("step1sp", val);
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step2setpoint", []() {
    float val;
    if (!parseNumber(server.arg("value"), MIN_SETPOINT, MAX_SETPOINT, val)) {
      server.send(400, "text/plain", "Invalid setpoint: expected 0-260 C");
      return;
    }
    LOCK_STATE();
    step2Setpoint = val;
    if (currentStep == 2) setpoint = val;
    pidResetRequested = true;
    UNLOCK_STATE();
    prefs.putFloat("step2sp", val);
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/rpm", []() {
    float val;
    if (!parseNumber(server.arg("value"), MIN_RPM, MAX_RPM, val)) {
      server.send(400, "text/plain", "Invalid RPM: expected 0.1-60");
      return;
    }
    LOCK_STATE();
    rpm = val;
    updateStepperOutputLocked();
    UNLOCK_STATE();
    prefs.putFloat("rpm", val);
    lcdForce();
    server.send(200, "text/plain", "OK");
  });

  ElegantOTA.begin(&server);
  server.begin();

  Serial.println("=== UPCYCLE FreeRTOS Controller Ready ===");
  Serial.printf("Tasks starting: Thermal(Core 1, Prio 5), UI(Core 1, Prio 2), Net(Core 0, Prio 1)\n");

  delay(600);
  lcd.clear();
  lcdRefresh(true);

  // Spawn FreeRTOS Tasks
  xTaskCreatePinnedToCore(taskThermal, "TaskThermal", 4096, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(taskUI, "TaskUI", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskNetwork, "TaskNetwork", 8192, NULL, 1, NULL, 0);
}

// =====================================================
// MAIN ARDUINO LOOP (Core 1, Priority 1: Serial CLI & log)
// =====================================================
void loop() {
  handleSerial();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    LOCK_STATE();
    bool verb = verbose;
    float r = rpm;
    UNLOCK_STATE();
    if (verb) {
      Serial.print("RPM = ");
      Serial.println(r);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(15));
}
