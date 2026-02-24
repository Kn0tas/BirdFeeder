/**
 * @file max17048_math.h
 * @brief Pure math for MAX17048 register-to-value conversions.
 *        Extracted for host-based unit testing (no ESP-IDF dependency).
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert raw VCELL register bytes to voltage in volts.
 * @param msb High byte of VCELL register.
 * @param lsb Low byte of VCELL register.
 * @return Voltage in volts.
 */
static inline float max17048_raw_to_voltage(uint8_t msb, uint8_t lsb) {
  uint16_t vraw = ((uint16_t)msb << 8) | lsb;
  vraw >>= 4;
  return (float)vraw * 1.25f / 1000.0f;
}

/**
 * @brief Convert raw SOC register bytes to state-of-charge percentage.
 * @param msb High byte (integer part of SOC).
 * @param lsb Low byte (fractional part, 1/256 resolution).
 * @return SOC in percent (e.g. 75.5 means 75.5%).
 */
static inline float max17048_raw_to_soc(uint8_t msb, uint8_t lsb) {
  return (float)msb + ((float)lsb / 256.0f);
}

#ifdef __cplusplus
}
#endif
