"""Shared pytest fixtures for BirdFeeder model validation tests."""
import os

import numpy as np
import pytest
import tensorflow as tf

MODEL_PATH = os.path.join(
    os.path.dirname(__file__), "..", "main", "vision", "model_int8.tflite"
)
CLASSES = ["crow", "magpie", "squirrel", "background"]
INPUT_SIZE = 96


@pytest.fixture(scope="session")
def interpreter():
    """Load the TFLite model once for all tests."""
    interp = tf.lite.Interpreter(model_path=MODEL_PATH, experimental_delegates=[])
    interp.allocate_tensors()
    return interp


@pytest.fixture(scope="session")
def input_details(interpreter):
    return interpreter.get_input_details()[0]


@pytest.fixture(scope="session")
def output_details(interpreter):
    return interpreter.get_output_details()[0]


@pytest.fixture(scope="session")
def input_quant(input_details):
    """Return (scale, zero_point) for input tensor."""
    return input_details["quantization"]


@pytest.fixture(scope="session")
def output_quant(output_details):
    """Return (scale, zero_point) for output tensor."""
    return output_details["quantization"]


def quantize_input(image_uint8: np.ndarray, input_details: dict) -> np.ndarray:
    """Quantize a [H, W, 3] uint8 image to the model's expected int8 input.

    Matches the firmware preprocessing in vision.cpp:
        norm = pixel / 127.5 - 1.0
        q    = round(norm / scale + zero_point)
    """
    scale, zp = input_details["quantization"]
    norm = image_uint8.astype(np.float32) / 127.5 - 1.0
    q = np.clip(np.round(norm / scale + zp), -128, 127).astype(np.int8)
    return q[np.newaxis]  # add batch dim → [1, H, W, 3]


def run_inference(interpreter, input_details, output_details, input_tensor):
    """Run inference and return dequantized float32 output probabilities."""
    interpreter.set_tensor(input_details["index"], input_tensor)
    interpreter.invoke()
    raw = interpreter.get_tensor(output_details["index"])[0]
    out_scale, out_zp = output_details["quantization"]
    return (raw.astype(np.float32) - out_zp) * out_scale
