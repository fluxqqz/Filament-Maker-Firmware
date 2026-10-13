#include <math.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "dashboard.h"

Preferences prefs;

// ================= THERMISTOR =================
const int THERM_PIN = 15;

const float SERIES_RESISTOR = 10000.0;
const float NOMINAL_RESISTANCE = 100000.0;
const float NOMINAL_TEMPERATURE = 25.0;
const float BETA_COEFFICIENT = 3950.0;
const float MIN_SAFE_TEMP = 5.0;     // Below this = disconnected or open circuit
const float MAX_SAFE_TEMP = 280.0;   // Above this = overtemperature hazard

bool thermalFault = false;
String faultReason = "";

// ================= BTS7960 =================
const int ENABLE_PIN = 4;
const int RPWM_PIN = 5;
const int LPWM_PIN = 6;

// ================= PID =================
float setpoint = 210.0;

float Kp = 19.0;
float Ki = 0.45;
float Kd = 1.5;

float integral = 0;
float lastTemp = 0;

int pwmOutput = 0;

// ================= FILTER =================
float adcFiltered = 0;

// ================= TIMING =================
unsigned long lastPID = 0;

// ================= STEPPER =================
#define STEP_PIN 18
#define DIR_PIN 17

const int stepsPerRevolution = 3200;

// Stepper shaft drives the output wheel through a 1:25 gear reduction
// (stepper turns 25x for every 1 revolution of the output wheel).
// GEAR_RATIO = motor_RPM / wheel_RPM
const float GEAR_RATIO = 25.0;

float rpm = 15;  // this is WHEEL RPM (the output wheel), not motor RPM

bool motorRunning = false;
bool direction = HIGH;

const float STABLE_BAND = 4.0; // used only to show stable/heat indicator

// ================= PROCESS STEPS =================
// currentStep: 0 = idle, 1 = Step 1 (heat), 2 = Step 2 (heat), 3 = Step 3 (stepper running)
int currentStep = 0;

float step1Setpoint = 100.0;
float step2Setpoint = 200.0;

// =====================================
// WIFI + OTA + WEB APP
// =====================================
WebServer server(80);

const char* ap_ssid = "UPCYCLE";
const char* ap_pass = "12345678";

// LAN / Station Wi-Fi credentials
const char* sta_ssid = "YOUR_WIFI_SSID";
const char* sta_pass = "YOUR_WIFI_PASSWORD";
const char* mdns_hostname = "filament-maker";

IPAddress AP_IP(192,168,1,1);
IPAddress AP_GATEWAY(192,168,1,1);
IPAddress AP_SUBNET(255,255,255,0);

bool heaterEnabled = false;

float currentTemp = 0;

// Serial telemetry spam toggle. false = quiet boot (no 100ms Temp/PWM flood).
// Change via serial: m (toggle), v0 (off), v1 (on). Persisted in prefs.
bool verbose = false;

// Dashboard UI is located in dashboard.h

String buildStatus() {
  String json = "{";
  json += "\"temp\":" + String(currentTemp,1) + ",";
  json += "\"setpoint\":" + String(setpoint,1) + ",";
  json += "\"rpm\":" + String(rpm,1) + ",";
  json += "\"pwm\":" + String(pwmOutput) + ",";
  json += "\"heater\":" + String(heaterEnabled ? "true" : "false") + ",";
  json += "\"stepper\":" + String(motorRunning ? "true" : "false") + ",";
  json += "\"step\":" + String(currentStep) + ",";
  json += "\"step1sp\":" + String(step1Setpoint,1) + ",";
  json += "\"step2sp\":" + String(step2Setpoint,1) + ",";
  json += "\"stable\":" + String((abs(setpoint - currentTemp) <= STABLE_BAND) ? "true" : "false") + ",";
  json += "\"fault\":" + String(thermalFault ? "true" : "false") + ",";
  json += "\"faultMsg\":\"" + faultReason + "\",";
  json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "192.168.1.1") + "\",";
  json += "\"hostname\":\"" + String(mdns_hostname) + ".local\"";
  json += "}";
  return json;
}

// =====================================
// LCD + ENCODER (user wiring)
// =====================================
// LCD 2004 I2C: SDA=GPIO8, SCL=GPIO9
// Encoder KY-040: CLK=GPIO11, DT=GPIO12, SW=GPIO13
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

#define ENC_CLK_PIN 11
#define ENC_DT_PIN 12
#define ENC_SW_PIN 13

