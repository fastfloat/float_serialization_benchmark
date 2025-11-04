#ifndef CHAMPAGNE_LEMIRE_FAST_H
#define CHAMPAGNE_LEMIRE_FAST_H

#ifdef _MSC_VER
#define CHAMPAGNE_LEMIRE_VISUAL_STUDIO 1
#ifdef __clang__
// clang under visual studio
#define CHAMPAGNE_LEMIRE_CLANG_VISUAL_STUDIO 1
#else
// just regular visual studio (best guess)
#define CHAMPAGNE_LEMIRE_REGULAR_VISUAL_STUDIO 1
#endif // __clang__
#endif // _MSC_VER

#if defined(CHAMPAGNE_LEMIRE_VISUAL_STUDIO)
#define champagne_lemire_really_inline                                         \
  __forceinline // really inline in release mode

#else // CHAMPAGNE_LEMIRE_REGULAR_VISUAL_STUDIO
#if defined(__OPTIMIZE__) || defined(NDEBUG)
#define champagne_lemire_really_inline inline __attribute__((always_inline))
#else
#define champagne_lemire_really_inline inline
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define champagne_lemire_unreachable() __builtin_unreachable()
#elif defined(_MSC_VER)
#define champagne_lemire_unreachable() __assume(false)
#else
// Fallback: A [[noreturn]] function that terminates the program
[[noreturn]] inline void champagne_lemire_unreachable_fail() {
    // In debug/test builds, you might want to log/assert before aborting.
    // This is intentionally minimal for a release-mode like 'unreachable'.
    std::abort();
}
#define champagne_lemire_unreachable() champagne_lemire_unreachable_fail()
#endif

#include <array>
#include <bit>
#include <cstring>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace champagne_lemire {
namespace multiplier {
champagne_lemire_really_inline std::pair<uint64_t, uint64_t>
mul64x64_to_128(uint64_t a, uint64_t b) {
#if defined(_M_ARM64) && !defined(__MINGW32__)
  // ARM64 has native support for 64-bit multiplications, no need to emulate
  // But MinGW on ARM64 doesn't have native support for 64-bit multiplications
  return {__umulh(a, b), a * b};
#elif defined(_WIN64) && !defined(__clang__)
  // MSVC: Use _umul128 intrinsic
  uint64_t high;
  uint64_t low = _umul128(a, b, &high);
  return {high, low};
#elif defined(__SIZEOF_INT128__)
  // GCC/Clang: Use __uint128_t
  __uint128_t result =
      static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
  return {static_cast<uint64_t>(result >> 64), static_cast<uint64_t>(result)};
#else
  auto emulu = [](uint32_t x, uint32_t y) -> uint64_t {
    return x * (uint64_t)y;
  };

  // Split 64-bit numbers into 32-bit halves
  uint32_t a_lo = (uint32_t)a;
  uint32_t a_hi = (uint32_t)(a >> 32);
  uint32_t b_lo = (uint32_t)b;
  uint32_t b_hi = (uint32_t)(b >> 32);

  // Perform partial multiplications
  uint64_t ll = emulu(a_lo, b_lo);
  uint64_t lh = emulu(a_lo, b_hi);
  uint64_t hl = emulu(a_hi, b_lo);
  uint64_t hh = emulu(a_hi, b_hi);

  // Combine partial products with carry handling
  uint64_t cross = lh + hl;
  uint64_t cross_carry = (cross < lh);
  uint64_t low = ll + (cross << 32);
  uint64_t low_carry = (low < ll);
  uint64_t high = hh + (cross >> 32) + cross_carry + low_carry;
  return {high, low};
#endif
}
} // namespace multiplier
champagne_lemire_really_inline int int_log2_64(uint64_t x) {
  return 63 - std::countl_zero(x | 1);
}

/**
 * Reference:
 * Daniel Lemire, "Computing the number of digits of an integer even faster,"
 * in Daniel Lemire's blog, June 3, 2021,
 * https://lemire.me/blog/2021/06/03/computing-the-number-of-digits-of-an-integer-even-faster/.
 */
champagne_lemire_really_inline int fast_digit_count32(uint32_t x) {
  static uint64_t table[] = {
      4294967296,  8589934582,  8589934582,  8589934582,  12884901788,
      12884901788, 12884901788, 17179868184, 17179868184, 17179868184,
      21474826480, 21474826480, 21474826480, 21474826480, 25769703776,
      25769703776, 25769703776, 30063771072, 30063771072, 30063771072,
      34349738368, 34349738368, 34349738368, 34349738368, 38554705664,
      38554705664, 38554705664, 41949672960, 41949672960, 41949672960,
      42949672960, 42949672960};
  return uint32_t((x + table[int_log2_64(x)]) >> 32);
}

