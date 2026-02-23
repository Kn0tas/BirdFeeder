#!/usr/bin/env python3
"""
Train a MobileNetV2 image classifier and export an int8 TFLite model for ESP32.
Updated with aggressive augmentation and 2-phase fine-tuning.
"""

import argparse
from pathlib import Path
import numpy as np
import tensorflow as tf
from PIL import Image
import os

def ensure_class_dirs(data_dir: Path, class_names):
    """Ensure each class dir exists; create a dummy image if missing."""
    for cls in class_names:
        cls_dir = data_dir / cls
        if not cls_dir.exists():
            cls_dir.mkdir(parents=True, exist_ok=True)
            # create a tiny dummy image to satisfy the loader
            img = Image.new("RGB", (32, 32), color=(128, 128, 128))
            dummy_path = cls_dir / "dummy.png"
            img.save(dummy_path)
            print(f"Created dummy directory: {cls_dir}")

def get_datasets(data_dir: Path, class_names, img_size: int, batch: int, val_split: float = 0.2, seed: int = 42):
    train_ds = tf.keras.utils.image_dataset_from_directory(
        data_dir,
        labels="inferred",
        label_mode="categorical",
        class_names=class_names,
        image_size=(img_size, img_size),
        batch_size=batch,
        shuffle=True,
        validation_split=val_split,
        subset="training",
        seed=seed,
    )
    val_ds = tf.keras.utils.image_dataset_from_directory(
        data_dir,
        labels="inferred",
        label_mode="categorical",
        class_names=class_names,
        image_size=(img_size, img_size),
        batch_size=batch,
        shuffle=True,
        validation_split=val_split,
        subset="validation",
        seed=seed,
    )
    
    # Calculate class weights for the training set
    # Note: This is an approximation based on file counts
    class_counts = {}
    total = 0
    for cls in class_names:
        path = data_dir / cls
        count = len(list(path.glob('*')))
        class_counts[cls] = count
        total += count
    
    print("Class distribution:", class_counts)
    
    # Calculate weights: total / (num_classes * class_count)
    class_indices = {name: i for i, name in enumerate(class_names)}
    class_weights = {}
    for cls, count in class_counts.items():
        if count > 0:
            weight = total / (len(class_names) * count)
            class_weights[class_indices[cls]] = weight
        else:
             class_weights[class_indices[cls]] = 1.0

    print("Class weights:", class_weights)

    autotune = tf.data.AUTOTUNE

    # --- Preprocessing in the data pipeline (NOT in the model graph) ---
    # Moving Rescaling here eliminates MUL+ADD ops from the TFLite flatbuffer.
    # Those ops cause quantisation collapse in TFLite Micro's int8 path.
    augmentation = build_data_augmentation(img_size)

    def augment_and_normalize(image, label):
        image = augmentation(image, training=True)
        image = image / 127.5 - 1.0  # [0,255] -> [-1,1]
        return image, label

    def normalize_only(image, label):
        image = tf.cast(image, tf.float32) / 127.5 - 1.0
        return image, label

    train_ds = train_ds.map(augment_and_normalize, num_parallel_calls=autotune).prefetch(autotune)
    val_ds = val_ds.map(normalize_only, num_parallel_calls=autotune).prefetch(autotune)
    return train_ds, val_ds, class_weights

def build_data_augmentation(img_size):
    return tf.keras.Sequential([
        tf.keras.layers.RandomFlip("horizontal"),
        tf.keras.layers.RandomRotation(0.2),
        tf.keras.layers.RandomZoom(0.2),
        tf.keras.layers.RandomContrast(0.2),
        tf.keras.layers.RandomBrightness(0.2),
        # RandomTranslation could be useful too
        tf.keras.layers.RandomTranslation(0.1, 0.1),
    ], name="data_augmentation")

def build_model(img_size: int, num_classes: int):
    # Model expects [-1, 1] input (preprocessing is done in the data pipeline
    # and on the device, NOT inside the model graph, to avoid MUL+ADD ops
    # that cause quantisation collapse in TFLite Micro).
    inputs = tf.keras.Input(shape=(img_size, img_size, 3))

    base = tf.keras.applications.MobileNetV2(
        input_shape=(img_size, img_size, 3),
        include_top=False,
        weights="imagenet",
        alpha=0.35,  # small footprint
    )
    base.trainable = False  # Freeze for phase 1

    x = base(inputs, training=False)
    x = tf.keras.layers.GlobalAveragePooling2D()(x)
    x = tf.keras.layers.Dropout(0.2)(x) # Add dropout
    x = tf.keras.layers.Dense(128, activation="relu")(x) # Increased dense layer size
    outputs = tf.keras.layers.Dense(num_classes, activation="softmax")(x)
    
    model = tf.keras.Model(inputs, outputs)
    return model

