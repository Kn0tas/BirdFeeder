import os
import tensorflow as tf
from pathlib import Path

def remove_corrupted_images(folder: str):
    root = Path(folder)
    removed_count = 0
    for file_path in root.rglob('*.jpg'):
        try:
            image_string = tf.io.read_file(str(file_path))
            image = tf.image.decode_jpeg(image_string, channels=3)
        except Exception as e:
            print(f"Removing corrupted image: {file_path}")
            file_path.unlink()
            removed_count += 1
            
    print(f"Cleanup complete. Removed {removed_count} corrupted images.")

if __name__ == "__main__":
    remove_corrupted_images('datasets/background')
