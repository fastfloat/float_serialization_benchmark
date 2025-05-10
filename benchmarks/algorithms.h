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

#include <array>
#include <span>

#include "cpp/common/traits.hpp"  // Teju Jagua
#include "PrintFloat.h"  // Dragon4
#include "double-conversion/double-conversion.h"
#include "dragonbox/dragonbox_to_chars.h"
#include "grisu2.h"
#include "grisu3.h"
#include "grisu_exact.h"
#include "ieeeToString.h"
#include "floatutils.h"
#include "ryu/ryu.h"
#include "schubfach_32.h"
#include "schubfach_64.h"
#if SWIFT_LIB_SUPPORTED
#include "swift/Runtime/SwiftDtoa.h"
#endif
#if (__SIZEOF_INT128__ == 16) && (defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER))
#include "yy_double.h"
#define YY_DOUBLE_SUPPORTED 1
#else
#define YY_DOUBLE_SUPPORTED 0
#endif

template<arithmetic_float T>
struct BenchArgs {
  using Type = T;
  using BenchFn = std::function<int(T, std::span<char>&)>;

  BenchArgs(const std::string& name = {}, BenchFn func = {}, bool used = true, size_t testRepeat = 100)
      : name(name), func(func), used(used), testRepeat(testRepeat) {}

  std::string name{};
  BenchFn func{};
  bool used{};
  size_t testRepeat{100};

  static void initFixedSize(size_t size) {
    fixedSize = size;
    snprintf(formatStr, sizeof(formatStr), "%%.%zug", fixedSize);
    formatStrStr = fmt::format("{{:.{}g}}", fixedSize);
  }
  static inline size_t fixedSize;
  static inline char formatStr[10];
  static inline std::string formatStrStr;
};

namespace BenchmarkShortest {

template<arithmetic_float T>
int dragon4(T d, std::span<char>& buffer) {
  if constexpr (std::is_same_v<T, float>)
    return PrintFloat32(buffer.data(), buffer.size(), d,
                        PrintFloatFormat_Positional, -1);
  else
    return PrintFloat64(buffer.data(), buffer.size(), d,
                        PrintFloatFormat_Positional, -1);
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
      // Adding lots of zeroes so can create a really large output:
      //std::fill_n(buffer.data() + i, -decpt, '0'); // Add leading zeros
      //i += -decpt;
    }
    auto write_exponent = [&buffer, &i](int value) {
      if (value >= 100) {
          buffer[i++] = '0' + value / 100;
          value %= 100;
          buffer[i++] = '0' + value / 10;
          value %= 10;
          buffer[i++] = '0' + value;
      } else if (value >= 10) {
          buffer[i++] = '0' + value / 10;
          value %= 10;
          buffer[i++] = '0' + value;
      } else {
          buffer[i++] = '0' + value;
      }
    };
    // Fractional part (if any remaining digits)
    const int remaining_digits = rve - (result + std::max(0, decpt));
    if (remaining_digits > 0) {
      if (decpt > 0)
        buffer[i++] = '.';
      std::copy_n(result + std::max(0, decpt), remaining_digits, buffer.data() + i);
      i += remaining_digits;
      if(decpt < 0) {
        buffer[i++] = 'E';
        buffer[i++] = '-';
        write_exponent(-decpt);
      }
    } else if (remaining_digits < 0) {
      buffer[i++] = 'E';
      write_exponent(-remaining_digits);
    }

    freedtoa(result);
    return i;
  } else {
    std::cerr << "problem with " << d << std::endl;
    std::abort();
  }
#else
  std::cerr << "netlib not supported" << std::endl;
  std::abort();
#endif
}

// No errol3 implementation optimized for float instead of double ?
template<arithmetic_float T>
int errol3(T d, std::span<char>& buffer) {
#if ERROL_SUPPORTED
  errol3_dtoa(d, buffer.data());  // returns the exponent
  return std::strlen(buffer.data());
#else
  std::cerr << "errol3 not supported" << std::endl;
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
  const auto it = fmt::format_to(buffer.data(), "{}", d);
  return std::distance(buffer.data(), it);
}

// grisu2 is hardcoded for double.
template<arithmetic_float T>
int grisu2(T d, std::span<char>& buffer) {
  const char* newp = grisu2::Dtoa(buffer.data(), d);
  return newp - buffer.data();
}

