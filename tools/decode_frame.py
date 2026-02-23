import base64
import sys
from pathlib import Path

def decode_base64_image(b64_string, output_path):
    try:
        # Add padding if needed
        missing_padding = len(b64_string) % 4
        if missing_padding:
            b64_string += '=' * (4 - missing_padding)
            
        img_data = base64.b64decode(b64_string)
        with open(output_path, "wb") as f:
            f.write(img_data)
        print(f"Saved image to {output_path}")
    except Exception as e:
        print(f"Error decoding: {e}")

if __name__ == "__main__":
    try:
        with open("tools/latest_frame.b64", "r") as f:
            b64_content = f.read().replace('\n', '').strip()
            
        decode_base64_image(b64_content, "datasets/background/real_capture_3.jpg")
    except FileNotFoundError:
        print("tools/latest_frame.b64 not found!")