LiquidCrystal_I2C lcd(0x27, 20, 4); // if blank, try 0x3F

enum UiMode { UI_STATUS, UI_MENU, UI_EDIT };
enum EditTarget { EDIT_NONE, EDIT_S1SP, EDIT_S2SP, EDIT_RPM };

UiMode uiMode = UI_STATUS;
EditTarget editTarget = EDIT_NONE;
float editValue = 0;

const int MENU_COUNT = 10;
int menuIndex = 0;

unsigned long lastLcdUpdate = 0;
const unsigned long LCD_INTERVAL_MS = 250;

// Encoder ISR state (interrupt-driven, won't miss steps)
volatile long encDelta = 0;
volatile uint8_t encPrevState = 0;

void IRAM_ATTR encISR() {
  uint8_t clk = digitalRead(ENC_CLK_PIN);
  uint8_t dt = digitalRead(ENC_DT_PIN);
  uint8_t cur = (clk << 1) | dt;
  uint8_t transition = (encPrevState << 2) | cur;
  // Full-quadrature decode table
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
// ADC AVERAGING
// =====================================================
int readADC() {
  long sum = 0;

  for (int i = 0; i < 10; i++) {
    sum += analogRead(THERM_PIN);
  }

  return sum / 10;
}

// =====================================================
// TEMPERATURE
// =====================================================
float readTempC() {
  int adc = readADC();

  if (adcFiltered == 0)
    adcFiltered = adc;

  adcFiltered = 0.8 * adcFiltered + 0.2 * adc;

  if (adcFiltered <= 15 || adcFiltered >= 4080)
    return -100;

  float resistance =
    SERIES_RESISTOR * adcFiltered / (4095.0 - adcFiltered);

  float tempK =
    1.0 /
    (log(resistance / NOMINAL_RESISTANCE) / BETA_COEFFICIENT +
     1.0 / (NOMINAL_TEMPERATURE + 273.15));

  return tempK - 273.15;
}

// =====================================================
// SAFETY FAULT HELPERS
// =====================================================
void tripThermalFault(const String& reason) {
  thermalFault = true;
  faultReason = reason;
  heaterEnabled = false;
  pwmOutput = 0;
  integral = 0;
  ledcWrite(RPWM_PIN, 0);
  ledcWrite(LPWM_PIN, 0);
  currentStep = 0;
  Serial.print("SAFETY FAULT: ");
  Serial.println(reason);
}

bool resetFault() {
  if (currentTemp >= MIN_SAFE_TEMP && currentTemp <= MAX_SAFE_TEMP) {
    thermalFault = false;
    faultReason = "";
    integral = 0;
    return true;
  }
  return false;
}

// =====================================================
// STEPPER OUTPUT (hardware LEDC pulse generator)
// =====================================================
void updateStepperOutput() {
  if (motorRunning) {
    float motorRPM = rpm * GEAR_RATIO; // convert wheel RPM -> motor RPM
    float stepFreq = (motorRPM / 60.0) * stepsPerRevolution; // pulses per second
    ledcWriteTone(STEP_PIN, (uint32_t)stepFreq);
  } else {
    ledcWriteTone(STEP_PIN, 0); // stop the pulse train
  }
}

// =====================================================
// PROCESS STEP HELPERS
// =====================================================
void startStep1() {
  if (thermalFault) return;
  currentStep = 1;

  setpoint = step1Setpoint;
  integral = 0;

  heaterEnabled = true;
}

void stopStep1() {
  if (currentStep == 1) {
    currentStep = 0;
  }

  heaterEnabled = false;
  pwmOutput = 0;
  integral = 0;
  ledcWrite(RPWM_PIN, 0);
}

void startStep2() {
  if (thermalFault) return;
  currentStep = 2;

  setpoint = step2Setpoint;
  integral = 0;

  heaterEnabled = true;
}

void stopStep2() {
  if (currentStep == 2) {
    currentStep = 0;
  }

  heaterEnabled = false;
  pwmOutput = 0;
  integral = 0;
  ledcWrite(RPWM_PIN, 0);
}

void startStep3() {
  currentStep = 3;

  motorRunning = true;
  updateStepperOutput();
}

void stopStep3() {
  motorRunning = false;
  updateStepperOutput();

  if (currentStep == 3) {
    currentStep = heaterEnabled ? 2 : 0;
  }
}

void stopAll() {
  currentStep = 0;
  heaterEnabled = false;
  motorRunning = false;
  pwmOutput = 0;
  integral = 0;
  ledcWrite(RPWM_PIN, 0);
  updateStepperOutput();
}

void toggleHeater() {
  if (thermalFault) {
    heaterEnabled = false;
    return;
  }
  heaterEnabled = !heaterEnabled;
  if (!heaterEnabled) {
    pwmOutput = 0;
    integral = 0;
    ledcWrite(RPWM_PIN, 0);
  } else {
    integral = 0;
  }
}

void toggleMotor() {
  motorRunning = !motorRunning;
  updateStepperOutput();
}

// =====================================================
// LCD HELPERS
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

bool isStable() {
  return abs(setpoint - currentTemp) <= STABLE_BAND;
}

String menuLabel(int idx) {
  switch (idx) {
    case 0: return String(currentStep == 1 ? "Stop S1 " : "Start S1 ") + String(step1Setpoint, 0) + "C";
    case 1: return String(currentStep == 2 ? "Stop S2 " : "Start S2 ") + String(step2Setpoint, 0) + "C";
    case 2: return String(motorRunning ? "Stop STEPPER" : "Start STEPPER");
    case 3: return "Set S1SP:" + String(step1Setpoint, 0) + "C";
    case 4: return "Set S2SP:" + String(step2Setpoint, 0) + "C";
    case 5: return "Set RPM:" + String(rpm, 1);
    case 6: return String("Heater:") + String(heaterEnabled ? "ON" : "OFF");
    case 7: return String("Motor:") + String(motorRunning ? "ON" : "OFF");
    case 8: return "STOP ALL";
    case 9: return "IP:" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "192.168.1.1");
    default: return "";
  }
}

