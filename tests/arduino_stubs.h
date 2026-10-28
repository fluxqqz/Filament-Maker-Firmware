#pragma once

// Host-only hardware doubles. firmware_test.cpp includes the actual sketch.
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

using std::abs;
using std::isfinite;

class String {
  std::string text;
public:
  String() = default;
  String(const char* s) : text(s) {}
  String(const std::string& s) : text(s) {}
  String(char c) : text(1, c) {}
  template<class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
  String(T n) : text(std::to_string(n)) {}
  String(double n, int decimals = 2) {
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, n);
    text = buffer;
  }
  unsigned int length() const { return (unsigned int)text.size(); }
  const char* c_str() const { return text.c_str(); }
  void trim() {
    auto start = text.find_first_not_of(" \t\r\n");
    text = start == std::string::npos ? "" : text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
  }
  String substring(unsigned int start, unsigned int end = ~0u) const {
    return start >= text.size() ? String() : String(text.substr(start, end - start));
  }
  bool startsWith(const char* prefix) const { return text.rfind(prefix, 0) == 0; }
  void remove(unsigned int start) { text.erase(start); }
  void toCharArray(char* out, size_t size) const { std::snprintf(out, size, "%s", c_str()); }
  char& operator[](unsigned int i) { return text[i]; }
  char operator[](unsigned int i) const { return text[i]; }
  String& operator+=(const String& other) { text += other.text; return *this; }
  friend String operator+(const String& a, const String& b) { return a.text + b.text; }
  friend bool operator==(const String& a, const String& b) { return a.text == b.text; }
  friend bool operator!=(const String& a, const String& b) { return !(a == b); }
};

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define CHANGE 3
#define IRAM_ATTR
#define WIFI_AP_STA 3
#define HTTP_POST 1
#define WIFI_STA_SSID ""
#define WIFI_STA_PASSWORD ""
#define WIFI_AP_PASSWORD "test-ap-only"
#define constrain(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))

uint32_t fakeMillis = 0;
uint32_t millis() { return fakeMillis; }
void delay(unsigned long ms) { fakeMillis += ms; }
void noInterrupts() {}
void interrupts() {}
void pinMode(int, int) {}
void analogReadResolution(int) {}
void attachInterrupt(int, void (*)(), int) {}
int digitalPinToInterrupt(int pin) { return pin; }
int analogValue = 3723;
int analogRead(int) { return analogValue; }
int digitalRead(int) { return HIGH; }
int pinValues[50] = {};
uint32_t pwmValues[50] = {};
uint32_t toneValues[50] = {};
void digitalWrite(int pin, int value) { pinValues[pin] = value; }
bool ledcAttach(int, int, int) { return true; }
bool ledcWrite(int pin, uint32_t value) { pwmValues[pin] = value; return true; }
uint32_t ledcWriteTone(int pin, uint32_t value) { toneValues[pin] = value; return value; }

int lockDepth = 0;
using SemaphoreHandle_t = int*;
SemaphoreHandle_t xSemaphoreCreateMutex() { static int mutex; return &mutex; }
constexpr int portMAX_DELAY = -1;
void xSemaphoreTake(SemaphoreHandle_t mutex, int) { assert(mutex && lockDepth == 0); ++lockDepth; }
void xSemaphoreGive(SemaphoreHandle_t) { assert(lockDepth == 1); --lockDepth; }
using TickType_t = uint32_t;
TickType_t xTaskGetTickCount() { return fakeMillis; }
TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
void vTaskDelay(uint32_t ms) { fakeMillis += ms; }
struct EndTask {};
int thermalCycles = 0;
void vTaskDelayUntil(TickType_t* tick, TickType_t period) {
  if (thermalCycles-- <= 0) throw EndTask{};
  *tick += period;
  fakeMillis = *tick;
}
void xTaskCreatePinnedToCore(void (*)(void*), const char*, int, void*, int, void*, int) {}

struct FakeSerial {
  String input;
  std::string output;
  void begin(int) {}
  void setTimeout(int) {}
  int available() { return input.length(); }
  String readStringUntil(char) { String command = input; input = ""; return command; }
  template<class T> void print(T value) { output += String(value).c_str(); }
  void print(double value, int decimals) { output += String(value, decimals).c_str(); }
  template<class T> void println(T value) { print(value); output += '\n'; }
  void println() { output += '\n'; }
  void printf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    output += buffer;
  }
} Serial;

