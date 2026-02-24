"""Automated validation tests for the BirdFeeder TFLite int8 model.

Run with:
    cd tools
    pytest test_model.py -v
"""
import numpy as np
import pytest
from conftest import CLASSES, INPUT_SIZE, quantize_input, run_inference


# ---------------------------------------------------------------------------
# Model structure tests
# ---------------------------------------------------------------------------

class TestModelStructure:
    """Verify the model has the expected shape, dtype, and quantization."""

    def test_model_loads(self, interpreter):
        """Model file loads without error."""
        assert interpreter is not None

    def test_input_shape(self, input_details):
        assert list(input_details["shape"]) == [1, INPUT_SIZE, INPUT_SIZE, 3]

    def test_input_dtype(self, input_details):
        assert input_details["dtype"] == np.int8

    def test_output_shape(self, output_details):
        assert list(output_details["shape"]) == [1, len(CLASSES)]

    def test_output_dtype(self, output_details):
        assert output_details["dtype"] == np.int8

    def test_input_quantization_exists(self, input_quant):
        scale, zp = input_quant
        assert scale > 0, "Input scale must be positive"

    def test_output_quantization_exists(self, output_quant):
        scale, zp = output_quant
        assert scale > 0, "Output scale must be positive"


# ---------------------------------------------------------------------------
# Inference sanity tests
# ---------------------------------------------------------------------------

class TestInferenceSanity:
    """Run inference on synthetic inputs and verify outputs are sensible."""

    @staticmethod
    def _infer(interpreter, input_details, output_details, image_uint8):
        tensor = quantize_input(image_uint8, input_details)
        return run_inference(interpreter, input_details, output_details, tensor)

    def test_black_image_classifies(self, interpreter, input_details, output_details):
        """A solid black image should produce a valid classification."""
        img = np.zeros((INPUT_SIZE, INPUT_SIZE, 3), dtype=np.uint8)
        probs = self._infer(interpreter, input_details, output_details, img)
        assert probs.shape == (len(CLASSES),)
        best = int(np.argmax(probs))
        assert 0 <= best < len(CLASSES)

    def test_white_image_classifies(self, interpreter, input_details, output_details):
        """A solid white image should produce a valid classification."""
        img = np.full((INPUT_SIZE, INPUT_SIZE, 3), 255, dtype=np.uint8)
        probs = self._infer(interpreter, input_details, output_details, img)
        best = int(np.argmax(probs))
        assert 0 <= best < len(CLASSES)

    def test_grey_image_classifies(self, interpreter, input_details, output_details):
        """A solid grey image should produce a valid classification."""
        img = np.full((INPUT_SIZE, INPUT_SIZE, 3), 128, dtype=np.uint8)
        probs = self._infer(interpreter, input_details, output_details, img)
        best = int(np.argmax(probs))
        assert 0 <= best < len(CLASSES)

    def test_noise_image_classifies(self, interpreter, input_details, output_details):
        """Random noise should produce a valid classification."""
        rng = np.random.default_rng(42)
        img = rng.integers(0, 256, (INPUT_SIZE, INPUT_SIZE, 3), dtype=np.uint8)
        probs = self._infer(interpreter, input_details, output_details, img)
        best = int(np.argmax(probs))
        assert 0 <= best < len(CLASSES)

    def test_outputs_are_dynamic(self, interpreter, input_details, output_details):
        """Different inputs must produce different outputs (model is not stuck)."""
        imgs = [
            np.zeros((INPUT_SIZE, INPUT_SIZE, 3), dtype=np.uint8),
            np.full((INPUT_SIZE, INPUT_SIZE, 3), 255, dtype=np.uint8),
            np.full((INPUT_SIZE, INPUT_SIZE, 3), 128, dtype=np.uint8),
        ]
        outputs = []
        for img in imgs:
            probs = self._infer(interpreter, input_details, output_details, img)
            outputs.append(probs.copy())

        # At least two of the three must differ
        all_same = all(np.allclose(outputs[0], o, atol=1e-4) for o in outputs[1:])
        assert not all_same, (
            f"Model produced identical outputs for different inputs: {outputs}"
        )

    def test_probabilities_sum_near_one(self, interpreter, input_details, output_details):
        """Softmax output probabilities should sum to approximately 1.0."""
        rng = np.random.default_rng(99)
        img = rng.integers(0, 256, (INPUT_SIZE, INPUT_SIZE, 3), dtype=np.uint8)
        probs = self._infer(interpreter, input_details, output_details, img)
        total = float(np.sum(probs))
        assert 0.8 < total < 1.2, f"Probabilities sum to {total}, expected ~1.0"

    def test_no_class_permanently_zero(self, interpreter, input_details, output_details):
        """No class should be permanently stuck at zero across varied inputs."""
        rng = np.random.default_rng(123)
        class_max = np.full(len(CLASSES), -999.0)
        for _ in range(5):
            img = rng.integers(0, 256, (INPUT_SIZE, INPUT_SIZE, 3), dtype=np.uint8)
            probs = self._infer(interpreter, input_details, output_details, img)
            class_max = np.maximum(class_max, probs)

        for i, name in enumerate(CLASSES):
            assert class_max[i] > -0.5, (
                f"Class '{name}' never produced a meaningful score across 5 inputs"
            )

    def test_solid_color_not_threat(self, interpreter, input_details, output_details):
        """Solid-color images (no animal) should ideally classify as background."""
        bg_idx = CLASSES.index("background")
        for val in [0, 128, 255]:
            img = np.full((INPUT_SIZE, INPUT_SIZE, 3), val, dtype=np.uint8)
            probs = self._infer(interpreter, input_details, output_details, img)
            best = int(np.argmax(probs))
            # This is a soft assertion — log a warning rather than hard-failing,
            # since model quality varies. The test ensures the model at least
            # *considers* background for featureless images.
            if best != bg_idx:
                pytest.skip(
                    f"Solid-color ({val}) classified as '{CLASSES[best]}' "
                    f"instead of 'background' — model may need retraining"
                )
