# MOUNTOYA filament-maker controller

Arduino firmware for a dual-core ESP32 with thermistor/PID heater control,
stepper drive, a 20×4 I²C LCD, rotary encoder, web dashboard and ElegantOTA.

## Build

Build-tested with **ESP32-S3 Dev Module**, Arduino-ESP32 **3.3.10**,
ElegantOTA **3.1.7**, and LiquidCrystal_I2C **2.0.0** by Martin Kubovčík / Frank
de Brabander. Use the core and libraries already installed in your Arduino
sketchbook. The source requires two CPU cores; select the actual board and wiring
before uploading.

1. Copy `secrets.example.h` to `secrets.h` and configure the Wi-Fi credentials.
   `secrets.h` is ignored by Git. Without it, the example's AP-only settings apply.
2. Open `filament_maker.ino` in Arduino IDE and select **ESP32S3 Dev Module**.
   With Arduino CLI on PATH, the equivalent build is:

   ```sh
   arduino-cli compile --fqbn esp32:esp32:esp32s3 .
   ```

   If CLI cannot find libraries that work in the IDE, pass the IDE's
   `arduino-cli.yaml` using `--config-file`, or point `--libraries` at the IDE's
   sketchbook `libraries` folder. This reuses the existing installation.
   Arduino-ESP32 2.x uses different LEDC APIs and is not supported by this sketch.
3. Select the board's actual flash/partition settings and serial port to upload.
   Keep an OTA-capable partition layout if using ElegantOTA at `/update`.

## Wiring

| Component | GPIO / configuration |
|---|---|
| Thermistor ADC | 15; 100 kΩ NTC, beta 3950, 10 kΩ series resistor |
| BTS7960 heater | enable 4, RPWM 5, LPWM 6 |
| Stepper driver | STEP 18, DIR 17; 3200 pulses/motor revolution |
| Output-wheel reduction | 25 motor turns per wheel turn |
| LCD | SDA 8, SCL 9; I²C address `0x27`, 20×4 |
| Rotary encoder | CLK 11, DT 12, switch 13 |

Thermistor constants, microstepping and gearing live at the top of the sketch;
match them to the installed hardware and calibrate temperature/speed on-device.

## Operation

- Open `http://filament-maker.local` on the LAN, or join `Filament-Maker-AP`
  and open `http://192.168.1.1`.
- Setpoints use **0–260°C** and output-wheel speed uses **0.1–60 RPM** across
  the web, encoder and serial interfaces. Invalid web/serial numeric commands
  are rejected; invalid saved settings fall back to defaults at startup.
- Stages 1 and 2 select heating targets. Stage 3 controls the motor independently;
  stages do not advance automatically.
- LCD edits are drafts: click to save/apply, hold to discard and return to status.
- Web **Stop All** sends one `POST /stopall`, which disables both outputs under
  the state mutex before acknowledging. Failed/timed-out requests remain marked
  **STOP UNCONFIRMED** and can be retried.
- The dashboard polls every 500 ms. Displayed wheel RPM is calculated from the
  commanded pulse rate, not measured by a speed sensor.
- Sensor-range, overtemperature and heating-rise faults latch the heater off.
  Reset using the dashboard, a long encoder press, or serial `R` after the
  temperature returns to the safe range. Reset does not start the heater.

Serial runs at **115200 baud**, one command per line:

| Command | Action |
|---|---|
| `T210` | Set active target temperature |
| `S15` | Set output-wheel RPM |
| `P19`, `I0.45`, `D1.5` | Set a finite, non-negative PID gain |
| `1`, `2`, `3` | Start a stage |
| `!1`, `!2`, `!3` | Stop a stage |
| `s`, `f`, `d` | Start motor, stop motor, reverse direction |
| `R` / `reset` | Reset PID / clear a safe thermal fault |
| `m`, `v1`, `v0` | Toggle / enable / disable logging |
| `e`, `E` | Encoder snapshot / five-second diagnostic |

## Checks

With Node.js 18 or newer:

```sh
node --test dashboard.test.cjs
```

These tests run the actual embedded dashboard JavaScript against a fake DOM and
network. They cover status failures, edit/save races, numeric limits and Stop All
acknowledgement/failure/retry. The Arduino build checks firmware compilation.
Physical PWM timing, sensor calibration and shutdown behavior require a bench
check on the actual controller.
