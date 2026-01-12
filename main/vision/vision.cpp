#include "vision.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "img_converters.h"

#include "vision/model_data.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static const char *TAG = "vision";

namespace {
constexpr size_t kTensorArenaSize = 2 * 1024 * 1024; // 2MB for PSRAM

const tflite::Model *s_model = nullptr;
uint8_t *s_tensor_arena = nullptr;
tflite::MicroInterpreter *s_interpreter = nullptr;
TfLiteTensor *s_input = nullptr;
TfLiteTensor *s_output = nullptr;
int s_input_w = 0;
int s_input_h = 0;
int s_input_c = 0;

const vision_kind_t kLabelToKind[] = {
    VISION_RESULT_CROW,    VISION_RESULT_SQUIRREL, VISION_RESULT_RAT,
    VISION_RESULT_MAGPIE,  VISION_RESULT_BIRD,     VISION_RESULT_OTHER,
    VISION_RESULT_UNKNOWN,
};

int tensor_element_count(const TfLiteTensor *tensor) {
  if (!tensor || !tensor->dims) {
    return 0;
  }
  int count = 1;
  for (int i = 0; i < tensor->dims->size; ++i) {
    count *= tensor->dims->data[i];
  }
  return count;
}

int clamp_int(int value, int lo, int hi) {
  if (value < lo)
    return lo;
  if (value > hi)
    return hi;
  return value;
}
} // namespace

esp_err_t vision_init(void) {
  vision_model_data_init();
  if (g_model_tflite_len == 0) {
    ESP_LOGW(TAG, "no model embedded; vision will always return UNKNOWN");
    return ESP_OK;
  }

  s_model = tflite::GetModel(g_model_tflite);
  if (s_model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "model schema %d != supported %d", s_model->version(),
             TFLITE_SCHEMA_VERSION);
    return ESP_ERR_INVALID_VERSION;
  }

  s_tensor_arena = static_cast<uint8_t *>(
      heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!s_tensor_arena) {
    s_tensor_arena = static_cast<uint8_t *>(
        heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_8BIT));
  }
  if (!s_tensor_arena) {
    ESP_LOGE(TAG, "tensor arena alloc failed (%u bytes)",
             (unsigned)kTensorArenaSize);
    return ESP_ERR_NO_MEM;
  }

  static tflite::MicroMutableOpResolver<16> resolver;
  resolver.AddAdd();
  resolver.AddConv2D();
  resolver.AddDepthwiseConv2D();
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddAveragePool2D();
  resolver.AddMaxPool2D();
  resolver.AddReshape();
  resolver.AddMean();
  resolver.AddPad();
  resolver.AddMul();
  resolver.AddQuantize();
  resolver.AddDequantize();
  s_interpreter = new tflite::MicroInterpreter(
      s_model, resolver, s_tensor_arena, kTensorArenaSize);
  if (!s_interpreter) {
    ESP_LOGE(TAG, "failed to create TFLite Micro interpreter");
    return ESP_FAIL;
  }
  if (s_interpreter->AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(TAG, "AllocateTensors failed");
    return ESP_FAIL;
  }

  s_input = s_interpreter->input(0);
  s_output = s_interpreter->output(0);
  if (!s_input || !s_output) {
    ESP_LOGE(TAG, "missing input/output tensors");
    return ESP_FAIL;
  }
  if (s_input->type != kTfLiteInt8) {
    ESP_LOGE(TAG, "expected int8 input, got %d", s_input->type);
    return ESP_ERR_INVALID_STATE;
  }

  if (s_input->dims->size != 4) {
    ESP_LOGE(TAG, "unexpected input dims (%d)", s_input->dims->size);
    return ESP_ERR_INVALID_STATE;
  }
  s_input_h = s_input->dims->data[1];
  s_input_w = s_input->dims->data[2];
  s_input_c = s_input->dims->data[3];
  if (s_input_c != 3) {
    ESP_LOGE(TAG, "expected 3-channel input, got %d", s_input_c);
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "vision model loaded (%u bytes, input %dx%dx%d)",
           (unsigned)g_model_tflite_len, s_input_w, s_input_h, s_input_c);
  return ESP_OK;
}