def convert_to_int8(model, out_tflite: Path, rep_dataset):
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = rep_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    # Use per-tensor quantization instead of per-channel.
    # Per-channel is more accurate but some TFLite Micro versions (including the one
    # bundled with ESP-IDF 5.x) have compatibility issues that cause constant output.
    # Per-tensor is simpler and reliably correct in TFLite Micro.
    converter._experimental_disable_per_channel = True
    
    tflite_model = converter.convert()
    out_tflite.parent.mkdir(parents=True, exist_ok=True)
    out_tflite.write_bytes(tflite_model)
    print(f"Saved int8 model to {out_tflite} ({out_tflite.stat().st_size} bytes)")

    # --- Post-conversion sanity check (no XNNPACK = same int8 path as device) ---
    print("\n=== Verifying int8 model (pure int8, no XNNPACK delegate) ===")
    interp = tf.lite.Interpreter(
        model_content=tflite_model,
        experimental_delegates=[]  # no XNNPACK — matches TFLite Micro behaviour
    )
    interp.allocate_tensors()
    inp_d = interp.get_input_details()[0]
    out_d = interp.get_output_details()[0]
    inp_scale, inp_zp = inp_d["quantization"]
    out_scale, out_zp = out_d["quantization"]
    print(f"  input  scale={inp_scale} zp={inp_zp}  output scale={out_scale} zp={out_zp}")

    def verify(label, uint8_arr):
        # Match device preprocessing: [0,255] -> [-1,1] -> int8 via model's scale/zp
        norm = uint8_arr.astype(np.float32) / 127.5 - 1.0
        q = np.clip(np.round(norm / inp_scale + inp_zp), -128, 127).astype(np.int8)
        interp.set_tensor(inp_d["index"], q[np.newaxis])
        interp.invoke()
        raw = interp.get_tensor(out_d["index"])[0]
        probs = (raw.astype(np.float32) - out_zp) * out_scale
        row = "  ".join(f"{p:.3f}" for p in probs)
        print(f"  {label:8s}: [{row}]  best={probs.argmax()} ({probs.max():.3f})")

    np.random.seed(0)
    verify("BLACK",  np.zeros((96, 96, 3), dtype=np.uint8))
    verify("WHITE",  np.full((96, 96, 3), 255, dtype=np.uint8))
    verify("GREY",   np.full((96, 96, 3), 128, dtype=np.uint8))
    verify("NOISE",  np.random.randint(0, 256, (96, 96, 3), dtype=np.uint8))
    verify("NOISE2", np.random.randint(0, 256, (96, 96, 3), dtype=np.uint8))
    print("  (if all rows are identical the calibration collapsed — retrain with more data)")

def main():
    repo_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-dir", type=Path, default=repo_root / "datasets", help="Dataset root")
    parser.add_argument("--epochs", type=int, default=20, help="Epochs for phase 1")
    parser.add_argument("--fine-tune-epochs", type=int, default=20, help="Epochs for phase 2")
    parser.add_argument("--img-size", type=int, default=96)
    parser.add_argument("--batch", type=int, default=32)
    parser.add_argument("--output", type=Path, default=repo_root / "main/vision/model_int8.tflite")
    args = parser.parse_args()

    labels_path = repo_root / "main/vision/labels.txt"
    if not labels_path.exists():
        print(f"Error: {labels_path} not found.")
        return

    class_names = [line.strip() for line in labels_path.read_text().splitlines() if line.strip()]
    print(f"Classes: {class_names}")

    ensure_class_dirs(args.data_dir, class_names)
    train_ds, val_ds, class_weights = get_datasets(args.data_dir, class_names, args.img_size, args.batch)

    # --- Phase 1: Train Head ---
    print("\n=== Phase 1: Training Head (Frozen Base) ===")
    model = build_model(args.img_size, num_classes=len(class_names))
    model.compile(
        optimizer=tf.keras.optimizers.Adam(1e-3),
        loss="categorical_crossentropy",
        metrics=["accuracy"],
    )
    
    callbacks = [
        tf.keras.callbacks.EarlyStopping(monitor='val_loss', patience=5, restore_best_weights=True),
    ]

    model.fit(
        train_ds, 
        validation_data=val_ds, 
        epochs=args.epochs,
        class_weight=class_weights,
        callbacks=callbacks
    )

    # --- Phase 2: Fine-Tuning ---
    print("\n=== Phase 2: Fine-Tuning (Unfrozen Base) ===")
    # Unfreeze the base model
    # base_model is likely the 'mobilenetv2_1.00_224' (or similar) layer
    base_model = None
    for layer in model.layers:
        if "mobilenet" in layer.name.lower():
            base_model = layer
            break
            
    if base_model is None:
        print("Could not find base model layer! keys:", [l.name for l in model.layers])
        return

    print(f"Fine-tuning base model layer: {base_model.name}")
    base_model.trainable = True
    
    # Fine-tune from this layer onwards
    # Let's freeze the bottom N layers and unfreeze the top
    fine_tune_at = 100 # MobileNetV2 has ~155 layers. Unfreeze top ~50.
    for layer in base_model.layers[:fine_tune_at]:
        layer.trainable = False
        
    model.compile(
        optimizer=tf.keras.optimizers.Adam(1e-5), # Lower learning rate
        loss="categorical_crossentropy",
        metrics=["accuracy"],
    )
    
    model.fit(
        train_ds,
        validation_data=val_ds,
        epochs=args.fine_tune_epochs,
        class_weight=class_weights,
        callbacks=callbacks,
        initial_epoch=len(model.history.history['loss']) if model.history else 0
    )

    # Save Keras Model
    model_path = repo_root / "build/model.keras"
    model_path.parent.mkdir(parents=True, exist_ok=True)
    model.save(model_path)
    print(f"Keras model written to {model_path}")

    # --- Quantization ---
    # Images from train_ds are already normalised to [-1, 1] float32.
    # The calibrator needs float32 samples in the same range the model expects.
    print("\n=== Converting to TFLite Int8 ===")
    def rep_gen():
        for image, _ in train_ds.unbatch().shuffle(1000, seed=42).take(400):
            yield [tf.expand_dims(tf.cast(image, tf.float32), 0)]

    convert_to_int8(model, args.output, rep_gen)

if __name__ == "__main__":
    main()
