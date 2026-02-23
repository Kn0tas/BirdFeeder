import os
import numpy as np
from PIL import Image
from pathlib import Path

def generate_background_images(output_dir, count=50, img_size=96):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"Generating {count} synthetic background images in {output_dir}...")
    
    for i in range(count):
        # 1. Pure Black (simulating dead of night / lens cap)
        if i < 10:
            img_data = np.zeros((img_size, img_size, 3), dtype=np.uint8)
            filename = f"synth_black_{i}.jpg"
            
        # 2. Very Dark Noise (simulating low light sensor noise)
        elif i < 30:
            # Random noise between 0 and 20 (very dark)
            img_data = np.random.randint(0, 30, (img_size, img_size, 3), dtype=np.uint8)
            filename = f"synth_dark_noise_{i}.jpg"
            
        # 3. Gray/Uniform (simulating empty sky or wall)
        elif i < 40:
            gray_level = np.random.randint(50, 150)
            img_data = np.full((img_size, img_size, 3), gray_level, dtype=np.uint8)
            # Add slight noise to gray
            noise = np.random.randint(-10, 10, (img_size, img_size, 3), dtype=np.int16)
            img_data = np.clip(img_data + noise, 0, 255).astype(np.uint8)
            filename = f"synth_gray_{i}.jpg"

        # 4. Random Noise (simulating total sensor garbage)
        else:
            img_data = np.random.randint(0, 255, (img_size, img_size, 3), dtype=np.uint8)
            filename = f"synth_random_{i}.jpg"

        img = Image.fromarray(img_data)
        img.save(output_dir / filename)
        
    print("Done.")

if __name__ == "__main__":
    generate_background_images("datasets/background", count=100)