// grisu3 is hardcoded for double.
template<arithmetic_float T>
int grisu3(T d, std::span<char>& buffer) {
  const char* newp = grisu3::Dtoa(buffer.data(), d);
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
  using namespace double_conversion;
  const static DoubleToStringConverter conv(
      DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN | DoubleToStringConverter::UNIQUE_ZERO,
      "inf", "nan", 'e', -4, 6, 0, 0);

  double_conversion::StringBuilder builder(buffer.data(), buffer.size());
  const bool valid = std::is_same_v<T, float>
                         ? conv.ToShortestSingle(d, &builder)
                         : conv.ToShortest(d, &builder);
  if (!valid) {
    std::cerr << "problem with " << d << std::endl;
    std::abort();
  }
  return strlen(builder.Finalize());
}

template<arithmetic_float T>
int swiftDtoa(T d, std::span<char>& buffer) {
#if SWIFT_LIB_SUPPORTED
  if constexpr (std::is_same_v<T, float>)
    return swift_dtoa_optimal_float(d, buffer.data(), buffer.size());
  else
    return swift_dtoa_optimal_double(d, buffer.data(), buffer.size());
#else
  std::cerr << "swift code not supported" << std::endl;
  std::abort();
#endif
}

template<arithmetic_float T>
int yy_double(T d, std::span<char>& buffer) {
#if YY_DOUBLE_SUPPORTED
  const char* end_ptr = yy_double_to_string(d, buffer.data());
  return end_ptr - buffer.data();
#else
  std::cerr << "yy_double not supported" << std::endl;
  std::abort();
#endif
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
  std::cerr << "std::to_chars not supported" << std::endl;
  std::abort();
#endif
}

}  // namespace BenchmarksShortest

namespace BenchmarkFixedSize {

template<arithmetic_float T>
int dragon4(T d, std::span<char>& buffer) {
  if constexpr (std::is_same_v<T, float>)
    return PrintFloat32(buffer.data(), buffer.size(), d,
                        PrintFloatFormat_Positional, BenchArgs<T>::fixedSize);
  else
    return PrintFloat64(buffer.data(), buffer.size(), d,
                        PrintFloatFormat_Positional, BenchArgs<T>::fixedSize);
}

template<arithmetic_float T>
int netlib(T d, std::span<char>& buffer) {
#if NETLIB_SUPPORTED
  char* res;
  if constexpr (std::is_same_v<T, float>)
    res = g_ffmt(buffer.data(), &d, BenchArgs<T>::fixedSize, buffer.size());
  else
    res = g_dfmt(buffer.data(), &d, BenchArgs<T>::fixedSize, buffer.size());
  *res = '\0';
  return res - buffer.data() + 1;
#else
  std::cerr << "netlib not supported" << std::endl;
  std::abort();
#endif
}

template<arithmetic_float T>
int abseil(T d, std::span<char>& buffer) {
  // StrAppend is faster but only outputs 6 digits after the decimal point
  // std::string s;
  // absl::StrAppend(&s, d);
  // std::copy(s.begin(), s.end(), buffer.begin());
  // return size(s);
  return absl::SNPrintF(buffer.data(), buffer.size(),
                        BenchArgs<T>::formatStr, d);
}

template<arithmetic_float T>
int snprintf(T d, std::span<char>& buffer) {
  return std::snprintf(buffer.data(), buffer.size(),
                       BenchArgs<T>::formatStr, d);
}

template<arithmetic_float T>
int fmt_format(T d, std::span<char>& buffer) {
  const auto it = fmt::format_to(buffer.begin(),
                                 fmt::runtime(BenchArgs<T>::formatStrStr), d);
  return std::distance(buffer.begin(), it);
}

template<arithmetic_float T>
int ryu(T d, std::span<char>& buffer) {
  return d2fixed_buffered_n(d, BenchArgs<T>::fixedSize, buffer.data());
}

template<arithmetic_float T>
int double_conversion(T d, std::span<char>& buffer) {
  const static double_conversion::DoubleToStringConverter conv(
      double_conversion::DoubleToStringConverter::NO_FLAGS, "inf", "nan", 'e',
      -6, 21, BenchArgs<T>::fixedSize, BenchArgs<T>::fixedSize);

  double_conversion::StringBuilder builder(buffer.data(), buffer.size());
  if (!conv.ToPrecision(d, BenchArgs<T>::fixedSize, &builder)) {
    std::cerr << "problem with " << d << std::endl;
    std::abort();
  }
  return strlen(builder.Finalize());
}

template<arithmetic_float T>
int std_to_chars(T d, std::span<char>& buffer) {
#if TO_CHARS_SUPPORTED
  const auto [p, ec]
      = std::to_chars(buffer.data(), buffer.data() + buffer.size(), d,
                      std::chars_format::general, BenchArgs<T>::fixedSize);
  if (ec != std::errc()) {
    std::cerr << "problem with " << d << std::endl;
    std::abort();
  }
  return p - buffer.data();
#else
  std::cerr << "std::to_chars not supported" << std::endl;
  std::abort();
#endif
}

}  // namespace BenchmarksShortest

