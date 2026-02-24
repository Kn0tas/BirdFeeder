import numpy as np
import tensorflow as tf

tflite_model_path = "main/vision/model_int8.tflite"
interpreter = tf.lite.Interpreter(model_path=tflite_model_path, experimental_delegates=[])
interpreter.allocate_tensors()

input_details = interpreter.get_input_details()[0]
output_details = interpreter.get_output_details()[0]

inp_scale, inp_zp = input_details["quantization"]
out_scale, out_zp = output_details["quantization"]

with open("out2.txt", "w", encoding="utf-8") as f:
    f.write(f"input scale={inp_scale} zp={inp_zp} output scale={out_scale} zp={out_zp}\n")

    def verify(label, uint8_arr):
        norm = uint8_arr.astype(np.float32) / 127.5 - 1.0
        q = np.clip(np.round(norm / inp_scale + inp_zp), -128, 127).astype(np.int8)
        interpreter.set_tensor(input_details["index"], q[np.newaxis])
        interpreter.invoke()
        raw = interpreter.get_tensor(output_details["index"])[0]
        probs = (raw.astype(np.float32) - out_zp) * out_scale
        row = "  ".join(f"{p:.3f}" for p in probs)
        f.write(f"{label:8s}: [{row}]  best={probs.argmax()} ({probs.max():.3f})\n")

    verify("BLACK",  np.zeros((96, 96, 3), dtype=np.uint8))
    verify("WHITE",  np.full((96, 96, 3), 255, dtype=np.uint8))
    verify("GREY",   np.full((96, 96, 3), 128, dtype=np.uint8))
    verify("NOISE",  np.random.randint(0, 256, (96, 96, 3), dtype=np.uint8))
