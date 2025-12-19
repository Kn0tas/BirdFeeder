# Bird Feeder (ESP32-S3)

Battery-friendly, Wi-Fi-connected bird feeder with on-device vision to keep squirrels out.

## Hardware (current)
- ESP32-S3 DevKit N8R8
- PIR: AM312 (signal -> GPIO16, 3.3 V, GND)
- Camera: OV2640 Waveshare breakout (3.3 V only)
  - PWDN -> GPIO4, RESET -> GPIO1 (firmware drives GPIO1; update wiring if you previously used GPIO46)
  - SIOD/SIOC -> GPIO38/39
  - D0..D7 -> GPIO11,13,18,17,14,15,48,47
  - VSYNC/HREF/PCLK/XCLK -> GPIO6/7/12/10
- FRAM: MB85RC256V on I2C (SDA -> GPIO8, SCL -> GPIO9, A0/A1/A2/WP -> GND)
- Fuel gauge: MAX17048 (SDA -> GPIO8, SCL -> GPIO9, VCC 3.3 V, GND common, ALRT -> GPIO5)
- Servo: SG90 (signal -> GPIO2, powered from 5 V rail, GND common)
- Li-Ion 18650 cell
- Interim power: TPS63020 USB Auto Boost Buck Power Module (3.3 V) for prototyping

## Planned additions
- Solar panel (6 V / 1-2 W) + BQ24074 solar charging board (hookup paused until buck arrives)
- PCB power: TPS62740 3.3 V buck (planned for the final board)
- MAX17048 fuel gauge integration in firmware (I2C addr 0x36)
- Servo control refinements for lid actuation
- FRAM-backed config/event storage

## Project layout
- `CMakeLists.txt`: ESP-IDF project root
- `main/`: application sources
  - `app_main.c`: ties subsystems together; PIR-driven motion loop
  - `board_config.h`: pin map and shared settings
  - `power_manager.*`: deep sleep hooks (stub)
  - `sensors/`: PIR + camera
  - `vision/`: classifier API + embedded TFLite model (`model_int8.tflite`)
  - `actuators/`: servo control
  - `comms/`: Wi-Fi (secrets via `wifi_secrets.h`) + OTA stub
  - `storage/`: FRAM (I2C setup)
  - `logging/`: event logging shim
- `tools/train_and_convert.py`: trains a tiny MobileNetV2 model on `datasets/` and exports `main/vision/model_int8.tflite` (int8).
- `partitions.csv`: custom partition table with a larger factory slot (3 MB).
- `datasets/`: class folders for training data (crow/squirrel/rat/magpie/bird/other/unknown).

## Wi-Fi secrets
- Copy `main/wifi_secrets.example.h` to `main/wifi_secrets.h` and fill in your SSID/password (gitignored).

## Model training + embedding
- Train and export:
  - `python tools/train_and_convert.py --epochs 10 --img-size 96 --batch 32 --output main/vision/model_int8.tflite`
  - Uses `vision/labels.txt` class order: crow, squirrel, rat, magpie, bird, other, unknown.
  - Saves SavedModel to `build/saved_model/` and int8 TFLite to `main/vision/model_int8.tflite`.
- Build embeds `model_int8.tflite` automatically via `EMBED_FILES`.

## Wiring snapshot
- I2C bus: GPIO8 (SDA), GPIO9 (SCL); FRAM @0x50, MAX17048 @0x36; ALRT -> GPIO5
- Servo: GPIO2 (LEDC PWM); power from 5 V with common ground; add 470-1000 µF bulk cap near servo supply
- PIR: AM312 on GPIO16 (signal), 3.3 V, GND
- Camera (OV2640): PWDN GPIO4, RESET GPIO1 (driven by firmware), SIOD/SIOC GPIO38/39, D0..D7 GPIO11/13/18/17/14/15/48/47, VSYNC/HREF/PCLK/XCLK GPIO6/7/12/10; 3.3 V/GND
- Grounds: common across all devices; 3.3 V only for logic/I2C devices

## Getting started (ESP-IDF)
1) Install ESP-IDF (recommended v5.2.x)
   - Windows: use the "ESP-IDF Tools Installer for Windows" (Express, default paths, v5.2.x).
   - After install, open the "ESP-IDF PowerShell Environment" shortcut (activates the env).
   - Verify: `idf.py --version`.
2) From the ESP-IDF shell, go to the project root:
   `cd C:\Users\linus\OneDrive\Documents\git_repos\BirdFeeder`
3) Set the target (one-time per project):
   `idf.py set-target esp32s3`
4) Build:
   `idf.py build`
5) Find your COM port (Device Manager -> Ports). Example: COM5.
6) Flash + monitor (replace COM5 with your port):
   `idf.py -p COM5 flash monitor`
   - Exit monitor with `Ctrl+]` (or `Ctrl+T` then `Q`).

## Notes
- Pins for camera, servo, and power-switching rails are placeholders; adjust `main/board_config.h` once hardware is finalized.
- Vision: model is embedded; `vision_classify` still needs the TFLite Micro inference loop wired up to use it and log detections.
- OTA is stubbed; Wi-Fi uses secrets from `wifi_secrets.h`.
- Power management is minimal; plan to use PIR as the primary wake source and keep Wi-Fi/camera off by default.
- BQ24074 hookup is paused until the TPS63020 (3.3 V buck) arrives; for now, power via USB/bench 5 V feeding the interim regulator.
