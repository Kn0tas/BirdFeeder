#include "unity.h"
#include <stddef.h>
#include <stdint.h>


/* Reproduce the FRAM bounds-checking logic from fram.c locally,
   since the actual function depends on I2C handles. */
#define FRAM_CAP_BYTES 32768

static int fram_check_bounds(uint16_t addr, size_t len) {
  if (len == 0)
    return -1; /* invalid arg */
  if ((addr + len) > FRAM_CAP_BYTES)
    return -2; /* out of bounds */
  return 0;    /* ok */
}

static void test_valid_write_at_start(void) {
  TEST_ASSERT_EQUAL_INT(0, fram_check_bounds(0, 100));
}

static void test_valid_write_at_end(void) {
  TEST_ASSERT_EQUAL_INT(0, fram_check_bounds(FRAM_CAP_BYTES - 1, 1));
}

static void test_valid_write_full_capacity(void) {
  TEST_ASSERT_EQUAL_INT(0, fram_check_bounds(0, FRAM_CAP_BYTES));
}

static void test_overflow_past_end(void) {
  TEST_ASSERT_EQUAL_INT(-2, fram_check_bounds(FRAM_CAP_BYTES - 1, 2));
}

static void test_overflow_large_length(void) {
  TEST_ASSERT_EQUAL_INT(-2, fram_check_bounds(0, FRAM_CAP_BYTES + 1));
}

static void test_overflow_large_addr(void) {
  TEST_ASSERT_EQUAL_INT(-2, fram_check_bounds(FRAM_CAP_BYTES, 1));
}

static void test_zero_length_invalid(void) {
  TEST_ASSERT_EQUAL_INT(-1, fram_check_bounds(0, 0));
}

void test_fram_bounds(void) {
  RUN_TEST(test_valid_write_at_start);
  RUN_TEST(test_valid_write_at_end);
  RUN_TEST(test_valid_write_full_capacity);
  RUN_TEST(test_overflow_past_end);
  RUN_TEST(test_overflow_large_length);
  RUN_TEST(test_overflow_large_addr);
  RUN_TEST(test_zero_length_invalid);
}
