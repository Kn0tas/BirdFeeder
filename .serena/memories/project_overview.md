# BirdFeeder Project Overview

## Purpose
ESP32-S3 based bird feeder with on-device vision (TFLite Micro) to detect threats (crows, magpies, squirrels) and close a servo-controlled lid.

## Hardware
- ESP32-S3 DevKit N8R8 (8MB Flash, 8MB Octal PSRAM)
- OV2640 camera
- PIR sensor (AM312), Fuel Gauge (MAX17048), FRAM (MB85RC256V), Servo (SG90)

## Tech Stack
- Firmware: C/C++ with ESP-IDF v5.2-v5.5
- Build: CMake via `idf.py`
- AI: TFLite Micro (MobileNetV2 alpha=0.35, int8 quantized, 96x96 input)
- Training: Python + TensorFlow/Keras

## Key Files
- `main/app_main.c` — main app logic, motion detection, vision loop
- `main/vision/vision.cpp` — TFLite Micro inference pipeline
- `main/vision/vision.h` — vision result types (CROW, MAGPIE, SQUIRREL, BACKGROUND, UNKNOWN)
- `main/sensors/camera.c` — OV2640 camera init + capture
- `tools/train_and_convert.py` — model training and int8 TFLite export
- `tools/decode_frame.py` — decode base64 frame dumps from serial
- `documentation/plans.md` — detailed improvement plan
- `main/vision/labels.txt` — class labels (crow, magpie, squirrel, background)

## Build Commands
```powershell
idf.py build
idf.py -p COMx flash monitor
```

## AI Pipeline
Camera capture (QQVGA 160x120 JPEG) → fmt2rgb888 → center-crop/resize to 96x96 → quantize int8 → TFLite Micro → 4-class softmax output

## Current Status (as of 2026-02-20)
- Model trained for 4 classes: crow, magpie, squirrel, background
- SOI marker validation and RGB buffer zeroing added to vision.cpp
- Debug logging active (Input Stats, Raw Outputs, base64 frame dump)
- FB-OVF issue: camera still uses fb_count=1 + DRAM — PSRAM double-buffer fix NOT applied yet
- Preprocessing: raw pixels passed to quantization (scale/zero_point from TFLite)
