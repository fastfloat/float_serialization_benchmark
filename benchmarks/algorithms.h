#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#ifndef __CYGWIN__
#include "absl/strings/str_format.h"
#endif

#if ERROL_SUPPORTED
#include "errol.h"
#endif

#if TO_CHARS_SUPPORTED
#include <charconv>
#endif

#if NETLIB_SUPPORTED
#include <gdtoa.h>  // Netlib
#endif

#include <fmt/format.h>

#include <span>

#include "cpp/common/traits.hpp"  // Teju Jagua
#include "double-conversion/double-conversion.h"
#include "dragon4.h"
#include "dragonbox/dragonbox_to_chars.h"
#include "grisu2.h"
#include "grisu_exact.h"
#include "ieeeToString.h"
#include "ryu/ryu.h"
#include "schubfach_32.h"
#include "schubfach_64.h"

namespace Benchmarks {

enum Algorithm {
  DRAGON4 = 0,
  ERROL3 = 1,
  TO_STRING = 2,
  FMT_FORMAT = 3,
  NETLIB = 4,
  SNPRINTF = 5,
  GRISU2 = 6,
  GRISU_EXACT = 7,
  SCHUBFACH = 8,
  DRAGONBOX = 9,
  RYU = 10,
  TEJU_JAGUA = 11,
  DOUBLE_CONVERSION = 12,
  ABSEIL = 13,
  STD_TO_CHARS = 14,
  COUNT = 15
};

template<typename T>
concept arithmetic_float
    = std::is_same_v<T, float> || std::is_same_v<T, double>;

template<arithmetic_float T>
struct BenchArgs {
  using Type = T;

  BenchArgs(const std::string& name = {}, int (*func)(T, std::span<char>&) = {},
            bool used = true, unsigned char testRepeat = 100)
      : name(name), func(func), used(used), testRepeat(testRepeat) {}

