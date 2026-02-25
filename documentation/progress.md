# BirdFeeder — Development Progress

Last updated: 2026-02-23

---

## ✅ Completed Work

### Stage 1 — Camera Stability (FB-OVF Bugs)

**Problem:** The device was flooding the log with `cam_hal: FB-OVF` and `cam_hal: NO-EOI` errors, meaning frame buffer overflow events were corrupting JPEG frames before vision could decode them.

**Changed files:**

#### [`main/sensors/camera.c`](../main/sensors/camera.c)

| Setting        | Before              | After                | Reason                                                   |
| -------------- | ------------------- | -------------------- | -------------------------------------------------------- |
| `xclk_freq_hz` | 20 MHz              | 14 MHz               | Fewer DMA timing violations                              |
| `jpeg_quality` | 20                  | 12                   | Less aggressive JPEG compression → fewer artifacts       |
| `fb_count`     | 1                   | 2                    | Double-buffering prevents ring overflow when CPU is busy |
| `fb_location`  | `CAMERA_FB_IN_DRAM` | `CAMERA_FB_IN_PSRAM` | PSRAM has more headroom                                  |

**Result:** FB-OVF rate significantly reduced. Camera stable during inference.

---

### Stage 2 — Debug Frame Dump Gate

**Problem:** `camera_dump_base64()` was always active, flooding serial at ~100 KB/frame and blocking the detection loop by 100–500 ms.

#### [`main/app_main.c`](../main/app_main.c)

- Wrapped frame dump in `#ifdef BIRDFEEDER_DEBUG_FRAMES`
- To re-enable: add `#define BIRDFEEDER_DEBUG_FRAMES` or add the flag in CMakeLists.txt

---

### Stage 3 — AI Model Dataset & Training Pipeline Overhaul

**Problem:** Model only knew 3 classes, squirrel was severely under-represented (37 images), base model fully frozen, no fine-tuning, training bugs prevented model from saving.

#### [`tools/train_and_convert.py`](../tools/train_and_convert.py)

- **4-class output:** `crow`, `magpie`, `squirrel`, `background`
- **Augmentation:** horizontal flip, rotation (±20%), zoom (±20%), contrast (±20%), brightness (±20%), translation (±10%) — all moved into the **data pipeline** (not the model graph)
- **Two-phase training:**
  1. Phase 1 — frozen base, trains the classifier head (20 epochs, early stopping)
  2. Phase 2 — unfreezes top layers of MobileNetV2 backbone for fine-tuning (20 epochs, lr=1e-5)
- **Class weights:** computed from file counts, passed to `model.fit()` to balance loss contribution
- **Early stopping:** `patience=5`, monitors `val_loss`, restores best weights
- **Dataset:** shuffled 80/20 split, prefetched with `tf.data.AUTOTUNE`
- **Post-conversion self-test:** verifies the `int8` model with XNNPACK disabled (matching device path), flags calibration collapse before flashing

#### [`main/vision/labels.txt`](../main/vision/labels.txt)

- Updated from 3 classes to 4: `crow`, `magpie`, `squirrel`, `background`

#### Dataset (`datasets/`)

- Squirrel class expanded from 37 → 814 images using public sources and augmentation
- Background class added and expanded to 215 images
- Total: 1,911 images across 4 classes

---

### Stage 4 — AI Collapse Root Cause Investigation & Fix

**Problem:** Despite retraining, the device consistently output identical probabilities (e.g., `0.33, 0.18, 0.33, 0.16`) for every frame, regardless of what the camera was pointing at.

**Investigation path:**

1. Verified camera pipeline was producing valid, varying JPEG data ✅
2. Verified quantization parameters were sane (`scale=0.00784, zp=-1`) ✅
3. Removed `Rescaling` layer from Keras model graph (was creating `MUL`+`ADD` ops) ✅
4. Updated device preprocessing to normalize `[0,255] → [-1,1]` ✅
5. Added on-device self-test (BLACK and GREY synthetic inputs at boot) — **both produced identical output** → proved `Invoke()` itself was broken, not the model or camera

**Root cause confirmed:** **ESP-NN S3 assembly-optimized kernels** (`espressif__esp-nn`, default `CONFIG_NN_OPTIMIZED`) have a bug when the tensor arena is allocated in PSRAM. They were reading from a stale cached buffer instead of the actual input, causing all inferences to produce identical output regardless of input.

#### [`sdkconfig.defaults`](../sdkconfig.defaults)

