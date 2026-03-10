#!/usr/bin/env python3
"""Capture live feeder backgrounds and generate realistic variants."""

from __future__ import annotations

import argparse
import io
import random
import time
from datetime import datetime
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter, ImageOps

CAPTURE_URL = "http://192.168.0.21/capture"
IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def list_images(path: Path) -> list[Path]:
    return sorted(p for p in path.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_EXTS)


def fetch_capture(url: str, timeout_s: float) -> Image.Image:
    req = Request(
        url,
        headers={
            "Cache-Control": "no-cache",
            "Pragma": "no-cache",
            "User-Agent": "BirdFeederBackgroundCapture/1.0",
        },
    )
    with urlopen(req, timeout=timeout_s) as response:
        data = response.read()
    return Image.open(io.BytesIO(data)).convert("RGB")


def diff_fingerprint(image: Image.Image) -> np.ndarray:
    arr = np.asarray(image.resize((32, 32), Image.Resampling.BILINEAR).convert("L"), dtype=np.float32)
    return arr / 255.0


def capture_distance(a: np.ndarray, b: np.ndarray) -> float:
    pixel_delta = float(np.mean(np.abs(a - b)))
    hist_a, _ = np.histogram(a, bins=16, range=(0.0, 1.0), density=True)
    hist_b, _ = np.histogram(b, bins=16, range=(0.0, 1.0), density=True)
    hist_delta = float(np.mean(np.abs(hist_a - hist_b))) / 2.0
    return min(1.0, 0.7 * pixel_delta + 0.3 * hist_delta)


def add_noise(arr: np.ndarray, rng: random.Random, sigma: float) -> np.ndarray:
    if sigma <= 0:
        return arr
    noise = np.random.default_rng(rng.randrange(1 << 30)).normal(0.0, sigma, arr.shape)
    return np.clip(arr + noise, 0.0, 255.0)


def apply_gamma(arr: np.ndarray, gamma: float) -> np.ndarray:
    gamma = max(0.2, gamma)
    norm = np.clip(arr / 255.0, 0.0, 1.0)
    adjusted = np.power(norm, gamma)
    return np.clip(adjusted * 255.0, 0.0, 255.0)


def apply_channel_scales(arr: np.ndarray, scales: tuple[float, float, float]) -> np.ndarray:
    out = arr.copy()
    out[..., 0] *= scales[0]
    out[..., 1] *= scales[1]
    out[..., 2] *= scales[2]
    return np.clip(out, 0.0, 255.0)


def apply_gradient_light(
    arr: np.ndarray,
    rng: random.Random,
    strength_range: tuple[float, float],
    brighten: bool,
) -> np.ndarray:
    h, w = arr.shape[:2]
    angle = rng.uniform(0.0, 2.0 * np.pi)
    xs = np.linspace(-1.0, 1.0, w, dtype=np.float32)
    ys = np.linspace(-1.0, 1.0, h, dtype=np.float32)
    xx, yy = np.meshgrid(xs, ys)
    ramp = (np.cos(angle) * xx + np.sin(angle) * yy + 1.0) * 0.5
    strength = rng.uniform(*strength_range)

    if brighten:
        mask = 1.0 + ramp * strength
    else:
        mask = 1.0 - ramp * strength

    return np.clip(arr * mask[..., None], 0.0, 255.0)


def jpeg_roundtrip(image: Image.Image, quality: int) -> Image.Image:
    buf = io.BytesIO()
    image.save(buf, format="JPEG", quality=quality, optimize=True)
    buf.seek(0)
    return Image.open(buf).convert("RGB")


def crop_rotate_resize(image: Image.Image, rng: random.Random, crop_scale: tuple[float, float], rotation_deg: float) -> Image.Image:
    w, h = image.size
    scale = rng.uniform(*crop_scale)
    crop_w = max(8, int(w * scale))
    crop_h = max(8, int(h * scale))
    max_left = max(0, w - crop_w)
    max_top = max(0, h - crop_h)
    left = rng.randint(0, max_left) if max_left else 0
    top = rng.randint(0, max_top) if max_top else 0
    cropped = image.crop((left, top, left + crop_w, top + crop_h)).resize((w, h), Image.Resampling.LANCZOS)
    if rotation_deg:
        fill = tuple(int(v) for v in np.asarray(cropped).mean(axis=(0, 1)))
        cropped = cropped.rotate(
            rng.uniform(-rotation_deg, rotation_deg),
            resample=Image.Resampling.BILINEAR,
            fillcolor=fill,
        )
    return cropped


