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
  GRISU3 = 15,
  SWIFT_DTOA = 16,
  YY_DOUBLE = 17,
  COUNT // Keep last
};

template<arithmetic_float T>
struct BenchArgs {
  using Type = T;

  BenchArgs(const std::string& name = {}, int (*func)(T, std::span<char>&) = {},
            bool used = true, size_t testRepeat = 100)
      : name(name), func(func), used(used), testRepeat(testRepeat) {}

  std::string name{};
  int (*func)(T, std::span<char>&){};
  bool used{};
  size_t testRepeat{100};
};

template<arithmetic_float T>
int dragon4(T d, std::span<char>& buffer) {
  if constexpr (std::is_same_v<T, float>)
    return PrintFloat32(buffer.data(), buffer.size(), d,
                        PrintFloatFormat_Positional, -1);
  else
    return PrintFloat64(buffer.data(), buffer.size(), d,
                        PrintFloatFormat_Positional, -1);
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

template<arithmetic_float T>
int snprintf(T d, std::span<char>& buffer) {
  if constexpr (std::is_same_v<T, float>)
    return std::snprintf(buffer.data(), buffer.size(), "%.9g", d);
  else
    return std::snprintf(buffer.data(), buffer.size(), "%.17g", d);
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
  // const char* end_ptr = jkj::dragonbox::to_chars(d, buffer.data());
  // return end_ptr - buffer.data();

  const auto v = jkj::dragonbox::to_decimal(d);
  return to_chars(v.significand, v.exponent, v.is_negative, buffer.data());
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
  // return to_chars(fields.mantissa, fields.exponent, sign, buffer.data());

  char* ptr = buffer.data();
  if(sign) *ptr++ = '-';
  using traits = jkj::dragonbox::default_float_traits<T>;
  using carrier_uint = typename traits::carrier_uint;
  const char* end = jkj::dragonbox::to_chars_detail::to_chars<T, traits>(
      static_cast<carrier_uint>(fields.mantissa), fields.exponent, ptr);
  return end - buffer.data();
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
int abseil(T d, std::span<char>& buffer) {
  // StrAppend is faster but only outputs 6 digits after the decimal point
  // std::string s;
  // absl::StrAppend(&s, d);
  // std::copy(s.begin(), s.end(), buffer.begin());
  // return size(s);
  if constexpr (std::is_same_v<T, float>)
    return absl::SNPrintF(buffer.data(), buffer.size(), "%.9g", d);
  else
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
  std::cerr << "std::to_chars not supported" << std::endl;
  std::abort();
#endif
}

template <arithmetic_float T>
std::array<BenchArgs<T>, Benchmarks::COUNT> initArgs(bool errol = false) {
  std::array<BenchArgs<T>, Benchmarks::COUNT> args;
  args[Benchmarks::DRAGON4]           = { "dragon4"           , Benchmarks::dragon4<T>           , true                          , 10 };
  args[Benchmarks::ERROL3]            = { "errol3"            , Benchmarks::errol3<T>            , errol };
  args[Benchmarks::TO_STRING]         = { "std::to_string"    , Benchmarks::to_string<T>         , ERROL_SUPPORTED };
  args[Benchmarks::FMT_FORMAT]        = { "fmt::format"       , Benchmarks::fmt_format<T>        , true };
  args[Benchmarks::NETLIB]            = { "netlib"            , Benchmarks::netlib<T>            , NETLIB_SUPPORTED  && std::is_same_v<T, double>, 10 };
  args[Benchmarks::SNPRINTF]          = { "snprintf"          , Benchmarks::snprintf<T>          , true };
  args[Benchmarks::GRISU2]            = { "grisu2"            , Benchmarks::grisu2<T>            , std::is_same_v<T, double> };
  args[Benchmarks::GRISU_EXACT]       = { "grisu_exact"       , Benchmarks::grisu_exact<T>       , true };
  args[Benchmarks::SCHUBFACH]         = { "schubfach"         , Benchmarks::schubfach<T>         , true };
  args[Benchmarks::DRAGONBOX]         = { "dragonbox"         , Benchmarks::dragonbox<T>         , true };
  args[Benchmarks::RYU]               = { "ryu"               , Benchmarks::ryu<T>               , true };
  args[Benchmarks::TEJU_JAGUA]        = { "teju_jagua"        , Benchmarks::teju_jagua<T>        , true };
  args[Benchmarks::DOUBLE_CONVERSION] = { "double_conversion" , Benchmarks::double_conversion<T> , true };
  args[Benchmarks::ABSEIL]            = { "abseil"            , Benchmarks::abseil<T>            , ABSEIL_SUPPORTED };
  args[Benchmarks::STD_TO_CHARS]      = { "std::to_chars"     , Benchmarks::std_to_chars<T>      , TO_CHARS_SUPPORTED };
  args[Benchmarks::GRISU3]            = { "grisu3"            , Benchmarks::grisu3<T>            , std::is_same_v<T, double> };
  args[Benchmarks::SWIFT_DTOA]        = { "SwiftDtoa"         , Benchmarks::swiftDtoa<T>         , SWIFT_LIB_SUPPORTED };
  args[Benchmarks::YY_DOUBLE]         = { "yy_double"         , Benchmarks::yy_double<T>         , YY_DOUBLE_SUPPORTED && std::is_same_v<T, double> };
  return args;
};

}  // namespace Benchmarks

#endif
