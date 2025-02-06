#ifndef IEEETOSTRING_H
#define IEEETOSTRING_H

#include <cstdint>

constexpr uint8_t FloatMantissaBits = 23;
constexpr uint8_t FloatExponentBits = 8;

constexpr uint8_t DoubleMantissaBits = 52;
constexpr uint8_t DoubleExponentBits = 11;

// Step 1: extract the sign, mantissa and exponent from an IEEE 754 number
struct IEEE754f {
  uint32_t mantissa;
  uint32_t exponent;
  bool sign;
};

struct IEEE754d {
  uint64_t mantissa;
  uint32_t exponent;
  bool sign;
};

IEEE754f decode_ieee754(float f);
IEEE754d decode_ieee754(double f);

// Step 3: convert a decimal exponent and mantissa to a string representation
template <typename T>
int to_chars(T mantissa, int32_t exponent, bool sign, char* const result);
extern template int to_chars<uint32_t>(uint32_t mantissa, int32_t exponent, bool sign, char* const result);
extern template int to_chars<uint64_t>(uint64_t mantissa, int32_t exponent, bool sign, char* const result);

#endif
