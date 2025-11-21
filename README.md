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
1) Install ESP-IDF (recommended v5.1 or newer) and export the environment (`export.ps1`/`export.sh`).
2) Set target: `idf.py set-target esp32s3`.
3) Build: `idf.py build`.
4) Flash (update the correct serial port): `idf.py -p <PORT> flash monitor`.

## Notes
- Pins for camera, servo, and power-switching rails are placeholders; adjust `main/board_config.h` once hardware is finalized.
- Vision and OTA are stubs—next steps are to pick a tiny model (ESP-DL/TFLM) and add provisioning + OTA flow.
- Power management is minimal; plan to use PIR as the primary wake source and keep Wi-Fi/camera off by default.
