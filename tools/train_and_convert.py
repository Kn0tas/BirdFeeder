#!/usr/bin/env python3
"""
Train a tiny image classifier and export an int8 TFLite model for the ESP32-S3.

Expected dataset layout (already in this repo):
datasets/
  crow/
  squirrel/
  rat/
  magpie/
  bird/
  other/
  (unknown is synthesized by imbalance/other)

Usage (from repo root, Python with TensorFlow installed):
  python tools/train_and_convert.py \
    --epochs 10 \
    --img-size 96 \
    --batch 32 \
    --output main/vision/model_int8.tflite

The script saves:
- SavedModel at build/saved_model/
- Quantized TFLite at the --output path (defaults to main/vision/model_int8.tflite)
"""

import argparse
from pathlib import Path
import tempfile
from PIL import Image
import tensorflow as tf


def ensure_class_dirs(data_dir: Path, class_names):
    """Ensure each class dir exists; create a dummy image if missing."""
    tmp_dirs = []
    for cls in class_names:
        cls_dir = data_dir / cls
        if not cls_dir.exists():
            cls_dir.mkdir(parents=True, exist_ok=True)
            # create a tiny dummy image to satisfy the loader
            img = Image.new("RGB", (32, 32), color=(0, 0, 0))
            dummy_path = cls_dir / "dummy.png"
            img.save(dummy_path)
            tmp_dirs.append(cls_dir)
    return tmp_dirs


def get_datasets(data_dir: Path, class_names, img_size: int, batch: int, val_split: float = 0.2, seed: int = 123):
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
    autotune = tf.data.AUTOTUNE
    train_ds = train_ds.prefetch(buffer_size=autotune)
    val_ds = val_ds.prefetch(buffer_size=autotune)
    return train_ds, val_ds, class_names


def build_model(img_size: int, num_classes: int):
    inputs = tf.keras.Input(shape=(img_size, img_size, 3))
    x = tf.keras.layers.Rescaling(1.0 / 255)(inputs)
    x = tf.keras.layers.RandomFlip("horizontal")(x)
    x = tf.keras.layers.RandomRotation(0.05)(x)
    x = tf.keras.layers.RandomZoom(0.1)(x)

    base = tf.keras.applications.MobileNetV2(
        input_shape=(img_size, img_size, 3),
        include_top=False,
        weights="imagenet",
        alpha=0.35,  # small footprint
    )
    base.trainable = False  # fine-tune later if needed

    x = base(x, training=False)
    x = tf.keras.layers.GlobalAveragePooling2D()(x)
    x = tf.keras.layers.Dense(64, activation="relu")(x)
    outputs = tf.keras.layers.Dense(num_classes, activation="softmax")(x)
    model = tf.keras.Model(inputs, outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(1e-3),
        loss="categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def convert_to_int8(saved_model_dir: Path, out_tflite: Path, rep_dataset):
    converter = tf.lite.TFLiteConverter.from_saved_model(str(saved_model_dir))
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = rep_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    tflite_model = converter.convert()
    out_tflite.parent.mkdir(parents=True, exist_ok=True)
    out_tflite.write_bytes(tflite_model)
    print(f"Saved int8 model to {out_tflite} ({out_tflite.stat().st_size} bytes)")


def main():
    repo_root = Path(__file__).resolve().parent.parent

    parser = argparse.ArgumentParser()
    parser.add_argument("--data-dir", type=Path, default=repo_root / "datasets", help="Dataset root")
    parser.add_argument("--epochs", type=int, default=10)
    parser.add_argument("--img-size", type=int, default=96)
    parser.add_argument("--batch", type=int, default=32)
    parser.add_argument("--output", type=Path, default=repo_root / "main/vision/model_int8.tflite")
    args = parser.parse_args()

    labels_path = repo_root / "main/vision/labels.txt"
    class_names = labels_path.read_text().splitlines()
    tmp_dirs = ensure_class_dirs(args.data_dir, class_names)

    train_ds, val_ds, class_names = get_datasets(args.data_dir, class_names, args.img_size, args.batch)
    model = build_model(args.img_size, num_classes=len(class_names))

    model.fit(train_ds, validation_data=val_ds, epochs=args.epochs)

    saved_model_dir = repo_root / "build/saved_model"
    saved_model_dir.parent.mkdir(parents=True, exist_ok=True)
    model.save(saved_model_dir)
    print(f"SavedModel written to {saved_model_dir}")

    # Representative dataset for quantization
    def rep_gen():
        for images, _ in train_ds.take(20):
            yield [tf.cast(images, tf.float32)]

    convert_to_int8(saved_model_dir, args.output, rep_gen)


if __name__ == "__main__":
    main()