/**
 * Reference:
 * Daniel Lemire, "Counting the digits of 64-bit integers,"
 * in Daniel Lemire's blog, January 7, 2025,
 * https://lemire.me/blog/2025/01/07/counting-the-digits-of-64-bit-integers/.
 */
champagne_lemire_really_inline int fast_digit_count64(uint64_t x) {
  static uint64_t table[] = {9,
                             99,
                             999,
                             9999,
                             99999,
                             999999,
                             9999999,
                             99999999,
                             999999999,
                             9999999999,
                             99999999999,
                             999999999999,
                             9999999999999,
                             99999999999999,
                             999999999999999ULL,
                             9999999999999999ULL,
                             99999999999999999ULL,
                             999999999999999999ULL,
                             9999999999999999999ULL};
  int y = (19 * int_log2_64(x) >> 6);
  y += x > table[y];
  return y + 1;
}

template <typename T> champagne_lemire_really_inline int fast_digit_count(T x) {
  if constexpr (sizeof(T) == 4) {
    return fast_digit_count32(x);
  } else if constexpr (sizeof(T) == 8) {
    return fast_digit_count64(x);
  } else {
    static_assert(sizeof(T) == 4 || sizeof(T) == 8, "Unsupported type size");
  }
}


namespace digits {
// This will divide by 100 using multiplication and shifts
// and leave a remainder that is not x%100 but that
// is still unique to x mod 100
// In this manner, we go from a number x in [0, 10000) to
// a pair (div, mod) using a single multiplication.
//
// So div100v returns (x / 100, x % 100) but the second
// value is not exactly x % 100, it is a value unique to x mod 100
constexpr champagne_lemire_really_inline std::pair<uint64_t, uint64_t> div100v(uint64_t x) {
  const uint64_t v = x * uint64_t(0x28f5c29); // ceil(2^32 / 100)
  return {v >> 32, (v >> 24) & 0xff};
}

champagne_lemire_really_inline std::pair<uint64_t, uint64_t> div100(uint64_t x) {
  const uint64_t q = multiplier::mul64x64_to_128(x, 0x28f5c28f5c28f5d).first; // ceil(2^64 / 100)
  return {q, x - 100 * q};
}

enum {
  maybe_larger_than_1e15 = true,
  definitely_less_than_1e15 = false
};

template <bool maybe_larger_than_1e15>
champagne_lemire_really_inline std::pair<uint64_t, uint64_t> div10000(uint64_t x) {
  if constexpr (maybe_larger_than_1e15) {
    const uint64_t q = multiplier::mul64x64_to_128(x, 0x346dc5d63886594bull).first >> 11;
    const uint64_t r = x - 10000 * q;
    return {q, r};
  } else {
    const uint64_t q = multiplier::mul64x64_to_128(x, 0x68db8bac710cc).first; // ceil(2^64 / 10000)
    const uint64_t r = x - 10000 * q;
    return {q, r};
  }
}

// This is the constant g++15 generates (Granlund-Montgomery ?)
champagne_lemire_really_inline std::pair<uint64_t, uint64_t> div10e16(uint64_t x) {
  const uint64_t q = multiplier::mul64x64_to_128(x, 0x39A5652FB1137857ull).first >> 51;
  const uint64_t r = x - 10'000'000'000'000'000ull * q;
  return {q, r};
}

champagne_lemire_really_inline std::array<char, 2> get_one_digit_with_dot(uint32_t value) {
  constexpr static std::array<std::array<char, 2>, 10> digit_table = []() {
    std::array<std::array<char, 2>, 10> table;
    for (int i = 0; i < 10; ++i) {
      // Calculate the tens digit
      table[i][0] = i + '0';
      table[i][1] = '.';
    }
    return table;
  }();
  return digit_table[value];
}

champagne_lemire_really_inline std::array<char, 3> get_two_digits_with_dot(uint32_t value) {
  constexpr static std::array<std::array<char, 3>, 100> hundreds_digit_table =
    []() {
      std::array<std::array<char, 3>, 100> table{};
      for (int i = 0; i < 100; ++i) {
        table[i] = {static_cast<char>((i / 10) % 10 + '0'), '.',
                    static_cast<char>((i % 10) + '0')};
      }
      return table;
    }();
  return hundreds_digit_table[value];
}



champagne_lemire_really_inline std::array<char, 4> get_two_digits_with_dot_with_one_pad(uint32_t value) {
  constexpr static std::array<std::array<char, 4>, 100> hundreds_digit_table =
    []() {
      std::array<std::array<char, 4>, 100> table{};
      for (int i = 0; i < 100; ++i) {
        table[i] = {static_cast<char>((i / 10) % 10 + '0'), '.',
                    static_cast<char>((i % 10) + '0'), '0'}; // extra pad
      }
      return table;
    }();
  return hundreds_digit_table[value];
}

champagne_lemire_really_inline std::array<char, 2> get_two_digits_v(uint32_t value) {
  constexpr static std::array<std::array<char, 2>, 256> hundreds_digit_table =
    []() {
      std::array<std::array<char, 2>, 256> table{};
      for (int i = 0; i < 10000; ++i) {
        table[div100v(i).second] = {static_cast<char>((i / 10) % 10 + '0'),
                                    static_cast<char>((i % 10) + '0')};
      }
      return table;
    }();
  return hundreds_digit_table[value];
}

champagne_lemire_really_inline std::array<char, 2> get_two_digits(uint32_t value) {
  constexpr static std::array<std::array<char, 2>, 100> hundreds_digit_table =
    []() {
      std::array<std::array<char, 2>, 100> table;
      for (int i = 0; i < 100; ++i) {
        // Calculate the tens digit
        table[i][0] = (i / 10) + '0';
        // Calculate the units digit
        table[i][1] = (i % 10) + '0';
      }
      return table;
    }();
  return hundreds_digit_table[value];
}

// We stop at 309 because that is the upper bound for the exponent in a double
champagne_lemire_really_inline std::array<char, 3> get_three_digits(uint32_t value) {
  constexpr static std::array<std::array<char, 3>, 309> digit_table =
    []() {
      std::array<std::array<char, 3>, 309> table;
      for (int i = 0; i < 309; ++i) {
        table[i][0] = (i / 100) + '0';
        // Calculate the tens digit
        table[i][1] = ((i / 10) % 10) + '0';
        // Calculate the units digit
        table[i][2] = (i % 10) + '0';
      }
      return table;
    }();
  return digit_table[value];
}

champagne_lemire_really_inline void write_one_digit_with_dot(char *buffer, uint32_t value) {
  std::memcpy(buffer, get_one_digit_with_dot(value).data(), 2);
}

champagne_lemire_really_inline void write_two_digits_with_dot(char *buffer, uint32_t value) {
  std::memcpy(buffer, get_two_digits_with_dot(value).data(), 3);
}

// writes two digits, a dot, and a padding zero, why the padding zero? because
// it is faster to copy four bytes than three bytes on most architectures.
champagne_lemire_really_inline void write_two_digits_with_dot_with_one_pad(char *buffer, uint32_t value) {
  std::memcpy(buffer, get_two_digits_with_dot_with_one_pad(value).data(), 4);
}

champagne_lemire_really_inline void write_two_digits(char *buffer, uint32_t value) {
  std::memcpy(buffer, get_two_digits(value).data(), 2);
}

champagne_lemire_really_inline void write_two_digits_v(char *buffer, uint32_t value) {
  std::memcpy(buffer, get_two_digits_v(value).data(), 2);
}

champagne_lemire_really_inline void write_three_digits(char *buffer, uint32_t value) {
  std::memcpy(buffer, get_three_digits(value).data(), 3);
}

champagne_lemire_really_inline void write_four_digits_10000(char *buffer, uint64_t value) {
  auto [high, low] = div100v(value);
  std::memcpy(buffer, get_two_digits(high).data(), 2);
  std::memcpy(buffer + 2, get_two_digits_v(low).data(), 2);
}

champagne_lemire_really_inline char* write_three_or_four_digits_10000(char *buffer, uint64_t value) {
  auto [high, low] = div100v(value);
  if (value < 1000) {
    buffer[0] = char('0' + high);
    std::memcpy(buffer + 1, get_two_digits_v(low).data(), 2);
    return buffer + 3;
  } else {
    std::memcpy(buffer,     get_two_digits(high).data(), 2);
    std::memcpy(buffer + 2, get_two_digits_v(low).data(), 2);
    return buffer + 4;
  }
}

champagne_lemire_really_inline char* write_one_two_three_or_four_digits_10000(char *buffer, uint64_t value) {
  if(value >= 1000) { // four digits
    const auto [high, low] = div100v(value);
    std::memcpy(buffer,     get_two_digits(high).data(), 2);
    std::memcpy(buffer + 2, get_two_digits_v(low).data(), 2);
    return buffer + 4;
  } else if(value >= 100) { // three digits
    // This could be further optimized:
    const auto [high, low] = div100v(value);
    buffer[0] = char('0' + high);
    std::memcpy(buffer + 1, get_two_digits_v(low).data(), 2);
    return buffer + 3;
  } else if(value >= 10) {
    std::memcpy(buffer, get_two_digits(value).data(), 2);
    return buffer + 2;
  } else {
    buffer[0] = char('0' + value);
    return buffer + 1;
  }
}
} // namespace digits