  std::string name{};
  int (*func)(T, std::span<char>&){};
  bool used{};
  unsigned char testRepeat{100};
};

// No dragon4 implementation optimized for float instead of double ?
template<arithmetic_float T>
int dragon4(T d, std::span<char>& buffer) {
  using IEEE754Type
      = std::conditional_t<std::is_same_v<T, float>, IEEE754f, IEEE754d>;
  const IEEE754Type fields = decode_ieee754(d);

  uint64_t dm;
  int dexp;
  dragon4::Dragon4(dm, dexp, fields.mantissa, fields.exponent, true, true);
  return to_chars(dm, dexp, fields.sign, buffer.data());
}

// No errol3 implementation optimized for float instead of double ?
template<arithmetic_float T>
int errol3(T d, std::span<char>& buffer) {
#if ERROL_SUPPORTED
  errol3_dtoa(d, buffer.data());  // returns the exponent
  return std::strlen(buffer.data());
#else
  std::abort();
#endif
}

template<arithmetic_float T>
int to_string(T d, std::span<char>& buffer) {
  const std::string s = std::to_string(d);
  std::copy(s.begin(), s.end(), buffer.begin());
  return s.size();
}

template<arithmetic_float T>
int fmt_format(T d, std::span<char>& buffer) {
  const std::string s = fmt::format("{}", d);
  std::copy(s.begin(), s.end(), buffer.begin());
  return s.size();
}

// There's no "ftoa", only "dtoa", so not optimized for float.
template<arithmetic_float T>
int netlib(T d, std::span<char>& buffer) {
#if NETLIB_SUPPORTED
  int decpt, sign;
  char* rve;
  char* result = dtoa(d, 0, 0, &decpt, &sign, &rve);
  if (result) {
    int i = 0;
    if (sign)
      buffer[i++] = '-';
    if (decpt > 0) {
      // Integer part
      const int integer_digits = std::min(decpt, static_cast<int>(rve - result));
      std::copy_n(result, integer_digits, buffer.data() + i);
      i += integer_digits;
    } else {
      // Number is < 1 (e.g., 0.000123)
      buffer[i++] = '0';
      buffer[i++] = '.';
      std::fill_n(buffer.data() + i, -decpt, '0'); // Add leading zeros
      i += -decpt;
    }

    // Fractional part (if any remaining digits)
    const int remaining_digits = rve - (result + std::max(0, decpt));
    if (remaining_digits > 0) {
      if (decpt > 0)
        buffer[i++] = '.';
      std::copy_n(result + std::max(0, decpt), remaining_digits, buffer.data() + i);
      i += remaining_digits;
    }

    freedtoa(result);
    return i;
  } else {
    std::cerr << "problem with " << d << std::endl;
    std::abort();
  }
#else
  std::abort();
#endif
}

template<arithmetic_float T>
int snprintf(T d, std::span<char>& buffer) {
  return std::snprintf(buffer.data(), buffer.size(), "%.17g", d);
}

// grisu2::dtoa_impl::grisu2 can take a template type
// However, grisu2::to_chars is hardcoded for double.
template<arithmetic_float T>
int grisu2(T d, std::span<char>& buffer) {
  const char* newp = grisu2::to_chars(buffer.data(), nullptr, d);
  return newp - buffer.data();
}

template<arithmetic_float T>
int grisu_exact(T d, std::span<char>& buffer) {
  const auto v = jkj::grisu_exact(d);
  return to_chars(v.significand, v.exponent, v.is_negative, buffer.data());
}

template<arithmetic_float T>
int schubfach(T d, std::span<char>& buffer) {
  if constexpr (std::is_same_v<T, float>)
    return schubfach::Ftoa(buffer.data(), d) - buffer.data();
  else
    return schubfach::Dtoa(buffer.data(), d) - buffer.data();
}

template<arithmetic_float T>
int dragonbox(T d, std::span<char>& buffer) {
  const char* end_ptr = jkj::dragonbox::to_chars(d, buffer.data());
  return end_ptr - buffer.data();
}

template<arithmetic_float T>
int ryu(T d, std::span<char>& buffer) {
  if constexpr (std::is_same_v<T, float>)
    return f2s_buffered_n(d, buffer.data());
  else
    return d2s_buffered_n(d, buffer.data());
}

template<arithmetic_float T>
int teju_jagua(T d, std::span<char>& buffer) {
  const auto fields = teju::traits_t<T>::teju(d);
  const bool sign = std::signbit(d);
  return to_chars(fields.mantissa, fields.exponent, sign, buffer.data());
}

template<arithmetic_float T>
int double_conversion(T d, std::span<char>& buffer) {
  const static double_conversion::DoubleToStringConverter converter(
      double_conversion::DoubleToStringConverter::NO_FLAGS, "inf", "nan", 'e',
      -4, 6, 0, 0);
  double_conversion::StringBuilder builder(buffer.data(), buffer.size());
  const bool valid = std::is_same_v<T, float>
                         ? converter.ToShortestSingle(d, &builder)
                         : converter.ToShortest(d, &builder);
  if (!valid) {
    std::cerr << "problem with " << d << std::endl;
    std::abort();
  }
  return strlen(builder.Finalize());
}

template<arithmetic_float T>
int abseil(T d, std::span<char>& buffer) {
  // StrAppend is faster but only outputs 6 digits after the decimal point
  // std::string s;
  // absl::StrAppend(&s, d);
  // std::copy(s.begin(), s.end(), buffer.begin());
  // return size(s);
  return absl::SNPrintF(buffer.data(), buffer.size(), "%.17g", d);
}

template<arithmetic_float T>
int std_to_chars(T d, std::span<char>& buffer) {
#if TO_CHARS_SUPPORTED
  const auto [p, ec]
      = std::to_chars(buffer.data(), buffer.data() + buffer.size(), d);
  if (ec != std::errc()) {
    std::cerr << "problem with " << d << std::endl;
    std::abort();
  }
  return p - buffer.data();
#else
  std::abort();
#endif
}

}  // namespace Benchmarks

#endif
