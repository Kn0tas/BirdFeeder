# Bird Feeder (ESP32-S3)

Battery-friendly, Wi-Fi-connected bird feeder with on-device vision to keep squirrels out.

## Hardware Requirements
- **Board**: ESP32-S3 DevKit N8R8 (8MB Flash, 8MB Octal PSRAM)
  - **Required**: Flash >= 4MB, PSRAM >= 2MB (for TFLite arena)
- **Camera**: OV2640 (on standard header/breakout)
- **Sensors**:
  - PIR Motion Sensor (AM312)
  - Fuel Gauge (MAX17048)
  - FRAM (MB85RC256V)
- **Actuator**: Servo (SG90)

## Wiring / Pinout
See `main/board_config.h` for the definitive source.
- **Micro-Controller**: ESP32-S3
- **I2C Bus (Port 0)**:
  - SDA: GPIO 8
  - SCL: GPIO 9
  - Devices: FRAM (0x50), MAX17048 (0x36)
- **Camera (SCCB Port 1)**:
  - Data: GPIO 11, 13, 18, 17, 14, 15, 48, 47 (D0-D7)
  - Control: GPIO 38 (SIOD), 39 (SIOC), 10 (XCLK), 12 (PCLK), 7 (HREF), 6 (VSYNC)
  - Power/Reset: GPIO 4 (PWDN), GPIO 1 (RESET)
- **PIR**: GPIO 16
- **Servo**: GPIO 2

## Project Status
- **Vision**: Fully implemented using TFLite Micro.
  - Model: `main/vision/model_int8.tflite` (embedded)
  - Classes: Crow, Squirrel, Rat, Magpie, Bird, Other, Unknown.
  - Performance: Runs in ~250ms on ESP32-S3 (PSRAM).
- **Logging**: Detections are logged to serial output: `I (14111) app_main: detected SQUIRREL (conf=0.19)`
- **Power**: Deep sleep logic implemented (wakes on PIR).

## Build Instructions
1.  **Install ESP-IDF** (v5.2 - v5.5 supported).
2.  **Configuration**:
    - Project uses a custom partition table (`partitions.csv`) to fit the model.
    - Project requires PSRAM to be enabled.
    - `sdkconfig.defaults` is provided to set these automatically.
3.  **Build & Flash**:
    ```powershell
    idf.py build
    idf.py -p COMx flash monitor
    ```
    *(Replace COMx with your device port, e.g., COM5)*

## Technical Notes
- **I2C Driver**: The project uses the **ESP-IDF v5 NG I2C driver** (`driver/i2c_master.h`) for the sensor bus to avoid conflicts with the camera component (which also uses the new driver).
- **Memory**: The TFLite tensor arena (2MB) is allocated in **PSRAM**. If PSRAM is not detected, the application will fail to initialize vision.
- **Linkage**: The `model_data.h` includes C++ guards to ensure correct linking of the model symbols.

## Troubleshooting
- **"AllocateTensors failed"**: Ensure your board has PSRAM and it is enabled in `sdkconfig` (check boot logs for `Found 8MB PSRAM`).
- **"CONFLICT! driver_ng..."**: Ensure you have done a clean build if upgrading from an older version. The project code now strictly uses the new I2C driver.
