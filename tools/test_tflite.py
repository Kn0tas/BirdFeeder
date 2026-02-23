import tensorflow as tf
import numpy as np
from PIL import Image
import argparse

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", default="main/vision/model_int8.tflite")
    args = parser.parse_args()

    # Load TFLite model and allocate tensors
    interpreter = tf.lite.Interpreter(model_path=args.model)
    interpreter.allocate_tensors()

    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    print("Input details:", input_details)
    print("Output details:", output_details)
    
    # Check input expectations
    scale, zero_point = input_details[0]['quantization']
    print(f"Input Quantization: scale={scale}, zero_point={zero_point}")
    
    # Create a BLACK image (0,0,0)
    # Simulate what happens if camera sees darkness
    img = Image.new("RGB", (96, 96), color=(0, 0, 0))
    input_data = np.array(img, dtype=np.uint8)
    input_data = np.expand_dims(input_data, axis=0) # (1, 96, 96, 3)

    # Quantize input
    # If the model expects quantized inputs, we need to convert [0..255] float to int8
    # q = (real / scale) + zero_point
    
    input_tensor = input_data.astype(np.float32)
    input_tensor = (input_tensor / scale) + zero_point
    input_tensor = np.clip(input_tensor, -128, 127).astype(np.int8)

    interpreter.set_tensor(input_details[0]['index'], input_tensor)
    interpreter.invoke()

    output_data = interpreter.get_tensor(output_details[0]['index'])
    
    # Dequantize output
    out_scale, out_zero_point = output_details[0]['quantization']
    output_probs = (output_data.astype(np.float32) - out_zero_point) * out_scale
    
    print("\nPrediction on BLACK image:")
    # print(f"Raw Output (int8): {output_data}")
    # print(f"Probabilities: {output_probs}")
    
    classes = ["CROW", "MAGPIE", "SQUIRREL", "BACKGROUND"]
    mapped = dict(zip(classes, output_probs[0]))
    for k, v in mapped.items():
        print(f"  {k}: {v:.4f}")

    # Test RANDOM noise
    print("\nPrediction on RANDOM noise:")
    noise = np.random.randint(0, 255, (1, 96, 96, 3), dtype=np.uint8)
    # Quantize input
    input_tensor = noise.astype(np.float32)
    # Note: scale/zero_point from input details
    input_tensor = (input_tensor / scale) + zero_point
    input_tensor = np.clip(input_tensor, -128, 127).astype(np.int8)
    
    interpreter.set_tensor(input_details[0]['index'], input_tensor)
    interpreter.invoke()
    output_data = interpreter.get_tensor(output_details[0]['index'])
    
    out_scale, out_zero_point = output_details[0]['quantization']
    output_probs = (output_data.astype(np.float32) - out_zero_point) * out_scale
    
    mapped = dict(zip(classes, output_probs[0]))
    for k, v in mapped.items():
        print(f"  {k}: {v:.4f}")

if __name__ == "__main__":
    main()
