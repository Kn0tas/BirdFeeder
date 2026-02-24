#include "unity.h"
#include "vision_utils.h"

static void test_clamp_int_within_range(void) {
  TEST_ASSERT_EQUAL_INT(50, clamp_int(50, 0, 100));
  TEST_ASSERT_EQUAL_INT(0, clamp_int(0, 0, 100));
  TEST_ASSERT_EQUAL_INT(100, clamp_int(100, 0, 100));
}

static void test_clamp_int_below(void) {
  TEST_ASSERT_EQUAL_INT(0, clamp_int(-10, 0, 100));
  TEST_ASSERT_EQUAL_INT(-128, clamp_int(-200, -128, 127));
}

static void test_clamp_int_above(void) {
  TEST_ASSERT_EQUAL_INT(100, clamp_int(200, 0, 100));
  TEST_ASSERT_EQUAL_INT(127, clamp_int(200, -128, 127));
}

static void test_clamp_int_int8_range(void) {
  /* This is the range used in vision.cpp quantization */
  TEST_ASSERT_EQUAL_INT(-128, clamp_int(-128, -128, 127));
  TEST_ASSERT_EQUAL_INT(127, clamp_int(127, -128, 127));
  TEST_ASSERT_EQUAL_INT(0, clamp_int(0, -128, 127));
  TEST_ASSERT_EQUAL_INT(-128, clamp_int(-999, -128, 127));
  TEST_ASSERT_EQUAL_INT(127, clamp_int(999, -128, 127));
}

void test_vision_utils(void) {
  RUN_TEST(test_clamp_int_within_range);
  RUN_TEST(test_clamp_int_below);
  RUN_TEST(test_clamp_int_above);
  RUN_TEST(test_clamp_int_int8_range);
}
