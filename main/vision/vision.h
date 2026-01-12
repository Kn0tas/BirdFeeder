#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "sensors/camera.h"

typedef enum {
    VISION_RESULT_UNKNOWN = 0,
    VISION_RESULT_CROW,
    VISION_RESULT_SQUIRREL,
    VISION_RESULT_MAGPIE,
    VISION_RESULT_BIRD,
    VISION_RESULT_RAT,
    VISION_RESULT_OTHER,
} vision_kind_t;

typedef struct {
    vision_kind_t kind;
    float confidence;
} vision_result_t;

esp_err_t vision_init(void);
esp_err_t vision_classify(const camera_frame_t *frame, vision_result_t *result);

#ifdef __cplusplus
}
#endif
