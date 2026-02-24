/**
 * @file vision_utils.h
 * @brief Pure utility functions for vision pipeline.
 *        Extracted for host-based unit testing (no ESP-IDF dependency).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

static inline int clamp_int(int value, int lo, int hi) {
  if (value < lo)
    return lo;
  if (value > hi)
    return hi;
  return value;
}

#ifdef __cplusplus
}
#endif