def build_variant(image: Image.Image, index: int, rng: random.Random) -> Image.Image:
    recipe = index % 8
    aug = crop_rotate_resize(image, rng, crop_scale=(0.88, 1.0), rotation_deg=4.0)
    if rng.random() < 0.5:
        aug = ImageOps.mirror(aug)

    brightness = 1.0
    contrast = 1.0
    saturation = 1.0
    gamma = 1.0
    channel_scales = (1.0, 1.0, 1.0)
    blur_radius = 0.0
    noise_sigma = 0.0
    gradient = None
    jpeg_quality = None

    if recipe == 0:
        brightness = rng.uniform(1.08, 1.28)
        contrast = rng.uniform(0.95, 1.12)
        saturation = rng.uniform(1.0, 1.08)
        channel_scales = (rng.uniform(1.02, 1.08), 1.0, rng.uniform(0.93, 0.99))
        gradient = ("bright", (0.08, 0.18))
    elif recipe == 1:
        brightness = rng.uniform(0.72, 0.92)
        contrast = rng.uniform(0.98, 1.18)
        saturation = rng.uniform(0.9, 1.02)
        gamma = rng.uniform(1.15, 1.45)
        channel_scales = (rng.uniform(0.9, 0.98), 1.0, rng.uniform(1.02, 1.08))
        gradient = ("shadow", (0.08, 0.22))
    elif recipe == 2:
        brightness = rng.uniform(0.9, 1.05)
        contrast = rng.uniform(0.8, 0.95)
        saturation = rng.uniform(0.72, 0.9)
        channel_scales = (0.98, 1.0, rng.uniform(1.02, 1.08))
    elif recipe == 3:
        brightness = rng.uniform(0.82, 0.98)
        contrast = rng.uniform(0.95, 1.12)
        saturation = rng.uniform(0.9, 1.0)
        blur_radius = rng.uniform(0.4, 1.2)
        noise_sigma = rng.uniform(2.0, 5.0)
        jpeg_quality = rng.randint(45, 70)
    elif recipe == 4:
        brightness = rng.uniform(1.02, 1.15)
        contrast = rng.uniform(1.05, 1.22)
        saturation = rng.uniform(0.95, 1.08)
        gradient = ("bright", (0.1, 0.24))
        jpeg_quality = rng.randint(60, 80)
    elif recipe == 5:
        brightness = rng.uniform(0.65, 0.84)
        contrast = rng.uniform(0.85, 1.02)
        saturation = rng.uniform(0.82, 0.95)
        gamma = rng.uniform(1.28, 1.6)
        noise_sigma = rng.uniform(4.0, 8.0)
        gradient = ("shadow", (0.12, 0.24))
    elif recipe == 6:
        brightness = rng.uniform(0.95, 1.08)
        contrast = rng.uniform(0.92, 1.05)
        saturation = rng.uniform(1.0, 1.14)
        channel_scales = (rng.uniform(0.96, 1.03), rng.uniform(1.02, 1.08), rng.uniform(0.92, 0.99))
        blur_radius = rng.uniform(0.0, 0.6)
    else:
        brightness = rng.uniform(0.88, 1.12)
        contrast = rng.uniform(0.9, 1.1)
        saturation = rng.uniform(0.88, 1.05)
        gamma = rng.uniform(0.88, 1.12)
        noise_sigma = rng.uniform(1.0, 3.0)
        jpeg_quality = rng.randint(55, 85)

    aug = ImageEnhance.Brightness(aug).enhance(brightness)
    aug = ImageEnhance.Contrast(aug).enhance(contrast)
    aug = ImageEnhance.Color(aug).enhance(saturation)

    arr = np.asarray(aug, dtype=np.float32)
    arr = apply_gamma(arr, gamma)
    arr = apply_channel_scales(arr, channel_scales)

    if gradient:
        kind, strength_range = gradient
        arr = apply_gradient_light(arr, rng, strength_range, brighten=(kind == "bright"))

    arr = add_noise(arr, rng, noise_sigma)
    aug = Image.fromarray(arr.astype(np.uint8), mode="RGB")

    if blur_radius > 0:
        aug = aug.filter(ImageFilter.GaussianBlur(radius=blur_radius))
    if jpeg_quality:
        aug = jpeg_roundtrip(aug, jpeg_quality)

    return aug


