#ifndef DECIMALTOSTRING_H
#define DECIMALTOSTRING_H

#include <cstdint>

int to_chars(uint64_t mantissa, int32_t exponent, bool sign, char* const result);

#endif
