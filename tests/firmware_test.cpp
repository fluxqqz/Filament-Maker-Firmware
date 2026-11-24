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

void assertLcdRow(int row, const char* text) {
  std::string expected = text;
  expected.resize(20, ' ');
  assert(lcd.rows[row] == expected);
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
  assertLcdRow(1, "CUT:0.0 PULL:0.0");
  assert(lcd.rows[2].find("RUNTIME 00:00:13") == 0);
  assertLcdRow(3, "WiFi: Not connected");
  startStep3();
  assert(motorElapsedMs == 0);
  currentTemp = 215.3f; setpoint = 220; pwmOutput = 166; heaterEnabled = true;
  fakeMillis += 754000;
  lcd.rows[1].assign(20, 'X');
  lcd.rows[2].assign(20, 'X'); // Reordered rows must overwrite the previous layout.
  lcdShowStatus();
  assert(lcd.rows[0].find("TEMP:215.3/220.0C") == 0);
  assertLcdRow(1, "CUT:0.0 PULL:15.0");
  assert(lcd.rows[2].find("RUNTIME 00:12:34") == 0);
  assertLcdRow(3, "WiFi: Not connected");
  stopAll();
  fakeMillis = 0xfffffff0u;
  startStep3();
  fakeMillis = 0x20u;
  stopStep3();
  assert(motorElapsedMs == 48); // millis() rollover.
  assert(toneValues[STEP_PIN] == 0);
}

void testMainScreenWifi() {
  resetWifi();
  stopAll();
  std::snprintf(savedWifi.ssid, sizeof(savedWifi.ssid), "Network-A");
  WiFi.targetSSID = "Network-B";
  WiFi.connectedSSID = "cached-router";
  lcdShowStatus();
  assertLcdRow(3, "WiFi: Not connected"); // A saved/cached SSID does not prove connection.
  WiFi.connection = WL_CONNECTED;
  WiFi.connectedSSID = "Network-B";
  lcdShowStatus();
  assertLcdRow(3, "WiFi: Network-B");
  for (const auto& row : lcd.rows) assert(row.find("HEAT:") == std::string::npos);

  fakeMillis = 1000;
  WiFi.connectedSSID = "abcdefghijklmnopqrstuvwxyz012345";
  wifiNameScrollStart = 0; // Main-screen scrolling has its own timer.
  int clears = lcd.clearCount;
  lcdShowStatus();
  assertLcdRow(3, "WiFi: abcdefghijklmn");
  std::string readings[] = {lcd.rows[0], lcd.rows[1], lcd.rows[2]};
  fakeMillis += 999;
  lcdShowStatus();
  assertLcdRow(3, "WiFi: abcdefghijklmn");
  wifiNameScrollStart = fakeMillis;
  fakeMillis += 501;
  lcdShowStatus();
  assertLcdRow(3, "WiFi: bcdefghijklmno");
  for (int row = 0; row < 3; ++row) assert(lcd.rows[row] == readings[row]);
  assert(lcd.clearCount == clears);

  WiFi.connectedSSID = "ZYXWVUTSRQPONMLKJIHGFEDCBA987654";
  lcdShowStatus();
  assertLcdRow(3, "WiFi: ZYXWVUTSRQPONM"); // Start at the beginning when the network changes.
  WiFi.connectedSSID = "C";
  lcdShowStatus();
  assertLcdRow(3, "WiFi: C"); // Clear the tail of the longer name.
  WiFi.connectedSSID = "bad\nnet";
  lcdShowStatus();
  assertLcdRow(3, "WiFi: bad?net");
  assert(WiFi.connectedSSID == "bad\nnet");
  WiFi.connection = 0;
  lcdShowStatus();
  assertLcdRow(3, "WiFi: Not connected");

  fakeMillis = 0xffffff00u;
  WiFi.connection = WL_CONNECTED;
  WiFi.connectedSSID = "1234567890abcdefghijklmnopqrstuv";
  lcdShowStatus();
  assertLcdRow(3, "WiFi: 1234567890abcd");
  fakeMillis += 1500;
  lcdShowStatus();
  assertLcdRow(3, "WiFi: 234567890abcde"); // Scrolling survives millis() rollover.
  assert(String(savedWifi.ssid) == "Network-A" && wifiPrefs.writes == 0);
}