template <typename T>
auto wrap(int (*fn)(T, std::span<char>&)) {
  return [fn](T v, std::span<char>& buf) -> int {
    return fn(v, buf);
  };
}

template <arithmetic_float T>
std::vector<BenchArgs<T>> initArgs(bool use_errol = false, size_t repeat = 0, size_t fixed_size = 0) {
  std::vector<BenchArgs<T>> args;
  if (fixed_size == 0) {  // shortest-length representation
    namespace s = BenchmarkShortest;
    args.emplace_back("dragon4"           , wrap(s::dragon4<T>)           , true                                           , 10);
    args.emplace_back("netlib"            , wrap(s::netlib<T>)            , NETLIB_SUPPORTED && std::is_same_v<T, double>  , 10);
    args.emplace_back("errol3"            , wrap(s::errol3<T>)            , ERROL_SUPPORTED && use_errol);
    args.emplace_back("fmt_format"        , wrap(s::fmt_format<T>)        , true);
    // args.emplace_back("grisu2"            , wrap(s::grisu2<T>)            , std::is_same_v<T, double>);
    args.emplace_back("grisu3"            , wrap(s::grisu3<T>)            , std::is_same_v<T, double>);
    args.emplace_back("grisu_exact"       , wrap(s::grisu_exact<T>)       , true);
    args.emplace_back("schubfach"         , wrap(s::schubfach<T>)         , true);
    args.emplace_back("dragonbox"         , wrap(s::dragonbox<T>)         , true);
    args.emplace_back("ryu"               , wrap(s::ryu<T>)               , true);
    args.emplace_back("teju_jagua"        , wrap(s::teju_jagua<T>)        , true);
    args.emplace_back("double_conversion" , wrap(s::double_conversion<T>) , true);
    args.emplace_back("swiftDtoa"         , wrap(s::swiftDtoa<T>)         , SWIFT_LIB_SUPPORTED);
    args.emplace_back("yy_double"         , wrap(s::yy_double<T>)         , YY_DOUBLE_SUPPORTED && std::is_same_v<T, double>);
    args.emplace_back("std::to_chars"     , wrap(s::std_to_chars<T>)      , TO_CHARS_SUPPORTED);
    // to_string, snprintf and abseil do not support shortest-length representation
    // grisu2 does not round-trip correctly
  } else {  // fixed-length representation
    fmt::println("# testing fixed-size output to {} digits", fixed_size);
    BenchArgs<T>::initFixedSize(fixed_size);

    namespace f = BenchmarkFixedSize;
    args.emplace_back("dragon4"           , wrap(f::dragon4<T>)           , true                 , 10);
    args.emplace_back("netlib"            , wrap(f::netlib<T>)            , NETLIB_SUPPORTED     , 10);
    args.emplace_back("abseil"            , wrap(f::abseil<T>)            , ABSEIL_SUPPORTED);
    args.emplace_back("snprintf"          , wrap(f::snprintf<T>)          , true);
    args.emplace_back("fmt_format"        , wrap(f::fmt_format<T>)        , true);
    args.emplace_back("ryu"               , wrap(f::ryu<T>)               , std::is_same_v<T, double>);
    args.emplace_back("double_conversion" , wrap(f::double_conversion<T>) , true);
    args.emplace_back("std::to_chars"     , wrap(f::std_to_chars<T>)      , TO_CHARS_SUPPORTED);
  }

  if (repeat > 0) {
    fmt::println("# forcing repeat count to {}", repeat);
    for (auto &arg : args)
      arg.testRepeat = repeat;
  }

  return args;
};

#endif