```ini
CONFIG_NN_ANSI_C=y   # Use pure C reference kernels — bypasses the buggy S3 assembly
```

> **Trade-off:** inference is slower (~1.3 s/frame vs ~0.3 s) but correct.

#### [`main/vision/vision.cpp`](../main/vision/vision.cpp)

- Updated pixel normalization: `[0,255] → [-1,1]` before quantizing with model's `scale/zp`
- Removed on-device self-test block (no longer needed)
- Removed verbose input stats and raw output logging

**Result:** Model now dynamically responds to input. Live test with a magpie photo showed magpie (class 1) receiving 0.15–0.39 confidence vs. 0.01 for crow/background — confirming the classifier is active and discriminating between classes.

---

### Stage 5 — Hardware Power System Wiring

**Problem:** The device needed to be disconnected from USB power and connected to its permanent off-grid solar and battery power system.

**Hardware Utilized:**

- **Charger:** Adafruit USB-C Lithium Ion/Polymer Charger (bq24074)
- **Solar Panel:** 6V 2W Polycrystalline Solar Panel (136x110mm)
- **Battery:** 3.7V LiPo Battery
- **Fuel Gauge:** MAX17048 I2C Breakout

**Wiring Implemented:**

- _(Pending)_ Solar panel to be connected to `VBUS` and `GND` across the charger input (waiting for JST connectors).
- LiPo battery plugged directly into the charger's `Lipo Bat` JST port.
- Charger output `OUT` and `GND` routed directly to ESP32-S3 `5V` (VIN) and `GND`.
- The MAX17048 Fuel Gauge `+` pin was wired directly to the LiPo battery positive wire to ensure the ESP32 can accurately measure the true battery voltage instead of the charger's 4.4V regulated load output.

**Result:** ESP32-S3 successfully booted via battery power. Initial I2C serial logs confirmed the MAX17048 fuel gauge was communicating perfectly, reading a healthy 4.253V directly from the battery while the ESP32 was powered by the charger load pins.

---

## 📊 Current Dataset State

| Class      | Images    |
| ---------- | --------- |
| crow       | 421       |
| magpie     | 465       |
| squirrel   | 814       |
| background | 215       |
| **Total**  | **1,911** |

> Note: Squirrel is intentionally over-represented because the original dataset was severely lacking. Further balancing may improve performance on crow/magpie.

---

## 📈 Observed Performance (on-device, ANSI-C kernels)

| Input                  | Dominant class | Confidence                    |
| ---------------------- | -------------- | ----------------------------- |
| Squirrel in frame      | squirrel       | 97–99%                        |
| Magpie photo on screen | squirrel       | 56–88% (magpie 2nd at 10–39%) |
| Empty background       | background     | 98%                           |

The magpie misclassification is expected: the model was trained on real-world photos while testing used a phone screen (domain gap: glare, screen pixels vs. actual feathers).

---

### Stage 6 — Custom PCB Schematic Design

**Goal:** Replace the breadboard prototype with a purpose-built PCB that integrates all components.

**Design tool:** EasyEDA

**Key design decisions:**

- ESP32-S3-WROOM-1 N8R8 soldered directly onto PCB (no DevKit carrier board)
- Native USB Serial/JTAG (GPIO 19/20) — no external USB-UART bridge chip needed
- bq24074 charger IC integrated on-board (replaces Adafruit breakout)
- OV2640 camera via 24-pin 0.5mm FPC ribbon cable connector (copper traces, not pin headers)
- 3.3V LDO (AMS1117-3.3) stepping from battery voltage to logic rail

**New files:**

- [`hardware/README.md`](../hardware/README.md) — Full EasyEDA build guide (4 schematic sheets, complete GPIO table, design checklist)
- [`hardware/BOM.csv`](../hardware/BOM.csv) — Bill of Materials (~40 components)

**Result:** Complete netlist documentation ready to be drawn in EasyEDA. PCB layout to follow after schematic review.

---

## 🔜 Next Steps

1. **Balance dataset** — squirrel (814) dominates; add more crow/magpie images or reduce squirrel count
2. **Collect real domain data** — use `#define BIRDFEEDER_DEBUG_FRAMES` to capture actual device frames of birds at the feeder and add to training set
3. **Investigate ESP-NN fix** — consider filing a bug or updating `espressif__esp-nn` to a newer version that may fix the PSRAM tensor arena issue; restoring optimized kernels would cut inference from 1.3 s → 0.3 s
4. **Tune confidence threshold** — currently threats trigger at 70% confidence; may need adjustment based on real-world testing
