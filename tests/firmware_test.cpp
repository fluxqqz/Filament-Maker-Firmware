#define FILAMENT_HOST_TEST
#include "../filament_maker.ino"
#include <iostream>
#include <set>

void resetWifi() {
  wifiSetupState = WIFI_SETUP_IDLE;
  cancelWifiSetup();
  processWifiSetup(); // Drain any in-flight scan/connection before resetting doubles.
  WiFi = FakeWiFi{};
  WiFi.apEnabled = true;
  wifiPrefs = Preferences{};
  wifiStorageReady = true;
  savedWifi = WifiCredentials{};
  wifiNetworks.clear();
  uiMode = UI_STATUS;
  thermalFault = false;
  fakeMillis = 100;
}

void queueConnection(const char* ssid, const char* password) {
  wifiSelectedSSID = ssid;
  wifiSelectedSecure = password[0] != '\0';
  wifiPasswordDraft = password;
  requestWifiConnect();
  processWifiSetup();
}

void testRuntimeAndStatus() {
  fakeMillis = 1000;
  startStep3();
  fakeMillis += 12345;
  startStep3(); // Repeated ON and RPM commands must not restart the timer.
  fakeMillis += 1000;
  stopStep3();
  assert(motorElapsedMs == 13345);
  fakeMillis += 10000;
  lcdShowStatus();
  assert(lcd.rows[2].find("RPM:0.0 MOTOR:OFF") == 0);
  assert(lcd.rows[3].find("RUNTIME 00:00:13") == 0);
  startStep3();
  assert(motorElapsedMs == 0);
  currentTemp = 215.3f; setpoint = 220; pwmOutput = 166; heaterEnabled = true;
  fakeMillis += 754000;
  lcdShowStatus();
  assert(lcd.rows[0].find("TEMP:215.3/220.0C") == 0);
  assert(lcd.rows[1].find("HEAT:ON  PWM:65%") == 0);
  assert(lcd.rows[2].find("RPM:15.0 MOTOR:ON") == 0);
  assert(lcd.rows[3].find("RUNTIME 00:12:34") == 0);
  stopAll();
  fakeMillis = 0xfffffff0u;
  toggleMotor();
  fakeMillis = 0x20u;
  toggleMotor();
  assert(motorElapsedMs == 48); // millis() rollover.
  assert(toneValues[STEP_PIN] == 0);
}

void testPasswordEditor() {
  resetWifi();
  uiMode = UI_WIFI_PASSWORD;
  std::set<char> characters;
  for (int i = 3; i < WIFI_PASSWORD_CHOICES; ++i) {
    wifiPasswordChoice = i;
    selectWifiPasswordCharacter();
    characters.insert(WIFI_PASSWORD_CHARS[i - 3]);
    wifiPasswordChoice = 1;
    selectWifiPasswordCharacter();
    assert(wifiPasswordDraft.length() == 0);
  }
  assert(characters.size() == 95);
  for (int c = 32; c <= 126; ++c) assert(characters.count((char)c));
  wifiPasswordChoice = 0;
  onEncoderTurn(-1);
  assert(wifiPasswordChoice == WIFI_PASSWORD_CHOICES - 1);
  onEncoderTurn(1);
  assert(wifiPasswordChoice == 0);
  wifiSelectedSSID = "router"; wifiSelectedSecure = true;
  wifiPasswordDraft = "short";
  selectWifiPasswordCharacter();
  assert(uiMode == UI_WIFI_PASSWORD && wifiRequest == WREQ_NONE);
  assert(validWifiPassword("", false));
  assert(!validWifiPassword("password", false));
  assert(!validWifiPassword("1234567", true));
  assert(validWifiPassword("12345678", true));
  assert(validWifiPassword(std::string(63, 'z'), true));
  assert(validWifiPassword(std::string(64, 'a'), true));
  assert(!validWifiPassword(std::string(64, 'z'), true));
  wifiPasswordDraft = std::string(64, 'a');
  wifiPasswordChoice = 3;
  selectWifiPasswordCharacter();
  assert(wifiPasswordDraft.length() == 64);
  lcdShowWifiPassword();
  assert(lcd.rows[1] == "<*******************");
  assert(Serial.output.find(std::string(64, 'a')) == std::string::npos);
  onButtonLong();
  assert(uiMode == UI_STATUS && wifiPasswordDraft.length() == 0 && wifiRequest == WREQ_CANCEL);
}