void lcdShowStatus() {
  if (thermalFault) {
    lcdLine(0, "! SAFETY FAULT !");
    lcdLine(1, faultReason.substring(0, 20));
    lcdLine(2, "T:" + (currentTemp <= -50 ? "ERR" : String(currentTemp, 1)) + "C PWM:0");
    lcdLine(3, "Hold btn to reset");
    return;
  }

  String suffix;
  if (!heaterEnabled) suffix = " OFF";
  else if (isStable()) suffix = " OK";
  else suffix = " HEAT";

  String l0 = "T:" + String(currentTemp, 1) + "->" + String(setpoint, 1);
  // ensure suffix fits in 20 chars
  if (l0.length() + suffix.length() > 20) l0 = l0.substring(0, 20 - suffix.length());
  l0 += suffix;

  String l1 = "PWM:" + String(pwmOutput) + " H:" + String(heaterEnabled ? "ON" : "OFF") + " M:" + String(motorRunning ? "ON" : "OFF");
  String l2 = "St" + String(currentStep) + " S1:" + String(step1Setpoint, 0) + " S2:" + String(step2Setpoint, 0);
  String l3 = "RPM:" + String(rpm, 1) + " clk=menu";

  lcdLine(0, l0);
  lcdLine(1, l1);
  lcdLine(2, l2);
  lcdLine(3, l3);
}

void lcdShowMenu() {
  // Show 4 items window containing menuIndex
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
  if (!force && (now - lastLcdUpdate < LCD_INTERVAL_MS)) return;
  lastLcdUpdate = now;

  if (uiMode == UI_STATUS) lcdShowStatus();
  else if (uiMode == UI_MENU) lcdShowMenu();
  else if (uiMode == UI_EDIT) lcdShowEdit();
}

void lcdForce() {
  lastLcdUpdate = 0;
  lcdRefresh(true);
}

// =====================================================
// ENCODER + BUTTON LOGIC
// =====================================================
void enterEdit(int t) {
  editTarget = (EditTarget)t;
  if (t == EDIT_S1SP) editValue = step1Setpoint;
  else if (t == EDIT_S2SP) editValue = step2Setpoint;
  else if (t == EDIT_RPM) editValue = rpm;
  uiMode = UI_EDIT;
  lcd.clear();
  lcdForce();
}

