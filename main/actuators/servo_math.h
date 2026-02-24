/**
 * @file servo_math.h
 * @brief Pure math functions for servo PWM calculations.
 *        Extracted for host-based unit testing (no ESP-IDF dependency).
 */
#pragma once

#include <stdint.h>

#ifndef SERVO_PWM_FREQ_HZ
#define SERVO_PWM_FREQ_HZ 50
#endif
#ifndef SERVO_PWM_RES_BITS
#define SERVO_PWM_RES_BITS 14
#endif
#ifndef SERVO_PULSE_MIN_US
#define SERVO_PULSE_MIN_US 500
#endif
#ifndef SERVO_PULSE_MAX_US
#define SERVO_PULSE_MAX_US 2500
#endif

static inline uint32_t servo_pulse_to_duty(uint32_t pulse_us) {
  const uint32_t max_duty = (1U << SERVO_PWM_RES_BITS) - 1U;
  uint64_t duty = (uint64_t)pulse_us * SERVO_PWM_FREQ_HZ * max_duty;
  duty /= 1000000ULL;
  return (uint32_t)duty;
}

static inline uint32_t clamp_pulse(uint32_t pulse) {
  if (pulse < SERVO_PULSE_MIN_US) {
    return SERVO_PULSE_MIN_US;
  }
  if (pulse > SERVO_PULSE_MAX_US) {
    return SERVO_PULSE_MAX_US;
  }
  return pulse;
}
