# BirdFeeder AI Improvement Plan

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

| Component     | Details                                                           |
| ------------- | ----------------------------------------------------------------- |
| **Model**     | MobileNetV2, alpha=0.35, input 96×96×3, int8 quantized            |
| **Framework** | TensorFlow/Keras → TFLite → TFLite Micro on ESP32-S3              |
| **Classes**   | 7: crow, squirrel, rat, magpie, bird, other, unknown              |
| **Dataset**   | ~990 total images, massively imbalanced                           |
| **Training**  | 10 epochs, frozen base, no augmentation beyond basic Keras layers |
| **Inference** | ~250ms on ESP32-S3 with PSRAM                                     |
| **Camera**    | OV2640 at QQVGA (160×120), JPEG, single frame buffer              |

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

### Phase 1: Quick Wins (Data + Preprocessing Fix)

1. Fix the preprocessing mismatch between training and inference
2. Reduce classes to 4 (crow, magpie, squirrel, background)
3. Curate existing dataset (remove bad images, merge classes)
4. Source more squirrel images (at least 200-300 more)
5. Add more background images (empty feeder, generic outdoor scenes)
6. Add proper data augmentation to the training script
7. Add class weights to handle any remaining imbalance
8. Add evaluation metrics (confusion matrix, per-class scores)
9. Train and evaluate — this alone should significantly improve accuracy

### Phase 2: Fine-Tuning + Resolution

10. Implement two-phase training (frozen → fine-tune top layers)
11. Increase input resolution to 128×128
12. Add early stopping and learning rate scheduling
13. Add dropout to classifier head
14. Retrain and compare with Phase 1 results

### Phase 3: Validation + Deployment

15. Test the quantized int8 model on real images from the feeder
16. Flash to ESP32-S3 and test in real conditions
17. Iterate on dataset (add misclassified real-world images back to training)
18. Consider Edge Impulse as a comparison/alternative if accuracy is still insufficient

### Phase 4: Advanced (If Needed)

19. Try MobileNetV3-Small
20. Increase camera resolution to QVGA
21. Implement confidence thresholding with a "not sure" fallback

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
