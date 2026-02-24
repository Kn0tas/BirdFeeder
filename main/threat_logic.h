/**
 * @file threat_logic.h
 * @brief Pure logic for threat detection and vision result interpretation.
 *        Extracted for host-based unit testing (no ESP-IDF dependency).
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  THREAT_VISION_UNKNOWN = 0,
  THREAT_VISION_CROW,
  THREAT_VISION_MAGPIE,
  THREAT_VISION_SQUIRREL,
  THREAT_VISION_BACKGROUND,
} threat_vision_kind_t;

/**
 * @brief Determine whether a vision result constitutes a threat.
 * @param kind       The classification result.
 * @param confidence The confidence score (0.0 – 1.0).
 * @param threshold  The minimum confidence to consider a threat.
 * @return true if the result is a threat species above the threshold.
 */
static inline bool is_threat(threat_vision_kind_t kind, float confidence,
                             float threshold) {
  return (kind == THREAT_VISION_CROW || kind == THREAT_VISION_SQUIRREL ||
          kind == THREAT_VISION_MAGPIE) &&
         confidence >= threshold;
}

/**
 * @brief Convert a vision kind enum to a human-readable string.
 */
static inline const char *get_vision_kind_str(threat_vision_kind_t kind) {
  switch (kind) {
  case THREAT_VISION_CROW:
    return "CROW";
  case THREAT_VISION_SQUIRREL:
    return "SQUIRREL";
  case THREAT_VISION_MAGPIE:
    return "MAGPIE";
  case THREAT_VISION_BACKGROUND:
    return "BACKGROUND";
  case THREAT_VISION_UNKNOWN:
  default:
    return "UNKNOWN";
  }
}

#ifdef __cplusplus
}
#endif