def collect_base_captures(
    capture_url: str,
    timeout_s: float,
    interval_ms: int,
    max_attempts: int,
    min_diff: float,
    base_limit: int,
) -> list[Image.Image]:
    captures: list[Image.Image] = []
    fingerprints: list[np.ndarray] = []

    for attempt in range(max_attempts):
        try:
            image = fetch_capture(capture_url, timeout_s=timeout_s)
        except (HTTPError, URLError, TimeoutError, OSError) as exc:
            print(f"Capture attempt {attempt + 1}/{max_attempts} failed: {exc}")
            time.sleep(interval_ms / 1000.0)
            continue

        fingerprint = diff_fingerprint(image)
        nearest_distance = min((capture_distance(fingerprint, ref) for ref in fingerprints), default=1.0)

        if not fingerprints or nearest_distance >= min_diff:
            captures.append(image)
            fingerprints.append(fingerprint)
            print(
                f"Accepted capture {len(captures)}/{base_limit} from attempt {attempt + 1} "
                f"(distance={nearest_distance:.3f})"
            )
            if len(captures) >= base_limit:
                break
        else:
            print(
                f"Skipped near-duplicate at attempt {attempt + 1} "
                f"(distance={nearest_distance:.3f} < {min_diff:.3f})"
            )

        time.sleep(interval_ms / 1000.0)

    if not captures:
        raise RuntimeError("Could not fetch any capture from the feeder camera")

    return captures


def save_dataset(
    captures: list[Image.Image],
    output_dir: Path,
    additions_needed: int,
    seed: int,
) -> list[Path]:
    rng = random.Random(seed)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    saved: list[Path] = []

    base_names: list[str] = []
    for idx, image in enumerate(captures):
        base_name = f"real_capture_live_{timestamp}_{idx:02d}.jpg"
        image.save(output_dir / base_name, quality=95, optimize=True)
        saved.append(output_dir / base_name)
        base_names.append(base_name)

    variants_needed = max(0, additions_needed - len(captures))
    for idx in range(variants_needed):
        source = captures[idx % len(captures)]
        variant = build_variant(source, idx, rng)
        out_path = output_dir / f"real_bg_live_aug_{timestamp}_{idx:03d}.jpg"
        variant.save(out_path, quality=92, optimize=True)
        saved.append(out_path)

    print(f"Saved {len(base_names)} base captures and {variants_needed} augmented variants")
    return saved


def main() -> None:
    repo_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=repo_root / "datasets/background")
    parser.add_argument("--capture-url", default=CAPTURE_URL)
    parser.add_argument("--target-total", type=int, default=320, help="Target number of files in the background dataset")
    parser.add_argument("--max-attempts", type=int, default=25, help="Maximum live capture attempts before falling back to augmentation")
    parser.add_argument("--interval-ms", type=int, default=750, help="Pause between live capture attempts")
    parser.add_argument("--min-diff", type=float, default=0.03, help="Minimum capture distance required to keep another live frame")
    parser.add_argument("--base-limit", type=int, default=8, help="Maximum number of accepted live captures to store before augmentation")
    parser.add_argument("--timeout-s", type=float, default=5.0)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)

    existing_images = list_images(args.output_dir)
    if len(existing_images) >= args.target_total:
        print(
            f"{args.output_dir} already has {len(existing_images)} images, "
            f"which meets/exceeds target {args.target_total}"
        )
        return

    additions_needed = args.target_total - len(existing_images)
    print(
        f"Background dataset currently has {len(existing_images)} images; "
        f"creating {additions_needed} more using {args.capture_url}"
    )

    captures = collect_base_captures(
        capture_url=args.capture_url,
        timeout_s=args.timeout_s,
        interval_ms=args.interval_ms,
        max_attempts=args.max_attempts,
        min_diff=args.min_diff,
        base_limit=min(args.base_limit, additions_needed),
    )
    saved = save_dataset(
        captures=captures,
        output_dir=args.output_dir,
        additions_needed=additions_needed,
        seed=args.seed,
    )
    print(f"Done. Wrote {len(saved)} files to {args.output_dir}")


if __name__ == "__main__":
    main()