void testControlMenuScrollingAndActions() {
  resetWifi();
  stopAll();
  onButtonShort();
  assert(uiMode == UI_MENU && MENU_COUNT == 7 && menuIndex == 0 && menuTop == 0);
  const char* labels[] = {"Start S1 100C", "Start S2 200C", "Start CUTTER", "Start PULLER", "STOP ALL", "IP:192.168.1.1", "Wi-Fi Setup"};
  for (int i = 0; i < MENU_COUNT; ++i) assert(menuLabel(i) == labels[i]);
  int clears = lcd.clearCount;
  lcdRefresh(true);
  assertLcdRow(0, ">Start S1 100C");
  const int downTop[] = {0, 0, 0, 0, 1, 2, 3};
  for (int i = 1; i < MENU_COUNT; ++i) {
    onEncoderTurn(1);
    lcdRefresh(true);
    assert(menuIndex == i && menuTop == downTop[i]);
    assert(lcd.rows[i - menuTop][0] == '>');
  }
  assertLcdRow(0, " Start PULLER");
  assertLcdRow(3, ">Wi-Fi Setup");
  onEncoderTurn(1); lcdRefresh(true);
  assert(menuIndex == 6 && menuTop == 3);
  const int upTop[] = {0, 1, 2, 3, 3, 3, 3};
  for (int i = 5; i >= 0; --i) {
    onEncoderTurn(-1);
    lcdRefresh(true);
    assert(menuIndex == i && menuTop == upTop[i]);
    assert(lcd.rows[i - menuTop][0] == '>');
  }
  onEncoderTurn(-1); lcdRefresh(true);
  assert(menuIndex == 0 && menuTop == 0 && lcd.clearCount == clears);
  onButtonShort();
  assert(heaterEnabled && currentStep == 1 && menuLabel(0) == "Stop S1 100C");
  onButtonShort();
  assert(!heaterEnabled && currentStep == 0);
  onEncoderTurn(1); onButtonShort();
  assert(heaterEnabled && currentStep == 2 && menuLabel(1) == "Stop S2 200C");
  onButtonShort();
  assert(!heaterEnabled);
  onEncoderTurn(1); onButtonShort();
  assert(motor2Running && !motorRunning && menuLabel(2) == "Stop CUTTER");
  onButtonShort();
  assert(!motor2Running);
  onEncoderTurn(1); onButtonShort();
  assert(motorRunning && !motor2Running && menuLabel(3) == "Stop PULLER");
  onButtonShort();
  assert(!motorRunning);
  // Puller start/stop must not change which heating stage is active.
  startStep1(); startStep3();
  assert(currentStep == 1 && heaterEnabled && menuLabel(0) == "Stop S1 100C");
  stopStep3();
  assert(currentStep == 1 && heaterEnabled && setpoint == step1Setpoint);
  stopStep1();
  onButtonShort(); startStep1();
  onEncoderTurn(1); onButtonShort();
  assert(uiMode == UI_STATUS && !heaterEnabled && !motorRunning);
  assert(pwmValues[RPWM_PIN] == 0 && toneValues[STEP_PIN] == 0);
  onButtonShort();
  for (int i = 0; i < 5; ++i) onEncoderTurn(1);
  onButtonShort();
  assert(uiMode == UI_MENU && menuIndex == 5 && wifiRequest == WREQ_NONE);
  onEncoderTurn(1); onButtonShort();
  assert(uiMode == UI_WIFI_NETWORKS && wifiRequest == WREQ_SCAN);
  onButtonLong(); processWifiSetup();
}

