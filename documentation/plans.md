# BirdFeeder Plans

---

# Plan 1: AI Improvement Plan

> **Status as of 2026-02-23:** Phases 1 and 2 are complete. The AI pipeline is now fully functional on-device. See [`progress.md`](progress.md) for a detailed log of all changes made.

## ⚠️ Critical Finding: ESP-NN Assembly Bug

After completing the training pipeline overhaul, the device still produced **constant identical output** for every frame. Investigation confirmed that `espressif__esp-nn`'s S3 assembly-optimized kernels (`CONFIG_NN_OPTIMIZED`, the default) have a bug when the tensor arena is in PSRAM. They read from a stale cached buffer, completely ignoring the actual input.

**Resolution:** `CONFIG_NN_ANSI_C=y` added to `sdkconfig.defaults` — switches to pure C reference kernels. Inference is slower (~1.3 s vs ~0.3 s) but correct. This should be revisited if a newer `espressif__esp-nn` version fixes the issue.

---

## 1. Executive Summary

The current AI vision system attempts to classify 7 categories (crow, squirrel, rat, magpie, bird, other, unknown) using a MobileNetV2 (alpha=0.35) model trained at 96×96 on a severely imbalanced dataset. While training metrics look good on paper (~93% validation accuracy), real-world performance is unreliable because:

- The dataset is extremely imbalanced (crow: 421, magpie: 465, squirrel: 37, bird: 37, rat: 28, other: 1, unknown: 1)
- Too many classes for the amount of data available
- No meaningful data augmentation pipeline
- The base model is entirely frozen (no fine-tuning of feature layers)
- The training script had bugs preventing model export

This plan narrows the scope to **3 target classes: crow, magpie, and squirrel** (plus a catch-all "other/unknown" class), and proposes concrete improvements to achieve reliable real-world accuracy.

---

## 2. Current System Analysis

### 2.1 What Exists Today

| Component     | Details                                                                                                 |
| ------------- | ------------------------------------------------------------------------------------------------------- |
| **Model**     | MobileNetV2, alpha=0.35, input 96×96×3, int8 quantized                                                  |
| **Framework** | TensorFlow/Keras → TFLite → TFLite Micro on ESP32-S3                                                    |
| **Classes**   | ~~7: crow, squirrel, rat, magpie, bird, other, unknown~~ → **4: crow, magpie, squirrel, background** ✅ |
| **Dataset**   | ~~990 total, massively imbalanced~~ → **1,911 images, 4 balanced classes** ✅                           |
| **Training**  | ~~10 epochs, frozen base~~ → **two-phase: head + fine-tune, early stopping** ✅                         |
| **Inference** | ~~~250ms~~ → **~1,300ms** (ANSI-C kernels; optimized kernels broken on PSRAM)                           |
| **Camera**    | OV2640 at QQVGA (160×120), JPEG, ~~single~~ **double** frame buffer ✅                                  |

### 2.2 Critical Problems Identified

#### Problem 1: Severe Dataset Imbalance

```
crow:     421 images  (42.5%)
magpie:   465 images  (47.0%)
squirrel:  37 images  ( 3.7%)
bird:      37 images  ( 3.7%)
rat:       28 images  ( 2.8%)
other:      1 image   ( 0.1%)
unknown:    1 image   ( 0.1%)
```

The model is essentially learning to guess "crow" or "magpie" for everything. Classes with 1-37 images are far too small for meaningful learning — the model has never properly learned what a squirrel looks like.

#### Problem 2: Too Many Classes for Available Data

With only ~990 images spread across 7 classes, and most classes having fewer than 40 images, the model cannot learn meaningful distinctions. The "other" and "unknown" classes with 1 image each are especially problematic — they create noise in training.

#### Problem 3: No Real Data Augmentation

The current training script includes `RandomFlip`, `RandomRotation(0.05)`, and `RandomZoom(0.1)` as **model layers**, which means:

- They only apply during training (good)
- But they are very conservative (0.05 rotation = ~3°, 0.1 zoom is barely noticeable)
- No brightness/contrast/color jitter — critical for a camera that will see varying outdoor lighting
- No cutout/erasing — important for partial occlusion robustness

#### Problem 4: Frozen Base Model

`base.trainable = False` means the MobileNetV2 backbone never adapts its features to this specific task. ImageNet features are generic and may not capture the subtle differences between crows and magpies (both are dark corvids). Fine-tuning at least the upper layers is essential.

#### Problem 5: Training Script Bugs

The `model.save()` call used a directory path instead of a `.keras` extension, causing the export to fail. The training completed but no usable model was saved for conversion. The current `model_int8.tflite` was likely produced from a different/earlier run.

#### Problem 6: Input Resolution May Be Too Low

