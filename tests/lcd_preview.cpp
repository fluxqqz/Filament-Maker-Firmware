// Renders real LCD frames from the firmware into a standalone HTML preview.
//   c++ -std=c++17 tests/lcd_preview.cpp -o lcd-preview && ./lcd-preview ../lcd-preview.html
#define FILAMENT_HOST_TEST
#include "../filament_maker.ino"
#include <fstream>
#include <iostream>

struct Shot { std::string title, how; std::string rows[4]; };
std::vector<Shot> shots;

void shot(const char* title, const char* how) {
  lcdRefresh(true);
  shots.push_back({title, how, {lcd.rows[0], lcd.rows[1], lcd.rows[2], lcd.rows[3]}});
}

std::string esc(char c) {
  if (c == '<') return "&lt;";
  if (c == '>') return "&gt;";
  if (c == '&') return "&amp;";
  return std::string(1, c);
}
std::string esc(const std::string& s) { std::string o; for (char c : s) o += esc(c); return o; }

int main(int argc, char** argv) {
  setup();
  wifiStorageReady = true;
  step1Setpoint = 200; step2Setpoint = 230; rpm = 15; rpm2 = 8;

  // Status screens
  stopAll();
  currentTemp = 27.4f; setpoint = 200;
  shot("Status: idle", "Power-on screen, nothing running, no Wi-Fi.");

  fakeMillis = 1000;
  startStep1(); startStep3(); startStepper2();
  currentTemp = 199.6f; pwmOutput = 170;
  fakeMillis += 754000;
  WiFi.connection = WL_CONNECTED; WiFi.connectedSSID = "Workshop-2G";
  shot("Status: running", "Stage 1 heating, puller and cutter running, connected to Wi-Fi.");

  // Control menu
  onButtonShort();
  shot("Menu: top", "Click the encoder from the status screen.");
  for (int i = 0; i < MENU_COUNT - 1; ++i) onEncoderTurn(1);
  shot("Menu: bottom", "Turn the encoder down to the last item.");
  uiMode = UI_STATUS;

  // Wi-Fi setup
  requestWifiScan(); processWifiSetup();
  uiMode = UI_WIFI_NETWORKS;
  shot("Wi-Fi: scanning", "Menu > Wi-Fi Setup.");
  WiFi.results = {{"Workshop-2G", -48, WIFI_AUTH_WPA2_PSK}, {"Guest", -61, WIFI_AUTH_OPEN},
                  {"Office-Router", -72, WIFI_AUTH_WPA2_PSK}};
  WiFi.scanState = (int)WiFi.results.size();
  processWifiSetup();
  shot("Wi-Fi: networks", "Scan finished; turn to pick a network.");
  uiMode = UI_WIFI_PASSWORD;
  wifiSelectedSSID = "Workshop-2G"; wifiSelectedSecure = true;
  wifiPasswordDraft = "filament"; wifiPasswordChoice = 0;
  shot("Wi-Fi: password", "Turn to choose characters, click to add.");
  requestWifiConnect(); processWifiSetup();
  WiFi.connected(); processWifiSetup();
  shot("Wi-Fi: connected", "After choosing <CONNECT>.");
  onButtonShort(); processWifiSetup();
  uiMode = UI_STATUS;

  // Fault overrides every screen
  LOCK_STATE();
  currentTemp = 226.2f; setpoint = 210;
  tripThermalFaultLocked("OVERTEMP RUNAWAY");
  UNLOCK_STATE();
  shot("Safety fault", "Thermal fault trips; heater and cutter stop.");

  std::ofstream out(argc > 1 ? argv[1] : "lcd-preview.html");
  out << R"(<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>MOUNTOYA LCD preview</title>
<style>
body{margin:0;padding:32px 24px;background:#F7F7F7;color:#212121;font-family:-apple-system,"Segoe UI",Roboto,Arial,sans-serif}
main{max-width:1100px;margin:0 auto}
h1{font-size:24px;margin:0 0 4px;color:#0B3658}
.sub{color:#6B737B;font-size:13px;margin:0 0 28px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(330px,1fr));gap:24px}
figure{margin:0}
.bezel{background:#1d1f22;border-radius:10px;padding:14px;box-shadow:inset 0 0 0 2px #33363b}
.lcd{background:#1f4fd6;border-radius:3px;padding:8px;display:grid;grid-template-columns:repeat(20,1fr);gap:2px;
  box-shadow:inset 0 0 18px rgba(0,0,30,.45);container-type:inline-size}
.c{aspect-ratio:5/8;display:flex;align-items:center;justify-content:center;background:rgba(255,255,255,.07);
  color:#eef3ff;font:700 6.2cqw/1 "Consolas","Courier New",monospace;text-shadow:0 0 3px rgba(220,235,255,.7)}
figcaption{margin-top:10px}
figcaption b{display:block;font-size:14px}
figcaption span{font-size:12px;color:#6B737B}
</style></head><body><main>
<h1>LCD preview (20&times;4)</h1>
<p class="sub">Rendered from the firmware's own LCD code. Regenerate after changing it:
<code>c++ -std=c++17 tests/lcd_preview.cpp -o lcd-preview &amp;&amp; ./lcd-preview ../lcd-preview.html</code></p>
<div class="grid">
)";
  for (const auto& s : shots) {
    out << "<figure><div class=\"bezel\"><div class=\"lcd\" role=\"img\" aria-label=\"";
    for (const auto& r : s.rows) out << esc(r) << " / ";
    out << "\">";
    for (const auto& r : s.rows)
      for (char c : r) out << "<span class=\"c\">" << (c == ' ' ? "&nbsp;" : esc(c)) << "</span>";
    out << "</div></div><figcaption><b>" << esc(s.title) << "</b><span>" << esc(s.how) << "</span></figcaption></figure>\n";
  }
  out << "</div></main></body></html>\n";
  std::cout << "Wrote " << shots.size() << " LCD frames\n";
}
