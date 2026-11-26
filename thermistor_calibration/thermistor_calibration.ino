// Thermistor calibration sketch for the Mountoya filament maker (ESP32-S3).
// Flash this in place of filament_maker.ino, take reference points from the web page,
// paste the fitted constants into filament_maker.ino, then flash the main firmware back.
//
// It reads the thermistor exactly the way filament_maker.ino does (10-sample average, same
// 0.8/0.2 filter, same divider formula), so the fitted constants also absorb ESP32 ADC error.
#include <math.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Preferences.h>
#include "calibration_dashboard.h"

// ---- Keep in sync with filament_maker.ino ----
const int THERM_PIN = 2;
const int ENABLE_PIN = 7;
const int RPWM_PIN = 6;
const int LPWM_PIN = 5;
const float SERIES_RESISTOR = 10000.0;
const float NOMINAL_RESISTANCE = 100000.0;
const float NOMINAL_TEMPERATURE = 25.0;
const float BETA_COEFFICIENT = 3950.0;
const float MAX_SAFE_TEMP = 280.0;
// ----------------------------------------------

const unsigned long WATCHDOG_MS = 10000; // Heater off if the dashboard stops polling (tab closed, Wi-Fi lost).

const char* AP_SSID = "Filament-Calibrate";
const char* AP_PASS = "calibrate"; // WPA2 needs at least 8 characters.
const char* MDNS_NAME = "filament-calibrate";

WebServer server(80);

float adcFiltered = 0;
int heaterPwm = 0;          // 0-255, set from the dashboard
String fault = "";          // Non-empty = heater locked off until reset
unsigned long lastRequest = 0;

int readADC() {
  long sum = 0;
  for (int i = 0; i < 10; i++) sum += analogRead(THERM_PIN);
  return sum / 10;
}

void sampleSensor() {
  int adc = readADC();
  if (adcFiltered == 0) adcFiltered = adc;
  adcFiltered = 0.8 * adcFiltered + 0.2 * adc;
}

bool sensorOk() { return adcFiltered > 15 && adcFiltered < 4080; }
float resistanceOhms() { return SERIES_RESISTOR * adcFiltered / (4095.0 - adcFiltered); }
float betaTempC(float r) {
  return 1.0 / (log(r / NOMINAL_RESISTANCE) / BETA_COEFFICIENT + 1.0 / (NOMINAL_TEMPERATURE + 273.15)) - 273.15;
}

void applyHeater() {
  bool on = heaterPwm > 0 && fault.length() == 0;
  digitalWrite(ENABLE_PIN, on ? HIGH : LOW);
  ledcWrite(RPWM_PIN, on ? heaterPwm : 0);
  ledcWrite(LPWM_PIN, 0);
}

void updateSafety(unsigned long now) {
  if (fault.length() == 0) {
    if (!sensorOk()) fault = "SENSOR OPEN OR SHORTED";
    else if (betaTempC(resistanceOhms()) > MAX_SAFE_TEMP) fault = "OVERTEMP";
  }
  if (fault.length()) heaterPwm = 0;
  if (heaterPwm > 0 && now - lastRequest > WATCHDOG_MS) {
    heaterPwm = 0;
    Serial.println("Dashboard silent: heater turned off");
  }
  applyHeater();
}

void handleStatus() {
  lastRequest = millis();
  bool ok = sensorOk();
  float r = resistanceOhms();
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"adc\":%.2f,\"ok\":%s,\"r\":%s,\"t\":%s,\"pwm\":%d,\"fault\":\"%s\","
    "\"series\":%.1f,\"r25\":%.1f,\"t25\":%.2f,\"beta\":%.1f,\"maxSafe\":%.1f}",
    adcFiltered, ok ? "true" : "false",
    ok ? String(r, 2).c_str() : "null",
    ok ? String(betaTempC(r), 2).c_str() : "null",
    heaterPwm, fault.c_str(),
    SERIES_RESISTOR, NOMINAL_RESISTANCE, NOMINAL_TEMPERATURE, BETA_COEFFICIENT, MAX_SAFE_TEMP);
  server.send(200, "application/json", buf);
}

void handleHeater() {
  lastRequest = millis();
  String s = server.arg("pwm");
  char* end = nullptr;
  long v = strtol(s.c_str(), &end, 10);
  if (s.length() == 0 || *end != '\0' || v < 0 || v > 255) {
    server.send(400, "text/plain", "Invalid pwm: expected 0-255");
    return;
  }
  if (v > 0 && fault.length()) {
    server.send(409, "text/plain", "Fault active: " + fault);
    return;
  }
  heaterPwm = v;
  applyHeater();
  server.send(200, "text/plain", "OK");
}

void handleReset() {
  if (!sensorOk() || betaTempC(resistanceOhms()) > MAX_SAFE_TEMP) {
    server.send(400, "text/plain", "Cannot reset: sensor reading still out of range");
    return;
  }
  fault = "";
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);

  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);
  ledcAttach(RPWM_PIN, 1000, 8);
  ledcAttach(LPWM_PIN, 1000, 8);
  applyHeater();
  analogReadResolution(12);

  WiFi.mode(WIFI_AP_STA);
  WiFi.persistent(false);
  WiFi.softAP(AP_SSID, AP_PASS);

  // Reuse the LAN credentials filament_maker.ino saved (same Preferences layout).
  struct { char ssid[33]; char password[65]; } saved = {};
  Preferences wifiPrefs;
  if (wifiPrefs.begin("wifi", true)) {
    if (wifiPrefs.getBytesLength("credentials") == sizeof(saved))
      wifiPrefs.getBytes("credentials", &saved, sizeof(saved));
    wifiPrefs.end();
  }
  saved.ssid[32] = '\0';
  saved.password[64] = '\0';
  if (saved.ssid[0]) {
    WiFi.begin(saved.ssid, saved.password);
    Serial.printf("Connecting to LAN Wi-Fi: %s\n", saved.ssid);
  }
  if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);

  server.on("/", []() { server.send(200, "text/html", calibrationPage); });
  server.on("/status", handleStatus);
  server.on("/heater", handleHeater);
  server.on("/reset", HTTP_POST, handleReset);
  server.begin();

  Serial.printf("Calibration AP: %s / %s -> http://%s\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
  Serial.printf("On LAN: http://%s.local\n", MDNS_NAME);
}

void loop() {
  server.handleClient();

  static unsigned long lastSample = 0, lastPrint = 0;
  unsigned long now = millis();
  if (now - lastSample >= 100) {
    lastSample = now;
    sampleSensor();
    updateSafety(now);
  }
  if (now - lastPrint >= 1000) {
    lastPrint = now;
    if (sensorOk()) {
      float r = resistanceOhms();
      Serial.printf("adc=%.1f R=%.1f ohm T=%.2f C pwm=%d %s\n", adcFiltered, r, betaTempC(r), heaterPwm, fault.c_str());
    } else {
      Serial.printf("adc=%.1f sensor out of range, pwm=%d %s\n", adcFiltered, heaterPwm, fault.c_str());
    }
    if (WiFi.status() == WL_CONNECTED && lastPrint < 20000)
      Serial.printf("LAN IP: %s\n", WiFi.localIP().toString().c_str());
  }
  delay(2);
}
