import numpy as np
import tensorflow as tf

model_path = "main/vision/model_int8.tflite"
interp = tf.lite.Interpreter(model_path=model_path)
interp.allocate_tensors()
inp = interp.get_input_details()[0]
out = interp.get_output_details()[0]
scale, zp = inp["quantization"]
print(f"Input dtype={inp['dtype']} scale={scale} zp={zp}")
outs, outzp = out["quantization"]
print(f"Output dtype={out['dtype']} scale={outs} zp={outzp}")
classes = ["crow", "magpie", "squirrel", "background"]

def run(label, arr_uint8):
    arr = arr_uint8.astype(np.float32)
    q = np.clip((arr / scale) + zp, -128, 127).astype(np.int8)
    interp.set_tensor(inp["index"], q)
    interp.invoke()
    raw = interp.get_tensor(out["index"])
    probs = (raw.astype(np.float32) - outzp) * outs
    row = ", ".join(f"{c}={v:.4f}" for c, v in zip(classes, probs[0], strict=False))
    print(f"{label}: {row}")

run("BLACK", np.zeros((1, 96, 96, 3), dtype=np.uint8))
run("WHITE", np.full((1, 96, 96, 3), 255, dtype=np.uint8))
np.random.seed(0)
run("NOISE", np.random.randint(0, 256, (1, 96, 96, 3), dtype=np.uint8))
np.random.seed(1)
run("NOISE2", np.random.randint(0, 256, (1, 96, 96, 3), dtype=np.uint8))
run("GREY",  np.full((1, 96, 96, 3), 128, dtype=np.uint8))