esp_err_t vision_classify(const camera_frame_t *frame,
                          vision_result_t *result) {
  if (!frame || !result) {
    return ESP_ERR_INVALID_ARG;
  }
  if (g_model_tflite_len == 0 || !s_interpreter || !s_input || !s_output) {
    result->kind = VISION_RESULT_UNKNOWN;
    result->confidence = 0.0f;
    return ESP_OK;
  }
  if (!frame->is_jpeg) {
    ESP_LOGW(TAG, "frame not JPEG; cannot classify");
    return ESP_ERR_INVALID_STATE;
  }

  const int src_w = frame->width;
  const int src_h = frame->height;
  const size_t rgb_size = (size_t)src_w * (size_t)src_h * 3;
  uint8_t *rgb = static_cast<uint8_t *>(malloc(rgb_size));
  if (!rgb) {
    ESP_LOGE(TAG, "RGB buffer alloc failed (%u bytes)", (unsigned)rgb_size);
    return ESP_ERR_NO_MEM;
  }

  if (!fmt2rgb888(frame->data, frame->size, PIXFORMAT_JPEG, rgb)) {
    ESP_LOGE(TAG, "JPEG to RGB888 conversion failed");
    free(rgb);
    return ESP_FAIL;
  }

  const int crop = (src_w < src_h) ? src_w : src_h;
  const int crop_x = (src_w - crop) / 2;
  const int crop_y = (src_h - crop) / 2;

  const float scale = s_input->params.scale;
  const int zero_point = s_input->params.zero_point;
  int8_t *input = s_input->data.int8;
  if (!input) {
    ESP_LOGE(TAG, "input tensor buffer missing");
    free(rgb);
    return ESP_FAIL;
  }

  for (int y = 0; y < s_input_h; ++y) {
    int src_y = crop_y + (y * crop) / s_input_h;
    for (int x = 0; x < s_input_w; ++x) {
      int src_x = crop_x + (x * crop) / s_input_w;
      size_t src_idx = ((size_t)src_y * (size_t)src_w + (size_t)src_x) * 3;
      for (int c = 0; c < 3; ++c) {
        float real = rgb[src_idx + c] / 255.0f;
        int32_t q = (int32_t)lrintf(real / scale + (float)zero_point);
        q = clamp_int(q, -128, 127);
        *input++ = (int8_t)q;
      }
    }
  }

  free(rgb);

  if (s_interpreter->Invoke() != kTfLiteOk) {
    ESP_LOGE(TAG, "Invoke failed");
    return ESP_FAIL;
  }

  const int out_count = tensor_element_count(s_output);
  if (out_count <= 0) {
    ESP_LOGE(TAG, "invalid output tensor shape");
    return ESP_FAIL;
  }

  float best_score = -1.0f;
  int best_idx = 0;
  const float out_scale = s_output->params.scale;
  const int out_zero = s_output->params.zero_point;

  if (s_output->type == kTfLiteInt8) {
    const int8_t *out = s_output->data.int8;
    for (int i = 0; i < out_count; ++i) {
      float score = (float)(out[i] - out_zero) * out_scale;
      if (score > best_score) {
        best_score = score;
        best_idx = i;
      }
    }
  } else if (s_output->type == kTfLiteUInt8) {
    const uint8_t *out = s_output->data.uint8;
    for (int i = 0; i < out_count; ++i) {
      float score = (float)((int)out[i] - out_zero) * out_scale;
      if (score > best_score) {
        best_score = score;
        best_idx = i;
      }
    }
  } else {
    ESP_LOGE(TAG, "unexpected output tensor type %d", s_output->type);
    return ESP_FAIL;
  }

  const int label_count = (int)(sizeof(kLabelToKind) / sizeof(kLabelToKind[0]));
  if (best_idx < 0 || best_idx >= label_count) {
    result->kind = VISION_RESULT_UNKNOWN;
  } else {
    result->kind = kLabelToKind[best_idx];
  }
  result->confidence = best_score;
  return ESP_OK;
}