struct Preferences {
  std::map<std::string, std::vector<unsigned char>> bytes;
  bool failWrites = false;
  int writes = 0;
  bool begin(const char*, bool) { return true; }
  float getFloat(const char*, float fallback) { return fallback; }
  bool getBool(const char*, bool fallback) { return fallback; }
  size_t putFloat(const char*, float) { assert(lockDepth == 0); return sizeof(float); }
  size_t putBool(const char*, bool) { assert(lockDepth == 0); return sizeof(bool); }
  size_t getBytesLength(const char* key) { return bytes[key].size(); }
  size_t getBytes(const char* key, void* out, size_t size) {
    size_t length = std::min(size, bytes[key].size());
    if (length) std::memcpy(out, bytes[key].data(), length);
    return length;
  }
  size_t putBytes(const char* key, const void* data, size_t size) {
    assert(lockDepth == 0);
    if (failWrites) return 0;
    const auto* start = (const unsigned char*)data;
    bytes[key] = {start, start + size};
    ++writes;
    return size;
  }
};

struct IPAddress {
  String value;
  IPAddress(int a, int b, int c, int d) : value(String(a) + "." + String(b) + "." + String(c) + "." + String(d)) {}
  String toString() const { return value; }
  operator String() const { return value; }
};
enum wifi_auth_mode_t { WIFI_AUTH_OPEN, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK,
  WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_WPA2_ENTERPRISE };
constexpr int WL_CONNECTED = 3;
constexpr int WIFI_SCAN_RUNNING = -1;
constexpr int WIFI_SCAN_FAILED = -2;
struct ScanItem { String name; int32_t rssi; wifi_auth_mode_t auth; };
struct FakeWiFi {
  int connection = 0;
  int scanState = WIFI_SCAN_FAILED;
  bool apEnabled = false;
  bool autoReconnect = true;
  String targetSSID, targetPassword, connectedSSID;
  std::vector<ScanItem> results;
  void mode(int) {}
  void persistent(bool) {}
  void softAPConfig(IPAddress, IPAddress, IPAddress) {}
  void softAP(const char*, const char*) { apEnabled = true; }
  void setAutoReconnect(bool enabled) { autoReconnect = enabled; }
  void disconnect(bool, bool) { connection = 0; connectedSSID = ""; }
  void begin(const char* ssid, const char* password) { targetSSID = ssid; targetPassword = password; }
  int status() { return connection; }
  String SSID() { return connectedSSID; }
  String SSID(uint8_t index) { return results.at(index).name; }
  int32_t RSSI(uint8_t index) { return results.at(index).rssi; }
  wifi_auth_mode_t encryptionType(uint8_t index) { return results.at(index).auth; }
  void scanNetworks(bool async) { assert(async && lockDepth == 0); scanState = WIFI_SCAN_RUNNING; }
  int scanComplete() { return scanState; }
  void scanDelete() { results.clear(); scanState = WIFI_SCAN_FAILED; }
  IPAddress localIP() { return {192, 168, 1, 123}; }
  void connected() { connection = WL_CONNECTED; connectedSSID = targetSSID; }
} WiFi;
void esp_wifi_scan_stop() { WiFi.scanState = WIFI_SCAN_FAILED; }

struct LiquidCrystal_I2C {
  std::string rows[4];
  int row = 0, column = 0;
  LiquidCrystal_I2C(int, int, int) { clear(); }
  void init() {}
  void backlight() {}
  void clear() { for (auto& text : rows) text.assign(20, ' '); }
  void setCursor(int col, int r) { assert(r >= 0 && r < 4); row = r; column = col; }
  void print(const String& value) {
    assert(lockDepth == 0);
    assert(value.length() <= 20 && column + value.length() <= 20);
    rows[row].replace(column, value.length(), value.c_str());
  }
};
struct { void begin(int, int) {} } Wire;
struct {
  bool begin(const char*) { return true; }
  void addService(const char*, const char*, int) {}
} MDNS;
struct WebServer {
  std::map<std::string, std::function<void()>> routes;
  std::map<std::string, String> arguments;
  int responseCode = 0;
  WebServer(int) {}
  template<class F> void on(const char* path, F handler) { routes[path] = handler; }
  template<class F> void on(const char* path, int, F handler) { routes[path] = handler; }
  String arg(const char* name) { return arguments[name]; }
  void send(int code, const char*, const String&) { assert(lockDepth == 0); responseCode = code; }
  void begin() {}
  void handleClient() {}
};
struct {
  void begin(WebServer*) {}
  void loop() {}
} ElegantOTA;
const char webpage[] = "host-test";