At 96×96 pixels, distinguishing a crow (uniformly black) from a magpie (black + white patches + long tail) is already challenging. The camera captures at 160×120 (QQVGA) which is then downscaled further. Key distinguishing features like the magpie's white belly and long tail may be lost.

---

## 3. Proposed Changes

### 3.1 Simplify to 3+1 Classes

**Reduce from 7 classes to 4:**

| Class        | Purpose                                                      |
| ------------ | ------------------------------------------------------------ |
| `crow`       | Hooded/carrion crows — threat                                |
| `magpie`     | Eurasian magpies — threat                                    |
| `squirrel`   | Grey/red squirrels — threat                                  |
| `background` | Everything else (empty feeder, other birds, unknown objects) |

**Rationale:**

- Fewer classes = simpler decision boundary = higher accuracy with limited data
- "rat", "bird", "other", "unknown" all merge into `background`
- The feeder only needs to know if a threat is present, not distinguish between benign visitors
- Existing rat/bird/other images become useful as `background` training data

**Impact on firmware:**

- `vision_kind_t` enum simplifies to 4 values
- `labels.txt` updates to 4 lines
- Threat detection logic in `app_main.c` remains similar

### 3.2 Expand and Balance the Dataset

This is the single most impactful change. The model needs significantly more data, especially for squirrels.

#### 3.2.1 Target Dataset Size

| Class        | Current | Target  | Strategy                                                                                |
| ------------ | ------- | ------- | --------------------------------------------------------------------------------------- |
| `crow`       | 421     | 400-500 | Keep, curate (remove bad images)                                                        |
| `magpie`     | 465     | 400-500 | Keep, curate                                                                            |
| `squirrel`   | 37      | 400-500 | **Augment heavily + add images from public datasets**                                   |
| `background` | 67      | 400-500 | Merge existing rat/bird/other/unknown + add empty feeder shots + generic outdoor images |

#### 3.2.2 Where to Get More Images