void testControlMenuTimeout() {
  resetWifi();
  fakeMillis = 1000;
  startStep1(); startStep3();
  onButtonShort();
  fakeMillis += MENU_IDLE_TIMEOUT_MS - 1;
  lcdRefresh(true);
  assert(uiMode == UI_MENU);
  onEncoderTurn(-1); // Even a turn at the top boundary resets inactivity.
  fakeMillis += MENU_IDLE_TIMEOUT_MS - 1;
  lcdRefresh(true);
  assert(uiMode == UI_MENU);
  for (int i = 0; i < 5; ++i) onEncoderTurn(1);
  ++fakeMillis;
  onButtonShort(); // The informational IP item also resets inactivity.
  assert(lastMenuInteractionMs == fakeMillis);
  fakeMillis += MENU_IDLE_TIMEOUT_MS - 1;
  lcdRefresh(false);
  assert(uiMode == UI_MENU);
  ++fakeMillis;
  lcdRefresh(false); // Must expire even inside the normal 250ms render throttle.
  assert(uiMode == UI_STATUS && heaterEnabled && motorRunning);
  assertLcdRow(1, "CUT:0.0 PULL:15.0");
  stopAll();

  fakeMillis = 0xfffffff0u;
  onButtonShort();
  fakeMillis += MENU_IDLE_TIMEOUT_MS - 1;
  lcdRefresh(true);
  assert(uiMode == UI_MENU);
  ++fakeMillis; lcdRefresh(false);
  assert(uiMode == UI_STATUS); // Inactivity arithmetic survives millis() rollover.

  onEncoderTurn(1);
  assert(uiMode == UI_MENU && menuIndex == 0 && menuTop == 0);
  fakeMillis += MENU_IDLE_TIMEOUT_MS - 10;
  inputValues[ENC_SW_PIN] = LOW;
  pollButton();
  fakeMillis += 20;
  lcdRefresh(true);
  assert(uiMode == UI_MENU); // A press near the deadline counts before debounce completes.
  fakeMillis += 31; pollButton();
  fakeMillis += 799; pollButton();
  assert(uiMode == UI_MENU);
  fakeMillis += 2; pollButton();
  assert(uiMode == UI_STATUS); // Long press still exits after ~0.8s.
  inputValues[ENC_SW_PIN] = HIGH;
  pollButton(); fakeMillis += 51; pollButton();
  assert(uiMode == UI_STATUS); // Releasing a long press must not reopen the menu.
  inputValues.clear();

  onButtonShort();
  for (int i = 0; i < 6; ++i) onEncoderTurn(1);
  lcdRefresh(true);
  assert(menuIndex == 6 && menuTop == 3);
  for (UiMode mode : {UI_WIFI_NETWORKS, UI_WIFI_PASSWORD, UI_WIFI_PROGRESS}) {
    uiMode = mode;
    wifiSetupState = WIFI_SETUP_CONNECTING;
    wifiPasswordDraft = "unfinished-draft";
    fakeMillis += MENU_IDLE_TIMEOUT_MS + 1;
    lcdRefresh(true);
    assert(uiMode == mode && wifiPasswordDraft == "unfinished-draft");
  }
  uiMode = UI_WIFI_PASSWORD;
  wifiPasswordChoice = 2;
  onButtonShort();
  lcdRefresh(false);
  assert(uiMode == UI_MENU && lastMenuInteractionMs == fakeMillis);
  assert(menuIndex == 6 && menuTop == 3);
  assertLcdRow(3, ">Wi-Fi Setup");
  processWifiSetup();
  fakeMillis += MENU_IDLE_TIMEOUT_MS - 1; lcdRefresh(true);
  assert(uiMode == UI_MENU);
  ++fakeMillis; lcdRefresh(false);
  assert(uiMode == UI_STATUS);
  uiMode = UI_WIFI_PROGRESS;
  wifiSetupState = WIFI_SETUP_SUCCESS;
  onButtonShort(); lcdRefresh(false);
  assert(uiMode == UI_MENU && lastMenuInteractionMs == fakeMillis);
  assertLcdRow(3, ">Wi-Fi Setup");
  onButtonLong();
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
  startStep1(); startStep3(); startStepper2();
  analogValue = 3723;
  adcFiltered = 0;
  thermalCycles = 1;
  try { taskThermal(nullptr); } catch (EndTask&) {}
  assert(lockDepth == 0 && pwmOutput > 0);
  server.routes.at("/stopall")();
  assert(!motorRunning && !motor2Running && !heaterEnabled && pwmValues[RPWM_PIN] == 0 && toneValues[STEP_PIN] == 0 && toneValues[STEP2_PIN] == 0);
  thermalCycles = 1;
  try { taskThermal(nullptr); } catch (EndTask&) {}
  assert(lockDepth == 0 && pwmValues[RPWM_PIN] == 0 && pinValues[ENABLE_PIN] == LOW);
}

void testStepper2Controls() {
  resetWifi();
  stopAll();
  assert(!motor2Running && toneValues[STEP2_PIN] == 0 && menuLabel(2) == "Start CUTTER");
  server.arguments["state"] = "on";
  server.routes.at("/stepper2")();
  assert(motor2Running && toneValues[STEP2_PIN] == 800 && menuLabel(2) == "Stop CUTTER"); // 15 RPM * 3200 / 60
  lcdRefresh(true);
  assertLcdRow(1, "CUT:15.0 PULL:0.0");
  assert(!motorRunning); // stepper 1 untouched
  server.arguments["value"] = "60";
  server.routes.at("/rpm2")();
  assert(rpm2 == 60.0f && toneValues[STEP2_PIN] == 3200);
  server.arguments["value"] = "61";
  server.routes.at("/rpm2")();
  assert(rpm2 == 60.0f && server.responseCode == 400);
  activateMenuItem(2);
  assert(!motor2Running && toneValues[STEP2_PIN] == 0);
  activateMenuItem(2);
  assert(motor2Running);
  server.routes.at("/stopall")();
  assert(!motor2Running && toneValues[STEP2_PIN] == 0);
  startStepper2();
  currentTemp = 236.4f; setpoint = 210.0f;
  LOCK_STATE(); tripThermalFaultLocked("TEST TRIP"); UNLOCK_STATE();
  assert(thermalFault && !motor2Running && toneValues[STEP2_PIN] == 0);
  assert(std::string(buildStatus().c_str()).find("\"faultTemp\":236.4,\"faultSP\":210.0,") != std::string::npos);
  thermalFault = false;
}