void saveEditAndToMenu() {
  if (editTarget == EDIT_S1SP) {
    step1Setpoint = editValue;
    prefs.putFloat("step1sp", step1Setpoint);
    if (currentStep == 1) { setpoint = step1Setpoint; integral = 0; }
  } else if (editTarget == EDIT_S2SP) {
    step2Setpoint = editValue;
    prefs.putFloat("step2sp", step2Setpoint);
    if (currentStep == 2) { setpoint = step2Setpoint; integral = 0; }
  } else if (editTarget == EDIT_RPM) {
    rpm = editValue;
    if (rpm < 0.1) rpm = 0.1;
    prefs.putFloat("rpm", rpm);
    updateStepperOutput();
  }
  editTarget = EDIT_NONE;
  uiMode = UI_MENU;
  lcd.clear();
  lcdForce();
}

void applyEditLive() {
  // Live preview without prefs write (prefs written on save)
  if (editTarget == EDIT_S1SP) {
    step1Setpoint = editValue;
    if (currentStep == 1) { setpoint = step1Setpoint; }
  } else if (editTarget == EDIT_S2SP) {
    step2Setpoint = editValue;
    if (currentStep == 2) { setpoint = step2Setpoint; }
  } else if (editTarget == EDIT_RPM) {
    float v = editValue;
    if (v < 0.1) v = 0.1;
    rpm = v;
    updateStepperOutput();
  }
}

void onEncoderTurn(int dir) {
  if (verbose) {
    Serial.print("ENC turn dir=");
    Serial.println(dir);
  }
  if (uiMode == UI_STATUS) {
    // Rotating from status jumps straight into menu so it never feels dead
    uiMode = UI_MENU;
    lcd.clear();
    // fall through to move selection as well
  }
  if (uiMode == UI_MENU) {
    menuIndex += dir;
    if (menuIndex < 0) menuIndex = MENU_COUNT - 1;
    if (menuIndex >= MENU_COUNT) menuIndex = 0;
    lcdForce();
  } else if (uiMode == UI_EDIT) {
    if (editTarget == EDIT_S1SP || editTarget == EDIT_S2SP) {
      editValue += dir * 1.0;
      editValue = constrain(editValue, 0, 300);
    } else if (editTarget == EDIT_RPM) {
      editValue += dir * 0.5;
      editValue = constrain(editValue, 0.1, 60);
    }
    applyEditLive();
    lcdForce();
  }
}

void activateMenuItem(int idx) {
  switch (idx) {
    case 0:
      if (currentStep == 1) stopStep1(); else startStep1();
      break;
    case 1:
      if (currentStep == 2) stopStep2(); else startStep2();
      break;
    case 2:
      if (motorRunning) stopStep3(); else startStep3();
      break;
    case 3:
      enterEdit(EDIT_S1SP);
      return;
    case 4:
      enterEdit(EDIT_S2SP);
      return;
    case 5:
      enterEdit(EDIT_RPM);
      return;
    case 6:
      toggleHeater();
      break;
    case 7:
      toggleMotor();
      break;
    case 8:
      stopAll();
      uiMode = UI_STATUS;
      lcd.clear();
      lcdForce();
      return;
  }
  lcdForce();
}

void onButtonShort() {
  if (verbose) Serial.println("BTN short press");
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
  if (verbose) Serial.println("BTN long press -> status");
  if (thermalFault) {
    resetFault();
  }
  // Long press always goes back to status.
  // If editing, save first so live preview isn't lost.
  if (uiMode == UI_EDIT) {
    saveEditAndToMenu();
  }
  uiMode = UI_STATUS;
  editTarget = EDIT_NONE;
  lcd.clear();
  lcdForce();
}

