# Bird Feeder (ESP32-S3)

Battery-friendly, Wi-Fi-connected bird feeder with on-device vision to keep squirrels out.

## Hardware (current)
- ESP32-S3 DevKit N8R8
- PIR: EKMB1103113 (signal -> GPIO4)
- FRAM: MB85RC256V (SDA -> GPIO8, SCL -> GPIO9, A0/A1/A2/WP -> GND)
- Li-Ion 18650 cell

## Planned additions
- OV2640 camera module
- Solar panel (6 V / 1–2 W) + BQ24074 solar charging board
- TPS62740 3.3 V buck
- MAX17048 fuel gauge
- Servo for lid actuation
- FRAM breakout (larger) and rocker switch

## Project layout
- `CMakeLists.txt`: ESP-IDF project root
- `main/`: application sources
  - `app_main.c`: ties subsystems together
  - `board_config.h`: pin map and shared settings
  - `power_manager.*`: deep sleep hooks (stub)
  - `sensors/`: PIR + camera stubs
  - `vision/`: placeholder classifier API
  - `actuators/`: servo control stub
  - `comms/`: Wi-Fi + OTA stubs
  - `storage/`: FRAM (I2C setup) stub
  - `logging/`: event logging shim
- `scripts/`, `tests/`: reserved for tooling and tests

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
- Vision and OTA are stubs; next steps are to pick a tiny model (ESP-DL/TFLM) and add provisioning + OTA flow.
- Power management is minimal; plan to use PIR as the primary wake source and keep Wi-Fi/camera off by default.
