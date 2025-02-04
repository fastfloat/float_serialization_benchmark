#include "ieeeToString.h"

#include <cassert>
#include <cstring>

#include "ryu/digit_table.h"

IEEE754f decode_ieee754(float f) {
  const uint32_t& bits = reinterpret_cast<const uint32_t&>(f);

  IEEE754f decomposed;
  decomposed.exponent = (bits >> FloatMantissaBits) & ((1 << FloatExponentBits) - 1);
  decomposed.mantissa = bits & ((1 << FloatMantissaBits) - 1);
  decomposed.sign = bits >> 31;
  return decomposed;
}

IEEE754d decode_ieee754(double f) {
  const uint64_t& bits = reinterpret_cast<const uint64_t&>(f);

  IEEE754d decomposed;
  decomposed.exponent = (bits >> DoubleMantissaBits) & ((1ull << DoubleExponentBits) - 1);
  decomposed.mantissa = bits & ((1ull << DoubleMantissaBits) - 1);
  decomposed.sign = bits >> 63;
  return decomposed;
}

// Extracted from the Ryu implementation.
static inline uint32_t decimalLength17(const uint64_t v) {
  // Function precondition: v is not an 18, 19, or 20-digit number.
  // (17 digits are sufficient for round-tripping.)
  assert(v < 100000000000000000L);

  // Slightly faster than a loop.
  // Average output length is 16.38 digits, so we check high-to-low.
  if (v >= 10000000000000000L) { return 17; }
  if (v >= 1000000000000000L) { return 16; }
  if (v >= 100000000000000L) { return 15; }
  if (v >= 10000000000000L) { return 14; }
  if (v >= 1000000000000L) { return 13; }
  if (v >= 100000000000L) { return 12; }
  if (v >= 10000000000L) { return 11; }
  if (v >= 1000000000L) { return 10; }
  if (v >= 100000000L) { return 9; }
  if (v >= 10000000L) { return 8; }
  if (v >= 1000000L) { return 7; }
  if (v >= 100000L) { return 6; }
  if (v >= 10000L) { return 5; }
  if (v >= 1000L) { return 4; }
  if (v >= 100L) { return 3; }
  if (v >= 10L) { return 2; }
  return 1;
}

// Adapted from the Ryu implementation.
int to_chars(uint64_t mantissa, int32_t exponent, bool sign, char* const result) {
  int index = 0;
  if (sign)
    result[index++] = '-';

  uint64_t output = mantissa;
  const uint32_t olength = decimalLength17(mantissa);

  // Print the decimal digits.
  // for (uint32_t i = 0; i < olength - 1; ++i) {
  //   const uint32_t c = output % 10; output /= 10;
  //   result[index + olength - i] = (char) ('0' + c);
  // }
  // result[index] = '0' + output % 10;

  uint32_t i = 0;
  // We prefer 32-bit operations, even on 64-bit platforms.
  // We have at most 17 digits, and uint32_t can store 9 digits.
  // If output doesn't fit into uint32_t, we cut off 8 digits,
  // so the rest will fit into uint32_t.
  if ((output >> 32) != 0) {
    // Expensive 64-bit division.
    const uint64_t q = output / 100000000;
    uint32_t output2 = ((uint32_t) output) - 100000000 * ((uint32_t) q);
    output = q;

    const uint32_t c = output2 % 10000;
    output2 /= 10000;
    const uint32_t d = output2 % 10000;
    const uint32_t c0 = (c % 100) << 1;
    const uint32_t c1 = (c / 100) << 1;
    const uint32_t d0 = (d % 100) << 1;
    const uint32_t d1 = (d / 100) << 1;
    memcpy(result + index + olength - 1, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - 3, DIGIT_TABLE + c1, 2);
    memcpy(result + index + olength - 5, DIGIT_TABLE + d0, 2);
    memcpy(result + index + olength - 7, DIGIT_TABLE + d1, 2);
    i += 8;
  }
  uint32_t output2 = (uint32_t) output;
  while (output2 >= 10000) {
    const uint32_t c = output2 % 10000;
    output2 /= 10000;
    const uint32_t c0 = (c % 100) << 1;
    const uint32_t c1 = (c / 100) << 1;
    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - i - 3, DIGIT_TABLE + c1, 2);
    i += 4;
  }
  if (output2 >= 100) {
    const uint32_t c = (output2 % 100) << 1;
    output2 /= 100;
    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c, 2);
    i += 2;
  }
  if (output2 >= 10) {
    const uint32_t c = output2 << 1;
    // We can't use memcpy here: the decimal dot goes between these two digits.
    result[index + olength - i] = DIGIT_TABLE[c + 1];
    result[index] = DIGIT_TABLE[c];
  } else {
    result[index] = (char) ('0' + output2);
  }

  // Print decimal point if needed.
  if (olength > 1) {
    result[index + 1] = '.';
    index += olength + 1;
  } else {
    ++index;
  }

  // Print the exponent.
  result[index++] = 'E';
  int32_t exp = exponent + (int32_t) olength - 1;
  if (exp < 0) {
    result[index++] = '-';
    exp = -exp;
  }

  if (exp >= 100) {
    const int32_t c = exp % 10;
    memcpy(result + index, DIGIT_TABLE + 2 * (exp / 10), 2);
    result[index + 2] = (char) ('0' + c);
    index += 3;
  } else if (exp >= 10) {
    memcpy(result + index, DIGIT_TABLE + 2 * exp, 2);
    index += 2;
  } else {
    result[index++] = (char) ('0' + exp);
  }

  return index;
}