int adcForTemp(float c) { // Inverse of readTempC().
  float r = NOMINAL_RESISTANCE * exp(BETA_COEFFICIENT * (1.0 / (c + 273.15) - 1.0 / (NOMINAL_TEMPERATURE + 273.15)));
  return (int)lround(4095.0 * r / (SERIES_RESISTOR + r));
}

void thermalCycleAt(float c) { // One 100 ms thermal-loop iteration with the block at c.
  analogValue = adcForTemp(c);
  adcFiltered = 0;
  thermalCycles = 1;
  try { taskThermal(nullptr); } catch (EndTask&) {}
}

// Stage 2 -> Stage 1 at 220 -> 180 C, then `seconds` of the block moving at `ratePerSec`.
void switchToLowerStage(float ratePerSec, int seconds) {
  stopAll();
  thermalFault = false;
  step2Setpoint = 220; step1Setpoint = 180;
  startStep2();
  for (int i = 0; i < 5; ++i) thermalCycleAt(220);
  startStep1();
  float t = 220;
  for (int i = 0; i < seconds * 10 && !thermalFault; ++i) {
    thermalCycleAt(t);
    t += ratePerSec * 0.1f;
  }
}

void testRunawayWatchdog() {
  switchToLowerStage(-1.0f, 30); // Passive cooldown after lowering the setpoint is normal.
  assert(!thermalFault && heaterEnabled && currentStep == 1 && pwmOutput == 0);
  switchToLowerStage(-0.15f, 30); // Even slowly, as long as it keeps falling.
  assert(!thermalFault);
  switchToLowerStage(0.0f, 12); // Holding 40 C over setpoint: driver stuck on.
  assert(thermalFault && faultReason == "OVERTEMP RUNAWAY" && !heaterEnabled);
  switchToLowerStage(0.2f, 12); // Rising over setpoint.
  assert(thermalFault && faultReason == "OVERTEMP RUNAWAY");
  switchToLowerStage(-1.0f, 6); // Cools to 214 C, then stops falling.
  for (int i = 0; i < 120 && !thermalFault; ++i) thermalCycleAt(214);
  assert(thermalFault && faultReason == "OVERTEMP RUNAWAY");
  stopAll();
  thermalFault = false;
  faultReason = "";
}

// Feeds CLK/DT states (bit1=CLK, bit0=DT) through the encoder ISR.
void feedEncoder(std::initializer_list<int> states) {
  for (int s : states) {
    inputValues[ENC_CLK_PIN] = s >> 1;
    inputValues[ENC_DT_PIN] = s & 1;
    encISR();
  }
}

void testEncoderOneMovePerClick() {
  // KY-040 measured on hardware: one click = 11 -> 01 -> 00 -> 10 -> 11 (4 edges).
  encPrevState = 0b11; encDelta = 0;
  feedEncoder({0b01, 0b00, 0b10, 0b11});
  assert(encDelta == -1);
  feedEncoder({0b10, 0b00, 0b01, 0b11});
  assert(encDelta == 0);
  feedEncoder({0b01, 0b11, 0b01, 0b00, 0b10, 0b11}); // contact bounce at rest, then one click
  assert(encDelta == -1);
  encDelta = 0;

  uiMode = UI_MENU; menuIndex = 3;
  feedEncoder({0b10, 0b00, 0b01, 0b11});
  pollEncoder();
  assert(menuIndex == 4); // one click moves exactly one menu item
  feedEncoder({0b01, 0b00, 0b10, 0b11, 0b01, 0b00, 0b10, 0b11});
  pollEncoder();
  assert(menuIndex == 2);
  uiMode = UI_STATUS;
}

int main() {
  setup();
  testRuntimeAndStatus();
  testMainScreenWifi();
  testControlMenuScrollingAndActions();
  testControlMenuTimeout();
  testPasswordEditor();
  testScanAndSelection();
  testConnectionPersistenceAndCancel();
  testFaultPriorityAndStops();
  testStepper2Controls();
  testRunawayWatchdog();
  testEncoderOneMovePerClick();
  std::cout << "Firmware checks passed: runtime, LCD, live Wi-Fi/SSID scrolling, scrolling menu, inactivity, password editor, scan, connect, persistence, cancel, faults, stops, stepper 2, runaway watchdog, encoder\n";
}
