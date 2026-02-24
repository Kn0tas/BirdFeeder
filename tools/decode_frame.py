import base64
import re
from pathlib import Path


def decode_base64_image(b64_string, output_path):
    try:
        b64_string = re.sub(r'[^A-Za-z0-9+/=]', '', b64_string)
        b64_string = b64_string.rstrip('=')

        while len(b64_string) % 4 == 1:
            b64_string = b64_string[:-1]

        missing_padding = len(b64_string) % 4
        if missing_padding:
            b64_string += '=' * (4 - missing_padding)

        img_data = base64.b64decode(b64_string)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "wb") as f:
            f.write(img_data)
        print(f"Saved image to {output_path}")
    except Exception as e:
        print(f"Error decoding: {e}")

if __name__ == "__main__":
    try:
        with open("tools/latest_frame.b64") as f:
            b64_content = f.read()

        decode_base64_image(b64_content, Path("datasets/background/real_capture_3.jpg"))
    except FileNotFoundError:
        print("tools/latest_frame.b64 not found!")