void pollEncoder() {
  // Consume steps accumulated by ISRs. Divide by 2 because most KY-040
  // modules give 2 full-quadrature counts per detent; this makes
  // 1 click = 1 menu move. If yours feels half-speed, change /2 to /1,
  // if double-speed, change to /4.
  long steps = 0;
  noInterrupts();
  steps = encDelta;
  encDelta = 0;
  interrupts();

  // KY-040: 2 counts per click -> accumulate remainder
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
// SERIAL COMMANDS
// =====================================================
void handleSerial() {
  if (!Serial.available())
    return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  // ===== PID =====

  if (cmd.startsWith("T")) {
    setpoint = cmd.substring(1).toFloat();

    integral = 0;

    Serial.print("Setpoint = ");
    Serial.println(setpoint);
  }

  else if (cmd.startsWith("P")) {
    Kp = cmd.substring(1).toFloat();

    Serial.print("Kp = ");
    Serial.println(Kp);
  }

  else if (cmd.startsWith("I")) {
    Ki = cmd.substring(1).toFloat();

    Serial.print("Ki = ");
    Serial.println(Ki);
  }

  else if (cmd.startsWith("D")) {
    Kd = cmd.substring(1).toFloat();

    Serial.print("Kd = ");
    Serial.println(Kd);
  }

  else if (cmd == "R" || cmd == "reset") {
    integral = 0;
    if (resetFault()) {
      Serial.println("Fault reset OK");
    } else if (thermalFault) {
      Serial.println("Cannot reset: Temp out of safe range");
    } else {
      Serial.println("PID Reset");
    }
  }

  // ===== Stepper =====

  else if (cmd.startsWith("S")) {
    rpm = cmd.substring(1).toFloat();

    if (rpm < 0.1)
      rpm = 0.1;

    updateStepperOutput();

    Serial.print("RPM = ");
    Serial.println(rpm);
  }

  else if (cmd == "s") {
    motorRunning = true;
    updateStepperOutput();

    Serial.println("Motor started");
  }

  else if (cmd == "f") {
    motorRunning = false;
    updateStepperOutput();

    Serial.println("Motor stopped");
  }

  else if (cmd == "d") {
    direction = !direction;

    digitalWrite(DIR_PIN, direction);

    Serial.println("Direction changed");
  }

  // ===== Process Steps =====

  else if (cmd == "1") {
    startStep1();
    Serial.println("Step 1 started");
  }

  else if (cmd == "!1") {
    stopStep1();
    Serial.println("Step 1 stopped");
  }

  else if (cmd == "2") {
    startStep2();
    Serial.println("Step 2 started");
  }

  else if (cmd == "!2") {
    stopStep2();
    Serial.println("Step 2 stopped");
  }

  else if (cmd == "3") {
    startStep3();
    Serial.println("Step 3 started");
  }

  else if (cmd == "!3") {
    stopStep3();
    Serial.println("Step 3 stopped");
  }

  else if (cmd == "e") {
    // Encoder raw-pin diagnostic: rotate / press, should see changes
    // NOTE: at rest 1,1,1 is NORMAL. You must rotate WHILE sampling.
    // Use E (capital) for a 5-second watch window instead.
    Serial.print("CLK=");
    Serial.print(digitalRead(ENC_CLK_PIN));
    Serial.print(" DT=");
    Serial.print(digitalRead(ENC_DT_PIN));
    Serial.print(" SW=");
    Serial.print(digitalRead(ENC_SW_PIN));
    Serial.print(" delta=");
    Serial.println(encDelta);
  }

  else if (cmd == "E") {
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
      delay(1);
    }
    long endDelta;
    noInterrupts(); endDelta = encDelta; interrupts();
    Serial.print("RESULT CLK edges=");
    Serial.print(cEdges);
    Serial.print(" DT edges=");
    Serial.print(dEdges);
    Serial.print(" SW edges=");
    Serial.print(sEdges);
    Serial.print(" ISR delta=");
    Serial.println(endDelta - startDelta);
    if (cEdges == 0 && dEdges == 0) {
      Serial.println("DIAG: no edges = wiring/power/wrong-pins. Check 3V3+GND, CLK->11 DT->12.");
      Serial.println("DIAG: on many S3 boards D11 != GPIO11. Confirm GPIO numbers.");
    } else if ((endDelta - startDelta) == 0) {
      Serial.println("DIAG: pins move but ISR delta=0 = interrupt not attached. Tell me board model.");
    } else {
      Serial.println("DIAG: encoder OK. If menu doesn't move, it's UI logic.");
    }
  }

  else if (cmd == "m") {
    verbose = !verbose;
    prefs.putBool("verbose", verbose);
    Serial.println(verbose ? "log ON" : "log OFF");
  }

  else if (cmd == "v1" || cmd == "V1") {
    verbose = true;
    prefs.putBool("verbose", verbose);
    Serial.println("log ON");
  }

  else if (cmd == "v0" || cmd == "V0") {
    verbose = false;
    prefs.putBool("verbose", verbose);
    Serial.println("log OFF");
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);

  prefs.begin("settings", false);

  setpoint = prefs.getFloat("setpoint", 210.0);
  rpm = prefs.getFloat("rpm", 15.0);
  step1Setpoint = prefs.getFloat("step1sp", 100.0);
  step2Setpoint = prefs.getFloat("step2sp", 200.0);
  verbose = prefs.getBool("verbose", false);


  // BTS7960
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH);

  ledcAttach(RPWM_PIN, 1000, 8);
  ledcAttach(LPWM_PIN, 1000, 8);

  ledcWrite(LPWM_PIN, 0);

  // Thermistor ADC
  analogReadResolution(12);

  // Stepper
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(DIR_PIN, direction);

  // NOTE: this frequency must differ from RPWM_PIN/LPWM_PIN's (1000Hz) or the
  // LEDC driver will merge STEP_PIN onto the same hardware timer as the heater
  // outputs, and changing stepper speed later will also disturb the heater PWM.
  ledcAttach(STEP_PIN, 800, 8);    // STEP_PIN pulses are generated by hardware, not the loop
  updateStepperOutput();           // starts stopped (motorRunning = false)

  // LCD + Encoder
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UPCYCLE Controller");
  lcd.setCursor(0, 1);
  lcd.print("LCD+Encoder Ready");

  pinMode(ENC_CLK_PIN, INPUT_PULLUP);
  pinMode(ENC_DT_PIN, INPUT_PULLUP);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);
  encPrevState = (digitalRead(ENC_CLK_PIN) << 1) | digitalRead(ENC_DT_PIN);
  encDelta = 0;
  attachInterrupt(digitalPinToInterrupt(ENC_CLK_PIN), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT_PIN), encISR, CHANGE);

  // WIFI (AP + STA + Web UI + OTA + mDNS)
  WiFi.mode(WIFI_AP_STA);

  WiFi.softAPConfig(
    AP_IP,
    AP_GATEWAY,
    AP_SUBNET
  );

  WiFi.softAP(
    ap_ssid,
    ap_pass
  );

  if (strlen(sta_ssid) > 0 && strcmp(sta_ssid, "YOUR_WIFI_SSID") != 0) {
    WiFi.begin(sta_ssid, sta_pass);
    Serial.print("Connecting to LAN Wi-Fi: ");
    Serial.println(sta_ssid);
  }

  if (MDNS.begin(mdns_hostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS active: http://%s.local\n", mdns_hostname);
  }

  server.on("/", []() { server.send(200, "text/html", webpage); });
  server.on("/status", []() { server.send(200, "application/json", buildStatus()); });
  server.on("/resetfault", []() {
    if (resetFault()) {
      lcdForce();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Cannot reset: temperature out of range");
    }
  });
  server.on("/heater", []() {
    if (thermalFault && server.arg("state") == "on") {
      server.send(400, "text/plain", "Thermal Fault Active");
      return;
    }
    heaterEnabled = (server.arg("state") == "on");
    if (!heaterEnabled) { pwmOutput = 0; integral = 0; ledcWrite(RPWM_PIN, 0); }
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/stepper", []() {
    motorRunning = (server.arg("state") == "on");
    updateStepperOutput();
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step1", []() {
    if (server.arg("state") == "on") startStep1(); else stopStep1();
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step2", []() {
    if (server.arg("state") == "on") startStep2(); else stopStep2();
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step3", []() {
    if (server.arg("state") == "on") startStep3(); else stopStep3();
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step1setpoint", []() {
    step1Setpoint = server.arg("value").toFloat();
    prefs.putFloat("step1sp", step1Setpoint);
    if (currentStep == 1) { setpoint = step1Setpoint; integral = 0; }
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/step2setpoint", []() {
    step2Setpoint = server.arg("value").toFloat();
    prefs.putFloat("step2sp", step2Setpoint);
    if (currentStep == 2) { setpoint = step2Setpoint; integral = 0; }
    lcdForce();
    server.send(200, "text/plain", "OK");
  });
  server.on("/rpm", []() {
    rpm = server.arg("value").toFloat();
    if (rpm < 0.1) rpm = 0.1;
    prefs.putFloat("rpm", rpm);
    updateStepperOutput();
    lcdForce();
    server.send(200, "text/plain", "OK");
  });

  // OTA — open http://192.168.1.1/update after joining UPCYCLE AP
  ElegantOTA.begin(&server);
  server.begin();

  Serial.println("=== PID Heater + Stepper Ready (LCD+Encoder) ===");

  Serial.println("Temperature:");
  Serial.println("T200");
  Serial.println("P19");
  Serial.println("I0.39");
  Serial.println("D1.5");
  Serial.println("R");

  Serial.println("Stepper:");
  Serial.println("S15  (wheel RPM, not motor RPM)");
  Serial.println("s");
  Serial.println("f");
  Serial.println("d");

  Serial.println("Process Steps:");
  Serial.println("1  -> start Step 1 (heat to setpoint 1)");
  Serial.println("!1 -> stop Step 1");
  Serial.println("2  -> start Step 2 (heat to setpoint 2)");
  Serial.println("!2 -> stop Step 2");
  Serial.println("3  -> start Step 3 (stepper on)");
  Serial.println("!3 -> stop Step 3");

  Serial.println("LCD: click=menu/select, rotate=navigate/change, hold=back");
  Serial.println("OTA: http://192.168.1.1/update");
  Serial.println("Log: m(toggle) v0(off) v1(on) e(enc diag)");
  if (!verbose) Serial.println("log OFF (send v1 to enable)");

  delay(1000);
  lcd.clear();
  lastLcdUpdate = 0;
  lcdRefresh(true);

  lastPID = millis();
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  server.handleClient();
  ElegantOTA.loop();
  handleSerial();

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

  pollEncoder();
  pollButton();
  lcdRefresh(false);

  // ================= PID LOOP =================
  unsigned long now = millis();

  if (now - lastPID >= 100) {

    float dt = (now - lastPID) / 1000.0;
    lastPID = now;

    int adc = readADC();

    currentTemp = readTempC();
    float temp = currentTemp;

    // Safety check 1: Range limits (Sensor disconnect, short, or extreme overtemp)
    if (temp < MIN_SAFE_TEMP || temp > MAX_SAFE_TEMP) {
      if (!thermalFault) {
        tripThermalFault(temp < MIN_SAFE_TEMP ? "SENSOR DISCONNECTED" : "OVERTEMP HAZARD");
      }
    }

    if (thermalFault) {
      heaterEnabled = false;
      pwmOutput = 0;
      integral = 0;
      ledcWrite(RPWM_PIN, 0);
    } else {
      // Safety check 2: Thermal runaway watchdog
      // If heater is on, temp is >8C below setpoint, and PWM is high:
      // verify temperature rises by at least 2C every 40 seconds.
      static unsigned long runawayCheckTime = 0;
      static float runawayStartTemp = 0;
      static bool runawayTracking = false;

      if (heaterEnabled && (setpoint - temp > 8.0) && (pwmOutput > 150)) {
        if (!runawayTracking) {
          runawayTracking = true;
          runawayCheckTime = now;
          runawayStartTemp = temp;
        } else if (now - runawayCheckTime >= 40000) {
          if (temp - runawayStartTemp < 2.0) {
            tripThermalFault("THERMAL RUNAWAY");
          } else {
            runawayCheckTime = now;
            runawayStartTemp = temp;
          }
        }
      } else {
        runawayTracking = false;
      }

      float error = setpoint - temp;

      if (abs(error) < 30) {
        integral += error * dt;
      }

      integral = constrain(integral, -300, 300);

      float derivative = -(temp - lastTemp) / dt;

      lastTemp = temp;

      float Pterm = Kp * error;
      float Iterm = Ki * integral;
      float Dterm = Kd * derivative;

      float output = Pterm + Iterm + Dterm;

      pwmOutput = constrain(output, 0, 255);

      if (heaterEnabled)
      {
        ledcWrite(RPWM_PIN, pwmOutput);
      }
      else
      {
        pwmOutput = 0;
        integral = 0;          // optional but recommended
        ledcWrite(RPWM_PIN, 0);
      }
    }

    // Debug (gated by verbose flag: m / v0 / v1)
    if (verbose) {
      Serial.print("Temp:");
      Serial.print(temp, 1);

      Serial.print(" | Set:");
      Serial.print(setpoint, 1);

      Serial.print(" | Step:");
      Serial.print(currentStep);

      Serial.print(" | PWM:");
      Serial.print(pwmOutput);

      Serial.print(" | Error:");
      Serial.print(error);

      Serial.print(" | RPM:");
      Serial.print(rpm);

      Serial.print(" | Kp:");
      Serial.print(Kp);

      Serial.print(" | Ki:");
      Serial.print(Ki);

      Serial.print(" | Kd:");
      Serial.print(Kd);

      Serial.println();
    }
  }

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint > 1000) {
    lastPrint = millis();

    if (verbose) {
      Serial.print("RPM = ");
      Serial.println(rpm);
    }
  }

}
