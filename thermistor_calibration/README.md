# Thermistor calibration

A separate sketch that measures the thermistor the same way `filament_maker.ino` does,
and fits `NOMINAL_RESISTANCE` and `BETA_COEFFICIENT` from readings taken with a
reference thermometer. Because it uses the same ADC averaging, filter and divider formula,
the fitted constants also correct for ESP32 ADC error and the series resistor's tolerance.

## What you need

- A reference thermometer you trust (a K-type thermocouple meter is fine) with its probe
  in the heater block, as close to the thermistor as possible.
- The machine wired as usual. This sketch drives only the heater; both steppers stay off.

## Steps

1. Open this folder in Arduino IDE, select **ESP32S3 Dev Module**, and upload
   (or `arduino-cli compile --fqbn esp32:esp32:esp32s3 .` then upload).
2. Open the dashboard:
   - on the LAN at `http://filament-calibrate.local` (it reuses the Wi-Fi credentials the
     main firmware saved), or
   - join the Wi-Fi `Filament-Calibrate` (password `calibrate`) and open `http://192.168.4.1`.
   The serial monitor (115200) also prints the addresses and a reading every second.
3. **Cold point:** with the heater off and the block at room temperature, wait for
   **Stable**, type the reference reading, press **Capture**.
4. **Hot points:** set a fixed heater power, press **Apply**, and wait for **Stable**
   (this can take several minutes). Capture. Repeat near the temperatures you run at,
   for example around 100 °C and 200 °C. Three or more points spread over at least 30 °C
   fit both constants. With fewer points, only R25 is fitted and beta stays at 3950.
5. Check the **Error after** column, then copy the two lines from **Result** into the
   thermistor section of `../filament_maker.ino`, and flash the main firmware again.

Points are saved in the browser, so a page reload keeps them. Use the same browser
for the whole session.

## Safety

- The heater runs at a fixed power with no PID loop. Raise it in small steps.
- The firmware locks the heater off above 280 °C (calculated with the current constants)
  or if the sensor is open or shorted. Use **Reset fault** once the reading is back in range.
- The heater also turns off 10 s after the dashboard stops polling, for example when you
  close the tab or lose Wi-Fi.
- Do not leave the machine unattended while it is hot.

## Files

- `thermistor_calibration.ino`: sensor reading, heater, safety cut-offs, web server.
  Keep the pins and constants at the top in sync with `filament_maker.ino`.
- `calibration_dashboard.h`: the web page. The fit runs in the browser.
- `fit.test.cjs`: tests for the fit math, run with `node --test thermistor_calibration/fit.test.cjs`.