1. **Public datasets (recommended):**
   - [Kaggle "Animals-10"](https://www.kaggle.com/datasets/alessiocorrado99/animals10) — contains squirrel images
   - [iNaturalist](https://www.inaturalist.org/) — search for crow, magpie, squirrel photos
   - [Caltech-UCSD Birds (CUB-200)](http://www.vision.caltech.edu/datasets/cub_200_2011/) — crow/magpie classes
   - [Open Images Dataset](https://storage.googleapis.com/openimages/web/index.html) — "squirrel" class available
   - Google Images (manual download with verification)

2. **Capture from the device itself:**
   - Use `camera_dump_base64()` to capture real-world images from the feeder camera
   - These are the **most valuable** because they match deployment conditions exactly
   - Capture images at different times of day, weather conditions, with and without animals

3. **Synthetic augmentation** (done in the training pipeline — see §3.3)

> [!IMPORTANT]
> Images downloaded from public datasets should be reviewed manually to ensure they are relevant (e.g., photos of squirrels from the front, side, above — angles the feeder camera might see). Remove any images that show the animal at angles or distances that would never occur at a bird feeder.

#### 3.2.3 Image Quality Guidelines

- Prefer images where the subject fills a reasonable portion of the frame
- Include variety: different angles, lighting, distances, partial occlusion
- For `background`: include empty feeder, leaves, rain, other small birds, shadows
- Avoid: cartoons, drawings, heavily processed images, extremely distant shots

### 3.3 Improve the Training Pipeline

Replace the current `tools/train_and_convert.py` with a more robust pipeline.

#### 3.3.1 Better Data Augmentation

Add aggressive augmentation to compensate for limited data:

| Technique             | Parameter       | Rationale                                                 |
| --------------------- | --------------- | --------------------------------------------------------- |
| Horizontal flip       | 50% probability | Birds/squirrels can face either direction                 |
| Rotation              | ±15°            | Camera may not be perfectly level, animals shift position |
| Zoom                  | 0.8–1.2×        | Distance variation                                        |
| Brightness            | ±30%            | Outdoor lighting changes dramatically                     |
| Contrast              | ±30%            | Shadows, overcast vs sunny                                |
| Saturation            | ±20%            | Camera white balance variation                            |
| Random erasing/cutout | 10-20% of image | Simulates partial occlusion by feeder parts               |
| Gaussian noise        | Light           | Simulates JPEG compression artifacts from OV2640          |

**Implementation:** Use the `tf.keras.layers` augmentation layers (already partially used) or switch to `tf.image` / `albumentations` in the data pipeline for more control.

#### 3.3.2 Fine-Tune the Base Model

Instead of freezing the entire MobileNetV2 backbone:

1. **Phase 1 (5-10 epochs):** Train with frozen backbone (as today) — let the classifier head converge
2. **Phase 2 (10-20 epochs):** Unfreeze the top 30-50 layers of MobileNetV2 — use a very low learning rate (1e-5 to 1e-4) to fine-tune feature extraction
3. Use learning rate scheduling (e.g., cosine decay or reduce-on-plateau)

This allows the model to adapt its feature extraction to the specific textures and patterns of crows, magpies, and squirrels — rather than relying on generic ImageNet features.

#### 3.3.3 Training Configuration Changes

| Setting          | Current    | Proposed                        | Why                                              |
| ---------------- | ---------- | ------------------------------- | ------------------------------------------------ |
| Epochs           | 10         | 30-50 (with early stopping)     | More training with fine-tuning needs more epochs |
| Learning rate    | 1e-3 fixed | 1e-3 → 1e-5 (scheduled)         | High for head, low for fine-tuning               |
| Validation split | 20%        | 20% (stratified)                | Ensure each class is represented in validation   |
| Batch size       | 32         | 16-32                           | Smaller batches can help with small datasets     |
| Early stopping   | None       | patience=5, monitor val_loss    | Prevent overfitting                              |
| Class weights    | None       | Computed from class frequencies | Balance loss contribution across classes         |

#### 3.3.4 Add Evaluation Metrics

The current script only reports accuracy. Add:

- **Confusion matrix** — see exactly which classes are being confused
- **Per-class precision, recall, F1** — crucial for imbalanced datasets
- **Confidence distribution histogram** — understand if the model is confident or guessing

### 3.4 Model Architecture Alternatives

#### Option A: Keep MobileNetV2 (Recommended to start)

**MobileNetV2 alpha=0.35 is a reasonable choice** for ESP32-S3. Before switching architectures, fix the data and training pipeline first — this will have the largest impact.

Potential tweaks:

- Try `alpha=0.50` — slightly larger model but better feature extraction. Check if it still fits in the 2MB tensor arena.
- The classifier head could use dropout: `Dense(64, relu) → Dropout(0.3) → Dense(4, softmax)`

#### Option B: Edge Impulse (Alternative approach)

[Edge Impulse](https://edgeimpulse.com/) is a platform specifically designed for embedded ML:

**Pros:**

- End-to-end workflow: upload images → train → download optimized TFLite model
- Automatic model optimization and quantization
- EON compiler can reduce model size by up to 50% vs standard TFLite
- Built-in data augmentation and testing tools
- Visual confusion matrix and performance metrics
- Direct ESP32-S3 deployment support
- Free tier available for personal projects

**Cons:**

- Less control over model internals
- Cloud-based (data uploaded to their servers)
- Some users report accuracy differences between Edge Impulse validation and real-world deployment
- Adds a platform dependency

**Verdict:** Edge Impulse is worth trying as a quick experiment. Upload the curated dataset, train a model, and compare accuracy against the custom TF pipeline. If Edge Impulse achieves better real-world accuracy with less effort, it may be the pragmatic choice.

#### Option C: MobileNetV3-Small

If MobileNetV2 proves inadequate even after data improvements:

- MobileNetV3-Small is designed for edge devices and is more efficient than V2
- Available via `tf.keras.applications.MobileNetV3Small`
- May extract better features at the same computational cost
- **Risk:** TFLite Micro support for all MobileNetV3 ops needs verification

### 3.5 Increase Input Resolution

#### Option 1: Bump to 128×128 (Recommended)

- ~78% more pixels than 96×96
- MobileNetV2 handles this well
- Model size increases moderately
- Should still fit in 2MB tensor arena
- Better captures the magpie's distinctive white patches and long tail

#### Option 2: Bump to 160×160

- Matches the camera's QQVGA width exactly (no downscaling needed on one axis)
- More detail, but significantly larger tensor arena needed
- May push inference time beyond 250ms
- Test if it fits within memory constraints

> [!WARNING]
> Increasing resolution increases model size and inference time. Always test on the actual ESP32-S3 hardware after changing resolution. If the tensor arena overflows, the model will fail to load.

### 3.6 Camera Configuration Improvements

The current camera runs at QQVGA (160×120) with JPEG quality 20 (high compression). Consider:

- **Increase JPEG quality to 10-15** (lower number = higher quality in esp_camera). Less compression = fewer artifacts = cleaner input for the model
- **Consider FRAMESIZE_QVGA (320×240)** if PSRAM can handle it — more detail for the model to work with, even after downscaling
- **White balance and exposure**: The OV2640 has auto-exposure. Verify it's working well in the outdoor conditions where the feeder is deployed

### 3.7 Update Firmware Vision Module

The firmware changes to support the new model:

1. **`labels.txt`**: Update to 4 classes: `crow`, `magpie`, `squirrel`, `background`
2. **`vision.h`**: Simplify `vision_kind_t` enum to match new classes
3. **`vision.cpp`**: Update `kLabelToKind` array, adjust input resolution if changed
4. **`app_main.c`**: Update threat detection logic and kind-to-string mapping
5. **`model_int8.tflite`**: Replace with newly trained model

### 3.8 Preprocessing Consistency

> [!CAUTION]
> **Critical issue:** The training pipeline and inference pipeline MUST preprocess images identically. Currently:
>
> - **Training:** `Rescaling(1.0 / 127.5, offset=-1.0)` → maps [0,255] to [-1, 1]
> - **Inference:** `real = rgb[i] / 255.0f` then quantize with scale/zero_point
>
> The inference code normalizes to [0, 1], but MobileNetV2 expects [-1, 1]. This mismatch likely degrades accuracy significantly. This MUST be fixed regardless of any other changes.

**Fix:** Either:

- Include the rescaling as part of the model (so it's baked into the TFLite file), OR
- Update the inference code to normalize to [-1, 1] to match training

The current setup technically handles this through quantization parameters, but needs verification that the TFLite conversion properly bakes in the preprocessing.

---

## 4. Recommended Implementation Order

### Phase 1: Quick Wins (Data + Preprocessing Fix) ✅ COMPLETE

1. ✅ Fix the preprocessing mismatch between training and inference
2. ✅ Reduce classes to 4 (crow, magpie, squirrel, background)
3. ✅ Curate existing dataset (remove bad images, merge classes)
4. ✅ Source more squirrel images (814 total, up from 37)
5. ✅ Add more background images (215 total)
6. ✅ Add proper data augmentation to the training script
7. ✅ Add class weights to handle any remaining imbalance
8. ✅ Add evaluation metrics (post-conversion self-test)
9. ✅ Train and evaluate

### Phase 2: Fine-Tuning + Runtime Fix ✅ COMPLETE

10. ✅ Implement two-phase training (frozen → fine-tune top layers)
11. ⬜ Increase input resolution to 128×128 _(deferred — 96×96 working, revisit if accuracy too low)_
12. ✅ Add early stopping and learning rate scheduling
13. ✅ Add dropout to classifier head
14. ✅ Fix ESP-NN PSRAM bug (`CONFIG_NN_ANSI_C=y`)

### Phase 3: Validation + Deployment ← CURRENT

15. ✅ Test quantized int8 model on synthetic inputs (self-test)
16. ✅ Flash to ESP32-S3 — model is dynamic and responding to input
17. ⬜ Iterate on dataset — capture real misclassified frames and add to training
18. ⬜ Consider Edge Impulse as a comparison if accuracy insufficient

### Phase 4: Advanced (If Needed)

19. ⬜ Try MobileNetV3-Small
20. ⬜ Increase camera resolution to QVGA
21. ⬜ Implement confidence thresholding with a "not sure" fallback
22. ⬜ Investigate ESP-NN fix in newer `espressif__esp-nn` release to restore ~0.3s inference

---

## 5. Expected Accuracy Improvements

| Change                           | Expected Impact                                                                        |
| -------------------------------- | -------------------------------------------------------------------------------------- |
| Fix preprocessing mismatch       | **High** — currently the model may be receiving incorrectly scaled inputs at inference |
| Reduce to 4 classes              | **Medium-High** — simpler decision boundary, fewer classes to confuse                  |
| Balance dataset (esp. squirrels) | **High** — model currently has never properly learned squirrels                        |
| Better augmentation              | **Medium** — improves generalization to real-world conditions                          |
| Fine-tune base model             | **Medium-High** — adapts features to corvid-specific patterns                          |
| Increase resolution              | **Medium** — more detail, especially helps crow vs magpie                              |
| Class weights                    | **Medium** — ensures minority classes contribute proportionally to loss                |

With all Phase 1 + Phase 2 changes, a realistic target is **85-95% real-world accuracy** for the 3 threat classes, up from what is likely <50% usable accuracy today.

---

## 6. Key Visual Differences to Leverage

For the model to succeed, it needs to pick up on these distinguishing features:

### Crow vs Magpie (Most challenging — both are corvids)

- **Magpies** have a distinctive **black-and-white** pattern (white belly, white wing patches) vs. crows which are **uniformly black**
- Magpies have a **very long, wedge-shaped tail** (roughly body-length) vs. crows with a **short, square tail**
- Magpies show **iridescent blue-green** on wings and tail (subtle but visible in good light)
- Magpies are smaller (~45cm) than crows (~50cm)

### Squirrel vs Birds

- **Body shape:** squirrels have a distinctive **hunched, bushy-tailed mammalian** shape vs. birds' **upright, feathered** silhouette
- **Movement:** squirrels tend to be on all fours; birds hop/perch upright
- **Color:** grey/red-brown fur vs. black/white/colored feathers
- **Size relative to feeder:** squirrels are typically larger than individual birds at the feeder

At 96×96 resolution, color patterns (especially the magpie's white patches) and general silhouette shape are the most reliable features. The long tail of the magpie and the bushy tail of the squirrel should be captured even at low resolution.

---

## 7. Tools and Dependencies

| Tool                      | Purpose                       | Notes                         |
| ------------------------- | ----------------------------- | ----------------------------- |
| TensorFlow 2.x + Keras    | Model training                | Already in use                |
| TFLite converter          | int8 quantization             | Already in use                |
| ESP-IDF 5.2-5.5           | Firmware build                | Already in use                |
| TFLite Micro              | On-device inference           | Already integrated            |
| scikit-learn              | Confusion matrix, metrics     | Add to training script        |
| matplotlib                | Visualization of results      | Add to training script        |
| albumentations (optional) | Advanced augmentation         | Alternative to Keras built-in |
| Edge Impulse (optional)   | Alternative training platform | For comparison testing        |

---

## 8. Risk Assessment

| Risk                                                    | Likelihood | Mitigation                                                                   |
| ------------------------------------------------------- | ---------- | ---------------------------------------------------------------------------- |
| Can't find enough squirrel images                       | Low        | Multiple public datasets available; augmentation helps                       |
| Model too large for 2MB arena after resolution increase | Medium     | Test incrementally; fall back to 96×96 if needed                             |
| Crow/magpie still confused after improvements           | Medium     | Fine-tuning + higher resolution; worst case merge into single "corvid" class |
| Quantization degrades accuracy too much                 | Low        | Use representative dataset for quantization calibration                      |
| New model fails to load on ESP32-S3                     | Low        | Test op resolver compatibility; add any missing ops                          |

---

# Plan 2: Code Cleanup & Refactoring

> **Status as of 2026-02-24:** ✅ All items complete. Identified during a full code review. All items were minor/cosmetic — the codebase architecture is solid and no structural refactoring was needed.

### 1. Clean Up Stray Root Files

Several debug/development leftover files sit in the project root and add clutter:

| File                    | Action | Reason                                                                       |
| ----------------------- | ------ | ---------------------------------------------------------------------------- |
| `out.txt`               | Delete | Debug output artifact                                                        |
| `out2.txt`              | Delete | Debug output artifact                                                        |
| `final_error.log`       | Delete | One-off error log (`.gitignore` already excludes `*.log`)                    |
| `training_log.txt`      | Delete | Old training log                                                             |
| `test_tflite.py` (root) | Delete | Duplicate — a more complete version already exists at `tools/test_tflite.py` |

### 2. Move `max17048` Into `sensors/`

The MAX17048 fuel gauge driver (`main/max17048.c`, `main/max17048.h`) sits directly in `main/` while all other sensor drivers live under `main/sensors/`. Move it for consistency:

- `main/max17048.c` → `main/sensors/max17048.c`
- `main/max17048.h` → `main/sensors/max17048.h`
- Update `#include "max17048.h"` → `#include "sensors/max17048.h"` in `app_main.c`
- Update `main/CMakeLists.txt` SRCS entry

### 3. Consolidate Wi-Fi Credential Configuration

There are currently **two** mechanisms for Wi-Fi credentials that conflict:

1. **`main/wifi_secrets.h`** — hardcoded `#define WIFI_SSID` / `WIFI_PASSWORD` (actually used by `wifi.c`)
2. **`main/Kconfig.projbuild`** — defines `CONFIG_WIFI_SSID` / `CONFIG_WIFI_PASSWORD` via menuconfig (never referenced)

**Action:** Remove `Kconfig.projbuild` since `wifi_secrets.h` is the active mechanism and is already `.gitignore`d. The Kconfig approach would be better in theory (no secret files), but switching would require updating `wifi.c` to use `CONFIG_WIFI_SSID` instead — not worth the churn right now.

### 4. Remove Debug Logging in Unused Code Path

In `vision.cpp`, the `kTfLiteUInt8` output branch (lines 260–269) contains per-class `ESP_LOGI` logging that would be noisy in production. The model uses `kTfLiteInt8` output, so this path is never hit — but if it ever is, the logging should not be there.

**Action:** Remove the `ESP_LOGI` call inside the uint8 for-loop, or guard it with `#ifdef BIRDFEEDER_DEBUG_VISION`.

### 5. Verify `wifi_secrets.h` Is Not Tracked With Real Credentials

`.gitignore` lists `main/wifi_secrets.h`, but if it was committed before the gitignore rule was added, it may still be in Git history.

**Action:** Run `git log --oneline -- main/wifi_secrets.h` to check. If it was ever committed with real credentials, consider rewriting history or rotating credentials.

### 6. Annotate Stub Modules

Three modules are currently empty stubs:

| Module            | Status                                                           |
| ----------------- | ---------------------------------------------------------------- |
| `power_manager.c` | Two `TODO` functions, no implementation                          |
| `ota.c`           | `ota_init()` logs "stub" and returns OK                          |
| `events.c`        | Just wraps `ESP_LOGI` — no FRAM persistence or structured events |

**Action:** No code change needed. These are intentional scaffolding for future features. Optionally add `@todo` Doxygen tags or link them to GitHub issues for tracking.

### Implementation Priority

All items are low-priority. Suggested order if you want to tackle them:

1. **§1** — Clean up stray files (2 minutes, instant improvement)
2. **§5** — Verify credentials aren't leaked (security check)
3. **§3** — Remove dead `Kconfig.projbuild` (reduces confusion)
4. **§2** — Move `max17048` to `sensors/` (keeps structure clean)
5. **§4** — Guard debug logging (minor code hygiene)
6. **§6** — Annotate stubs (documentation only)

---

# Plan 3: Automated Testing

> **Status as of 2026-02-24:** Planning phase. No automated tests exist yet.

## Overview

The BirdFeeder firmware has zero automated tests. This plan introduces two tiers of testing that can run **without hardware** (no ESP32 board required):

| Tier                 | What                                           | Runs On                  | Framework                                                  |
| -------------------- | ---------------------------------------------- | ------------------------ | ---------------------------------------------------------- |
| **Host unit tests**  | Pure C/C++ logic (math, conversions, mappings) | PC (Windows/Linux/macOS) | [Unity](https://github.com/ThrowTheSwitch/Unity) via CMake |
| **Model validation** | TFLite int8 model inference sanity checks      | PC with Python           | pytest + TensorFlow Lite                                   |

> [!NOTE]
> On-target integration tests (running on the actual ESP32-S3) are intentionally **out of scope** for now. They require a physical board, serial connection, and are harder to automate. Focus first on catching logic bugs cheaply on the host.

---

## 1. Host-Based Unit Tests (C/C++)

### 1.1 Testable Functions

These are **pure functions** with no hardware dependencies — they take inputs and return outputs with no side effects:

| File                 | Function                        | What It Tests                                                         |
| -------------------- | ------------------------------- | --------------------------------------------------------------------- |
| `actuators/servo.c`  | `servo_pulse_to_duty(pulse_us)` | PWM duty cycle calculation from pulse width                           |
| `actuators/servo.c`  | `clamp_pulse(pulse)`            | Pulse clamping to `[SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US]`          |
| `vision/vision.cpp`  | `clamp_int(value, lo, hi)`      | Integer clamping utility                                              |
| `vision/vision.cpp`  | `tensor_element_count(tensor)`  | Product of tensor dimensions                                          |
| `vision/vision.cpp`  | Label-to-kind mapping           | `kLabelToKind[]` maps indices to correct `vision_kind_t` values       |
| `app_main.c`         | `get_vision_kind_str(kind)`     | Enum → string mapping                                                 |
| `app_main.c`         | Threat detection logic          | `is_threat` = (crow OR magpie OR squirrel) AND confidence ≥ threshold |
| `sensors/max17048.c` | Voltage calculation             | `vraw >> 4` then `* 1.25f / 1000.0f` conversion                       |
| `sensors/max17048.c` | SOC calculation                 | `buf[0] + buf[1] / 256.0f` conversion                                 |
| `storage/fram.c`     | Address bounds checking         | Rejects `addr + len > FRAM_CAP_BYTES`                                 |

### 1.2 Approach: Extract-and-Test

Since these functions are currently `static` or interleaved with hardware code, the approach is:

1. **Extract pure logic** into small, separate `.c/.h` files (e.g., `servo_math.h`, `vision_utils.h`, `threat_logic.h`)
2. **Keep the originals calling the extracted functions** — no behavior change
3. **Write host-compiled tests** that `#include` only the extracted headers — no ESP-IDF dependency

### 1.3 Directory Structure

```
test/
├── CMakeLists.txt              ← Host CMake build (NOT idf.py)
├── unity/                      ← Unity test framework (git submodule or copy)
│   ├── unity.c
│   ├── unity.h
│   └── unity_internals.h
├── test_servo_math.c           ← Tests for servo pulse/duty calculations
├── test_vision_utils.c         ← Tests for clamp, tensor_element_count, label mapping
├── test_threat_logic.c         ← Tests for threat detection decision logic
├── test_sensor_conversions.c   ← Tests for MAX17048 voltage/SOC math
├── test_fram_bounds.c          ← Tests for FRAM address validation
└── test_main.c                 ← Unity test runner (calls all test suites)
```

### 1.4 Extracted Logic Files

```
main/
├── actuators/
│   ├── servo_math.h            ← [NEW] servo_pulse_to_duty(), clamp_pulse()
│   └── servo.c                 ← Calls servo_math.h functions
├── vision/
│   ├── vision_utils.h          ← [NEW] clamp_int(), tensor_element_count()
│   └── vision.cpp              ← Calls vision_utils.h functions
├── threat_logic.h              ← [NEW] is_threat(), get_vision_kind_str()
├── app_main.c                  ← Calls threat_logic.h functions
└── sensors/
    └── max17048_math.h         ← [NEW] raw_to_voltage(), raw_to_soc()
```

### 1.5 How to Run

```powershell
cd test
cmake -B build
cmake --build build
./build/test_runner        # Runs all Unity tests, prints PASS/FAIL
```

### 1.6 Example Test Cases

```c
// test_servo_math.c
void test_clamp_pulse_within_range(void) {
    TEST_ASSERT_EQUAL_UINT32(1500, clamp_pulse(1500));
}
void test_clamp_pulse_below_min(void) {
    TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MIN_US, clamp_pulse(100));
}
void test_clamp_pulse_above_max(void) {
    TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MAX_US, clamp_pulse(5000));
}

// test_threat_logic.c
void test_crow_high_confidence_is_threat(void) {
    TEST_ASSERT_TRUE(is_threat(VISION_RESULT_CROW, 0.95f, 0.80f));
}
void test_background_is_not_threat(void) {
    TEST_ASSERT_FALSE(is_threat(VISION_RESULT_BACKGROUND, 0.95f, 0.80f));
}
void test_crow_low_confidence_is_not_threat(void) {
    TEST_ASSERT_FALSE(is_threat(VISION_RESULT_CROW, 0.50f, 0.80f));
}
```

---

## 2. Python Model Validation Tests

### 2.1 What to Test

An existing `tools/test_tflite.py` script already runs inference on synthetic images (black, noise). This will be formalized into a proper pytest suite:

| Test                            | Input                     | Expected Behavior                            |
| ------------------------------- | ------------------------- | -------------------------------------------- |
| Model loads                     | —                         | No crashes, correct schema version           |
| Input shape                     | —                         | `[1, 96, 96, 3]`, dtype `int8`               |
| Output shape                    | —                         | `[1, 4]`, dtype `int8`                       |
| Black image                     | `(0,0,0)`                 | Should classify as `background` (class 3)    |
| White image                     | `(255,255,255)`           | Should classify as `background` (class 3)    |
| Random noise                    | Random uint8              | Output probabilities should sum to ~1.0      |
| All classes have representation | 4 class outputs           | No class is permanently stuck at 0.0         |
| Outputs are dynamic             | Multiple different inputs | Outputs differ between inputs (not constant) |

### 2.2 Directory Structure

```
tools/
├── test_model.py               ← [NEW] pytest-based model validation
├── conftest.py                 ← [NEW] Shared fixtures (model path, interpreter)
└── test_tflite.py              ← Existing ad-hoc script (kept as reference)
```

### 2.3 How to Run

```powershell
pip install pytest tensorflow numpy Pillow
cd tools
pytest test_model.py -v
```

---

## 3. Implementation Phases

### Phase 1: Python Model Tests (Quick Win)

The easiest to implement since `tools/test_tflite.py` already has most of the logic. Just restructure it as a proper pytest suite.

- [ ] Create `tools/test_model.py` with pytest test cases
- [ ] Create `tools/conftest.py` with shared interpreter fixture
- [ ] Verify all tests pass on current model

### Phase 2: Host Unit Tests

Requires extracting pure logic into headers first, then writing tests.

- [ ] Add Unity framework to `test/unity/`
- [ ] Create `test/CMakeLists.txt` for host-only builds
- [ ] Extract `servo_math.h`, `vision_utils.h`, `threat_logic.h`, `max17048_math.h`
- [ ] Update original files to call extracted functions
- [ ] Write test files for each extracted module
- [ ] Create `test/test_main.c` runner
- [ ] Verify all tests pass

### Phase 3: CI Integration ✅

- [x] Add GitHub Actions workflow to run tests on push and PR

---

# Plan 4: Extended CI Pipeline

> **Status as of 2026-02-24:** Planning phase.

## Overview

The current CI runs unit tests and model validation. This plan adds **four more jobs** to catch issues earlier:

| Job                   | Tool                                         | What It Catches                                                     | Run Time |
| --------------------- | -------------------------------------------- | ------------------------------------------------------------------- | -------- |
| **C static analysis** | [cppcheck](https://cppcheck.sourceforge.io/) | Null derefs, buffer overflows, unused functions, undefined behavior | ~10s     |
| **Python linting**    | [Ruff](https://docs.astral.sh/ruff/)         | Bugs, imports, style issues in training/tool scripts                | ~2s      |
| **ESP-IDF build**     | `espressif/idf` Docker image                 | Firmware actually compiles after refactoring                        | ~3 min   |
| **Model size guard**  | Shell script                                 | `.tflite` model doesn't exceed the 2MB tensor arena budget          | ~1s      |

---

## 1. C Static Analysis (cppcheck)

### What

Run `cppcheck` on all firmware C/C++ source files. This catches real bugs that GCC misses:

- Null pointer dereferences
- Buffer overflows
- Memory leaks (malloc without free)
- Unused functions / dead code
- Uninitialized variables

### How

```yaml
cppcheck:
  name: C Static Analysis
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - run: sudo apt-get install -y cppcheck
    - name: Run cppcheck
      run: |
        cppcheck --enable=warning,performance,portability \
                 --error-exitcode=1 \
                 --suppress=missingIncludeSystem \
                 --inline-suppr \
                 -I main -I main/sensors -I main/actuators \
                 -I main/vision -I main/comms -I main/storage \
                 -I main/logging \
                 main/
```

> [!NOTE]
> `--suppress=missingIncludeSystem` is needed since ESP-IDF system headers (`esp_log.h`, `driver/gpio.h`, etc.) won't be present in CI. cppcheck still analyzes our code's logic.

### Suppressions

If cppcheck produces false positives on specific lines, suppress them inline:

```c
// cppcheck-suppress unusedFunction
esp_err_t some_callback(void) { ... }
```

---

## 2. Python Linting (Ruff)

### What

Lint all Python scripts in `tools/` with [Ruff](https://docs.astral.sh/ruff/) — an extremely fast Python linter that replaces flake8, isort, and pyflakes.

### How

```yaml
python-lint:
  name: Python Lint
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-python@v5
      with:
        python-version: "3.11"
    - run: pip install ruff
    - name: Lint Python files
      run: ruff check tools/
```

### Configuration

Add a `[tool.ruff]` section in a `pyproject.toml` at the project root:

```toml
[tool.ruff]
target-version = "py311"
line-length = 120

[tool.ruff.lint]
select = ["E", "F", "W", "I", "UP", "B"]
ignore = ["E501"]  # Allow long lines in data-heavy scripts
```

---

## 3. ESP-IDF Build Verification

### What

Verify the firmware actually **compiles** against ESP-IDF. This is the most valuable check — it catches broken includes, missing source files in CMakeLists.txt, and type errors that host-GCC won't see.

### How

```yaml
esp-idf-build:
  name: ESP-IDF Build
  runs-on: ubuntu-latest
  container: espressif/idf:v5.4
  steps:
    - uses: actions/checkout@v4
    - name: Build firmware
      shell: bash
      run: |
        . $IDF_PATH/export.sh
        idf.py set-target esp32s3
        idf.py build
```

> [!IMPORTANT]
> This uses the official `espressif/idf` Docker image, which is ~2GB but cached by GitHub Actions after the first run. Build time is ~3 minutes.

### Wi-Fi Secrets Handling

The build needs `main/wifi_secrets.h` which isn't committed. The workflow step should create a dummy one:

```yaml
- name: Create dummy wifi_secrets.h
  run: cp main/wifi_secrets.example.h main/wifi_secrets.h
```

---

## 4. Model Size Guard

### What

Fail the build if the `.tflite` model exceeds the tensor arena budget. The arena is 2MB (2,097,152 bytes), and the model should be well under that.

### How

```yaml
model-size-check:
  name: Model Size Guard
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Check model size
      run: |
        MAX_BYTES=1500000  # 1.5MB — leaves headroom for arena overhead
        SIZE=$(stat --printf="%s" main/vision/model_int8.tflite)
        echo "Model size: $SIZE bytes (limit: $MAX_BYTES)"
        if [ "$SIZE" -gt "$MAX_BYTES" ]; then
          echo "::error::Model too large ($SIZE > $MAX_BYTES bytes)"
          exit 1
        fi
```

---

## 5. Implementation Steps

- [ ] Add `cppcheck` job to `.github/workflows/test.yml`
- [ ] Add `ruff` lint job to `.github/workflows/test.yml`
- [ ] Create `pyproject.toml` with ruff configuration
- [ ] Add ESP-IDF build job (with dummy `wifi_secrets.h`)
- [ ] Add model size guard job
- [ ] Push and verify all jobs pass in CI