void testScanAndSelection() {
  resetWifi();
  requestWifiScan();
  processWifiSetup();
  lcdShowWifiNetworks();
  assert(lcd.rows[0].find("Scanning") == 0);
  WiFi.results = {{"weak", -80, WIFI_AUTH_WPA2_PSK}, {"open", -30, WIFI_AUTH_OPEN},
    {"weak", -40, WIFI_AUTH_WPA2_PSK}, {"enterprise", -50, WIFI_AUTH_WPA2_ENTERPRISE}};
  WiFi.scanState = (int)WiFi.results.size();
  processWifiSetup();
  assert(wifiSetupState == WIFI_SETUP_READY && wifiNetworks.size() == 3);
  assert(wifiNetworks[0].ssid == "open" && wifiNetworks[1].rssi == -40);
  wifiNetworkIndex = 2;
  selectWifiNetwork();
  assert(uiMode == UI_WIFI_NETWORKS && wifiSetupMessage == "Unsupported security");
  wifiNetworkIndex = 1;
  selectWifiNetwork();
  assert(uiMode == UI_WIFI_PASSWORD && wifiSelectedSSID == "weak");
  uiMode = UI_WIFI_NETWORKS;
  wifiNetworkIndex = 0;
  selectWifiNetwork();
  assert(uiMode == UI_WIFI_PROGRESS && !wifiSelectedSecure && wifiPasswordDraft.length() == 0);
  processWifiSetup();
  onButtonLong();
  processWifiSetup();
  assert(wifiPrefs.writes == 0 && WiFi.apEnabled);
  requestWifiScan(); processWifiSetup();
  fakeMillis += WIFI_SCAN_TIMEOUT_MS;
  processWifiSetup();
  assert(wifiSetupState == WIFI_SETUP_FAILED);
  onButtonShort(); // Empty/failed scan retries.
  assert(wifiRequest == WREQ_SCAN);
  processWifiSetup();
  WiFi.scanState = 0;
  processWifiSetup();
  assert(wifiSetupState == WIFI_SETUP_READY && wifiNetworks.empty());
  onButtonShort();
  assert(wifiRequest == WREQ_SCAN);
  wifiNameScrollStart = fakeMillis;
  String longSSID = "abcdefghijklmnopqrstuvwxyz012345";
  assert(lcdNetworkName(longSSID, 19) == longSSID.substring(0, 19));
  fakeMillis += 4000;
  assert(lcdNetworkName(longSSID, 19) != longSSID.substring(0, 19));
  assert(lcdNetworkName("bad\nname", 20) == "bad?name");
}

void testConnectionPersistenceAndCancel() {
  resetWifi();
  std::snprintf(savedWifi.ssid, sizeof(savedWifi.ssid), "previous");
  std::snprintf(savedWifi.password, sizeof(savedWifi.password), "old-password");
  queueConnection("new-router", "new-password");
  assert(wifiSetupState == WIFI_SETUP_CONNECTING && wifiPrefs.writes == 0);
  WiFi.connected();
  onButtonLong(); // Even a newly connected trial must not be saved after cancel.
  processWifiSetup();
  assert(wifiPrefs.writes == 0 && String(savedWifi.ssid) == "previous");
  assert(WiFi.targetSSID == "previous" && WiFi.apEnabled);
  queueConnection("new-router", "wrong-password");
  fakeMillis += WIFI_CONNECT_TIMEOUT_MS;
  processWifiSetup();
  assert(wifiSetupState == WIFI_SETUP_FAILED && wifiPrefs.writes == 0);
  assert(String(savedWifi.ssid) == "previous" && WiFi.targetSSID == "previous");
  onButtonShort();
  assert(uiMode == UI_WIFI_PASSWORD && wifiPasswordDraft == "wrong-password");
  queueConnection("new-router", "new-password");
  wifiPrefs.failWrites = true;
  WiFi.connected(); processWifiSetup();
  assert(wifiSetupState == WIFI_SETUP_FAILED && String(savedWifi.ssid) == "previous");
  wifiPrefs.failWrites = false;
  queueConnection("new-router", "new-password");
  WiFi.connected(); processWifiSetup();
  assert(wifiSetupState == WIFI_SETUP_SUCCESS && wifiPrefs.writes == 1);
  assert(String(savedWifi.ssid) == "new-router" && WiFi.autoReconnect);
  assert(wifiPrefs.getBytesLength("credentials") == sizeof(WifiCredentials));
  lcdShowWifiProgress();
  assert(lcd.rows[2].find("192.168.1.123") == 0 && wifiPasswordDraft.length() == 0);
  onButtonShort();
  processWifiSetup();
  assert(uiMode == UI_MENU && WiFi.status() == WL_CONNECTED);
  savedWifi = WifiCredentials{};
  setup(); // Simulated reboot: read the actual NVS blob with the real startup path.
  assert(WiFi.targetSSID == "new-router" && WiFi.targetPassword == "new-password");
}

void testFaultPriorityAndStops() {
  resetWifi();
  queueConnection("router", "password");
  thermalFault = true;
  faultReason = "SENSOR DISCONNECTED";
  lcdRefresh(true);
  assert(uiMode == UI_STATUS && wifiRequest == WREQ_CANCEL);
  assert(lcd.rows[0].find("! SAFETY FAULT !") == 0);
  onEncoderTurn(1); onButtonShort();
  assert(uiMode == UI_STATUS);
  processWifiSetup();
  assert(wifiPrefs.writes == 0);
  thermalFault = false;
  startStep1(); startStep3();
  analogValue = 3723;
  adcFiltered = 0;
  thermalCycles = 1;
  try { taskThermal(nullptr); } catch (EndTask&) {}
  assert(lockDepth == 0 && pwmOutput > 0);
  server.routes.at("/stopall")();
  assert(!motorRunning && !heaterEnabled && pwmValues[RPWM_PIN] == 0 && toneValues[STEP_PIN] == 0);
  thermalCycles = 1;
  try { taskThermal(nullptr); } catch (EndTask&) {}
  assert(lockDepth == 0 && pwmValues[RPWM_PIN] == 0 && pinValues[ENABLE_PIN] == LOW);
}

int main() {
  setup();
  testRuntimeAndStatus();
  testPasswordEditor();
  testScanAndSelection();
  testConnectionPersistenceAndCancel();
  testFaultPriorityAndStops();
  std::cout << "Firmware checks passed: runtime, LCD, password editor, scan, connect, persistence, cancel, faults, stops\n";
}
