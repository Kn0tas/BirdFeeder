"""
List all TFLite ops used in model_int8.tflite.
Run from repo root: python tools/list_ops.py
"""
import tensorflow as tf

model_path = "main/vision/model_int8.tflite"

# Use the flatbuffer schema to enumerate ops
with open(model_path, "rb") as f:
    buf = f.read()

# TFLite flatbuffer format: root table is Model, contains operatorCodes and subgraphs
# Use schema_fb for parsing
try:
    from tensorflow.lite.python import schema_py_generated as schema_fb
    model = schema_fb.ModelT.InitFromPackedBuf(buf, 0)

    # Collect op code indices used across all subgraphs
    used_indices = set()
    for sg in model.subgraphs:
        for op in sg.operators:
            used_indices.add(op.opcodeIndex)

    print("=== Ops used in model ===")
    for idx in sorted(used_indices):
        oc = model.operatorCodes[idx]
        # builtinCode is an integer corresponding to BuiltinOperator enum
        code = oc.builtinCode
        # Map to name
        try:
            from tensorflow.lite.python.schema_py_generated import BuiltinOperator
            name = {v: k for k, v in vars(BuiltinOperator).items() if not k.startswith("_")}.get(code, f"UNKNOWN_{code}")
        except Exception:
            name = f"code_{code}"
        print(f"  [{idx}] builtinCode={code}  name={name}")

except Exception as e:
    print(f"schema_fb approach failed: {e}")
    # Fallback: use Analyzer
    try:
        tf.lite.experimental.Analyzer.analyze(model_path=model_path)
    except Exception as e2:
        print(f"Analyzer failed: {e2}")
