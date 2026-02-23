import os
import numpy as np
from PIL import Image, ImageEnhance, ImageFile
from pathlib import Path
import random

ImageFile.LOAD_TRUNCATED_IMAGES = True

def augment_real_capture(input_path, output_dir, count=50):
    input_path = Path(input_path)
    output_dir = Path(output_dir)
    
    if not input_path.exists():
        print(f"Error: {input_path} not found")
        return

    img = Image.open(input_path)
    # Ensure RGB
    img = img.convert("RGB")
    
    print(f"Generating {count} augmented versions of {input_path}...")
    
    for i in range(count):
        # Start with original
        aug = img.copy()
        
        # 1. Random Brightness (0.5 to 1.5)
        enhancer = ImageEnhance.Brightness(aug)
        aug = enhancer.enhance(random.uniform(0.5, 1.5))
        
        # 2. Random Contrast (0.5 to 1.5)
        enhancer = ImageEnhance.Contrast(aug)
        aug = enhancer.enhance(random.uniform(0.5, 1.5))
        
        # 3. Add Noise
        if random.random() > 0.5:
            arr = np.array(aug)
            noise = np.random.randint(-20, 20, arr.shape, dtype=np.int16)
            arr = np.clip(arr + noise, 0, 255).astype(np.uint8)
            aug = Image.fromarray(arr)
            
        # 4. Tiny Random Crop/Zoom (90% to 100%)
        w, h = aug.size
        # zoom factor
        zoom = random.uniform(0.9, 1.0)
        new_w = int(w * zoom)
        new_h = int(h * zoom)
        left = random.randint(0, w - new_w)
        top = random.randint(0, h - new_h)
        aug = aug.crop((left, top, left + new_w, top + new_h))
        aug = aug.resize((96, 96)) # Resize back to model input size (optional, but good for dataset)

        aug.save(output_dir / f"real_aug_{i}.jpg")
        
    print("Done generating augmented real data.")

if __name__ == "__main__":
    # Augment BOTH captures if they exist
    if Path("datasets/background/real_capture.jpg").exists():
        augment_real_capture("datasets/background/real_capture.jpg", "datasets/background", count=50)
    
    if Path("datasets/background/real_capture_2.jpg").exists():
        augment_real_capture("datasets/background/real_capture_2.jpg", "datasets/background", count=50)

    if Path("datasets/background/real_capture_3.jpg").exists():
        augment_real_capture("datasets/background/real_capture_3.jpg", "datasets/background", count=50)