template <typename T>
champagne_lemire_really_inline int to_chars(T mantissa, int32_t exponent,
                                            char *const result) {
  //////////
  // LLVM/clang seems to like this function better than GCC.
  ////////
  //////////
  // For the math behind the multipliers, see:
  // scripts/optimal_bound.py
  ////////
  constexpr bool is_double = sizeof(T) == 8;
  static_assert(std::is_same_v<T, uint64_t> || std::is_same_v<T, uint32_t>,
                "Only integers are supported");
  static_assert(sizeof(T) == 8 || sizeof(T) == 4, "Unsupported type size");
  int32_t exp = exponent;
  const uint32_t number_of_digits =
      is_double ? fast_digit_count64(mantissa) : fast_digit_count32(mantissa);
  size_t exp_index;
  if (mantissa >= 100'00'00'00'00'00'00'00) {
    // The mantissa is in [10^16, 10^17)
    // Important: GCC gets really confused with the registers if you don't write
    // the results to memory right away. Hence, I interleave the multiplications
    // and the writes.
    //
    // We divide by 10^8, the binary remainder is in low10_8
    auto [xdiv10_8, low10_8] =
        multiplier::mul64x64_to_128(mantissa, 48357032784585167ULL);
    low10_8 = (low10_8 >> 18) | (xdiv10_8 << (64 - 18));
    xdiv10_8 >>= 18;
    auto [digits12, low10_8_1] = multiplier::mul64x64_to_128(low10_8, 100);
    digits::write_two_digits(result + 10, digits12);
    auto [digits23, low10_8_2] = multiplier::mul64x64_to_128(low10_8_1, 100);
    digits::write_two_digits(result + 12, digits23);
    auto [digits45, low10_8_3] = multiplier::mul64x64_to_128(low10_8_2, 100);
    digits::write_two_digits(result + 14, digits45);
    auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_8_3, 100);
    digits::write_two_digits(result + 16, digits67);
    // We divide by 10^8 again to get the first digits
    auto [xdiv10_16, low10_16] =
        multiplier::mul64x64_to_128(xdiv10_8, 184467440738ULL);
    digits::write_one_digit_with_dot(result, xdiv10_16);
    auto [digits89, low10_16_1] = multiplier::mul64x64_to_128(low10_16, 100);
    digits::write_two_digits(result + 2, digits89);
    auto [digits1011, low10_16_2] =
        multiplier::mul64x64_to_128(low10_16_1, 100);
    digits::write_two_digits(result + 4, digits1011);
    auto [digits1213, low10_16_3] =
        multiplier::mul64x64_to_128(low10_16_2, 100);
    digits::write_two_digits(result + 6, digits1213);
    auto [digits1415, unused2] = multiplier::mul64x64_to_128(low10_16_3, 100);
    digits::write_two_digits(result + 8, digits1415);
    exp += 16;
    exp_index = 17 + 1;
  } else {
    // 1 to 16 digits: determine number of digits and write accordingly.
    exp += number_of_digits - 1;
    size_t final_index = number_of_digits + 1;
    exp_index = final_index;

    ////////////////////
    // If <number_of_digits> is unpredictable, then we
    // get branch target buffer (BTB) misses.
    // This will lower the number of instructions per cycle (IPC).
    /////////////////
    ////////////////
    // The code below has poor naming conventions for the variables
    // and it is unclear. It should be rewritten for clarity.
    // The constants are from
    // scripts/optimal_bound.py
    ////////////////
    switch (number_of_digits) {
    case 16: {
      // We divide by 10^8, the binary remainder is in low10_8
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 48357032784585167ULL);
      low10_8 = (low10_8 >> 18) | (xdiv10_8 << (64 - 18));
      xdiv10_8 >>= 18;
      auto [digits12, low10_8_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 9, digits12);
      auto [digits23, low10_8_2] = multiplier::mul64x64_to_128(low10_8_1, 100);
      digits::write_two_digits(result + 11, digits23);
      auto [digits45, low10_8_3] = multiplier::mul64x64_to_128(low10_8_2, 100);
      digits::write_two_digits(result + 13, digits45);
      auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_8_3, 100);
      digits::write_two_digits(result + 15, digits67);
      // We divide by 10^6 to get the first digits
      auto [xdiv10_14, low10_14] =
          multiplier::mul64x64_to_128(xdiv10_8, 18446744073710ULL);
      digits::write_two_digits_with_dot_with_one_pad(result, xdiv10_14);
      auto [digits89, low10_14_1] = multiplier::mul64x64_to_128(low10_14, 100);
      digits::write_two_digits(result + 3, digits89);
      auto [digits1011, low10_14_2] =
          multiplier::mul64x64_to_128(low10_14_1, 100);
      digits::write_two_digits(result + 5, digits1011);
      auto [digits1213, unused] = multiplier::mul64x64_to_128(low10_14_2, 100);
      digits::write_two_digits(result + 7, digits1213);
    } break;

    case 15: {
      // We divide by 10^8, the binary remainder is in low10_8
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 48357032784585167ULL);
      low10_8 = (low10_8 >> 18) | (xdiv10_8 << (64 - 18));
      xdiv10_8 >>= 18;
      auto [digits12, low10_8_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 8, digits12);
      auto [digits23, low10_8_2] = multiplier::mul64x64_to_128(low10_8_1, 100);
      digits::write_two_digits(result + 10, digits23);
      auto [digits45, low10_8_3] = multiplier::mul64x64_to_128(low10_8_2, 100);
      digits::write_two_digits(result + 12, digits45);
      auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_8_3, 100);
      digits::write_two_digits(result + 14, digits67);
      // We divide by 10^6 to get the first digits
      auto [xdiv10_14, low10_14] =
          multiplier::mul64x64_to_128(xdiv10_8, 18446744073710ULL);
      digits::write_one_digit_with_dot(result, xdiv10_14);
      auto [digits89, low10_14_1] = multiplier::mul64x64_to_128(low10_14, 100);
      digits::write_two_digits(result + 2, digits89);
      auto [digits1011, low10_14_2] =
          multiplier::mul64x64_to_128(low10_14_1, 100);
      digits::write_two_digits(result + 4, digits1011);
      auto [digits1213, unused] = multiplier::mul64x64_to_128(low10_14_2, 100);
      digits::write_two_digits(result + 6, digits1213);
    } break;

    case 14: {
      // We divide by 10^8, the binary remainder is in low10_8
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 48357032784585167ULL);
      low10_8 = (low10_8 >> 18) | (xdiv10_8 << (64 - 18));
      xdiv10_8 >>= 18;
      auto [digits12, low10_8_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 7, digits12);
      auto [digits23, low10_8_2] = multiplier::mul64x64_to_128(low10_8_1, 100);
      digits::write_two_digits(result + 9, digits23);
      auto [digits45, low10_8_3] = multiplier::mul64x64_to_128(low10_8_2, 100);
      digits::write_two_digits(result + 11, digits45);
      auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_8_3, 100);
      digits::write_two_digits(result + 13, digits67);
      // We divide by 10^4 to get the first digits
      auto [xdiv10_12, low10_12] =
          multiplier::mul64x64_to_128(xdiv10_8, 1844674407370956ULL);
      digits::write_two_digits_with_dot_with_one_pad(result, xdiv10_12);
      auto [digits89, low10_12_1] = multiplier::mul64x64_to_128(low10_12, 100);
      digits::write_two_digits(result + 3, digits89);
      auto [digits1011, unused] = multiplier::mul64x64_to_128(low10_12_1, 100);
      digits::write_two_digits(result + 5, digits1011);
    } break;

    case 13: {
      // We divide by 10^8, the binary remainder is in low10_8
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 48357032784585167ULL);
      low10_8 = (low10_8 >> 18) | (xdiv10_8 << (64 - 18));
      xdiv10_8 >>= 18;
      auto [digits12, low10_8_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 6, digits12);
      auto [digits23, low10_8_2] = multiplier::mul64x64_to_128(low10_8_1, 100);
      digits::write_two_digits(result + 8, digits23);
      auto [digits45, low10_8_3] = multiplier::mul64x64_to_128(low10_8_2, 100);
      digits::write_two_digits(result + 10, digits45);
      auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_8_3, 100);
      digits::write_two_digits(result + 12, digits67);
      // We divide by 10^4 to get the first digits
      auto [xdiv10_12, low10_12] =
          multiplier::mul64x64_to_128(xdiv10_8, 1844674407370956ULL);
      digits::write_one_digit_with_dot(result, xdiv10_12);
      auto [digits89, low10_12_1] = multiplier::mul64x64_to_128(low10_12, 100);
      digits::write_two_digits(result + 2, digits89);
      auto [digits1011, unused] = multiplier::mul64x64_to_128(low10_12_1, 100);
      digits::write_two_digits(result + 4, digits1011);
    } break;
    case 12: {
      // We divide by 10^6, the binary remainder is in low10_6
      auto [xdiv10_6, low10_6] = multiplier::mul64x64_to_128(mantissa, 18446744073710ULL);
      auto [digits12, low10_6_1] = multiplier::mul64x64_to_128(low10_6, 100);
      digits::write_two_digits(result + 7, digits12);
      auto [digits23, low10_6_2] = multiplier::mul64x64_to_128(low10_6_1, 100);
      digits::write_two_digits(result + 9, digits23);
      auto [digits45, low10_6_3] = multiplier::mul64x64_to_128(low10_6_2, 100);
      digits::write_two_digits(result + 11, digits45);
      // We divide by 10^4 to get the first digits
      auto [xdiv10_10, low10_12] = multiplier::mul64x64_to_128(xdiv10_6, 1844674407370956ULL);
      digits::write_two_digits_with_dot_with_one_pad(result, xdiv10_10);
      auto [digits89, low10_12_1] = multiplier::mul64x64_to_128(low10_12, 100);
      digits::write_two_digits(result + 3, digits89);
      auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_12_1, 100);
      digits::write_two_digits(result + 5, digits67);
    } break;

    case 11: {
      // We divide by 10^6, the binary remainder is in low10_6
      auto [xdiv10_6, low10_6] = multiplier::mul64x64_to_128(mantissa, 18446744073710ULL);
      auto [digits12, low10_6_1] = multiplier::mul64x64_to_128(low10_6, 100);
      digits::write_two_digits(result + 6, digits12);
      auto [digits23, low10_6_2] = multiplier::mul64x64_to_128(low10_6_1, 100);
      digits::write_two_digits(result + 8, digits23);
      auto [digits45, low10_6_3] = multiplier::mul64x64_to_128(low10_6_2, 100);
      digits::write_two_digits(result + 10, digits45);
      // We divide by 10^4 to get the first digits
      auto [xdiv10_10, low10_12] = multiplier::mul64x64_to_128(xdiv10_6, 1844674407370956);
      digits::write_one_digit_with_dot(result, xdiv10_10);
      auto [digits89, low10_12_1] = multiplier::mul64x64_to_128(low10_12, 100);
      digits::write_two_digits(result + 2, digits89);
      auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_12_1, 100);
      digits::write_two_digits(result + 4, digits67);
    } break;
    case 10: {
      // We divide by 10^8, the binary remainder is in low10_8
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 184467440738ULL);
      digits::write_two_digits_with_dot_with_one_pad(result, xdiv10_8);
      auto [digits12, low10_6_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 3, digits12);
      auto [digits23, low10_6_2] = multiplier::mul64x64_to_128(low10_6_1, 100);
      digits::write_two_digits(result + 5, digits23);
      auto [digits45, low10_6_3] = multiplier::mul64x64_to_128(low10_6_2, 100);
      digits::write_two_digits(result + 7, digits45);
      auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_6_3, 100);
      digits::write_two_digits(result + 9, digits67);
    } break;

    case 9: {
      // We divide by 10^8, the binary remainder is in low10_8
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 184467440738ULL);
      digits::write_one_digit_with_dot(result, xdiv10_8);
      auto [digits12, low10_6_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 2, digits12);
      auto [digits23, low10_6_2] = multiplier::mul64x64_to_128(low10_6_1, 100);
      digits::write_two_digits(result + 4, digits23);
      auto [digits45, low10_6_3] = multiplier::mul64x64_to_128(low10_6_2, 100);
      digits::write_two_digits(result + 6, digits45);
      auto [digits67, unused1] = multiplier::mul64x64_to_128(low10_6_3, 100);
      digits::write_two_digits(result + 8, digits67);
    } break;

    case 8: {
      // We divide by 10^6, the binary remainder is in low10_6
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 18446744073710ULL);
      digits::write_two_digits_with_dot_with_one_pad(result, xdiv10_8);
      auto [digits12, low10_6_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 3, digits12);
      auto [digits23, low10_6_2] = multiplier::mul64x64_to_128(low10_6_1, 100);
      digits::write_two_digits(result + 5, digits23);
      auto [digits45, unused] = multiplier::mul64x64_to_128(low10_6_2, 100);
      digits::write_two_digits(result + 7, digits45);
    } break;

    case 7: {
      // We divide by 10^6, the binary remainder is in low10_6
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 18446744073710ULL);
      digits::write_one_digit_with_dot(result, xdiv10_8);
      auto [digits12, low10_6_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 2, digits12);
      auto [digits23, low10_6_2] = multiplier::mul64x64_to_128(low10_6_1, 100);
      digits::write_two_digits(result + 4, digits23);
      auto [digits45, unused] = multiplier::mul64x64_to_128(low10_6_2, 100);
      digits::write_two_digits(result + 6, digits45);

    } break;

    case 6: {
      // We divide by 10^4, the binary remainder is in low10_8
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 1844674407370956ULL);
      digits::write_two_digits_with_dot_with_one_pad(result, xdiv10_8);
      auto [digits12, low10_6_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 3, digits12);
      auto [digits23, unused] = multiplier::mul64x64_to_128(low10_6_1, 100);
      digits::write_two_digits(result + 5, digits23);
    } break;

    case 5: {
      // We divide by 10^4, the binary remainder is in low10_4
      auto [xdiv10_8, low10_8] =
          multiplier::mul64x64_to_128(mantissa, 1844674407370956ULL);
      digits::write_one_digit_with_dot(result, xdiv10_8);
      auto [digits12, low10_6_1] = multiplier::mul64x64_to_128(low10_8, 100);
      digits::write_two_digits(result + 2, digits12);
      auto [digits23, unused] = multiplier::mul64x64_to_128(low10_6_1, 100);
      digits::write_two_digits(result + 4, digits23);
    } break;

    case 4: {
      // remaining 4 digits: write 2 + 2 with dot
      auto [r, s_unique] = digits::div100v(mantissa);
      digits::write_two_digits_with_dot_with_one_pad(result, r);
      digits::write_two_digits_v(result + final_index - 2, s_unique);
    } break;

    case 3: {
      // remaining 3 digits: write 2 + 1 with dot
      auto [r, s_unique] = digits::div100v(mantissa);
      digits::write_two_digits_v(result + final_index - 2, s_unique);
      digits::write_one_digit_with_dot(result, r);
    } break;

    case 2: {
      // remaining 2 digits: write 2 with dot
      digits::write_two_digits_with_dot(result, mantissa);
    } break;

    case 1: {
      *result = '0' + mantissa;
      if (mantissa == 0) {
        return 1;
      }
    } break;

    default:
      champagne_lemire_unreachable();
    }
  }

  if (exp) { // We do not print the exponent if mantissa is zero but zero is
             // handled above.
    // About 20 instructions for the exponent?
    memcpy(result + exp_index, "E-", 2);
    if (exp < 0) {
      exp_index += 2;
      exp = -exp;
    } else {
      exp_index += 1;
    }

    if constexpr (sizeof(T) == 8) {
      if (exp >= 100) { // 3 digits
        digits::write_three_digits(result + exp_index, exp);
        exp_index += 3;
      } else { // 2 digits
        // If we need fewer than 2 digits, this will write a leading zero.
        digits::write_two_digits(result + exp_index, exp);
        exp_index += 2;
      }
    } else {
      // If we need fewer than 2 digits, this will write a leading zero.
      digits::write_two_digits(result + exp_index, exp);
      exp_index += 2;
    }
  }
  return exp_index;
}
} // namespace champagne_lemire
#endif // CHAMPAGNE_LEMIRE_FAST_H
