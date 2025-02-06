/**
 * Currently, we benchmark the speed of converting floating-point numbers to
 * strings. We compare the speed of different libraries and methods. The
 * benchmark is run on a file containing one floating-point number per line or
 * synthetic data. The synthetic data is generated using a random number
 * generator. We measure the speed of the conversion by computing the volume of
 * data generated.
 */

// Teju Jagua
#include "cpp/common/traits.hpp"

#ifndef __CYGWIN__
#include "absl/strings/str_cat.h"
#endif

#include "dragonbox/dragonbox_to_chars.h"
#include "ryu/ryu.h"
#include "double-conversion/double-conversion.h"
#include "grisu_exact.h"
#include "dragon4.h"
#include "schubfach_32.h"
#include "schubfach_64.h"

#if __has_include("errol.h")
#include "errol.h"
#define ERROL_SUPPORTED
#endif

#define IEEE_8087
#include "benchutil.h"
#include "cxxopts.hpp"

#if NETLIB_SUPPORTED
#include "gdtoa.h"
#endif

#include "grisu2.h"
#include "random_generators.h"
#include "ieeeToString.h"

#include <fmt/format.h>

#include <climits>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if FROM_CHARS_SUPPORTED
#include <charconv>
#endif

template <typename T>
void process(const std::vector<T> &lines) {
  static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                "The function currently only supports float or double");
  using MantissaType = std::conditional_t<std::is_same_v<T, float>,
                                          uint32_t, uint64_t>;
  using IEEE754Type = std::conditional_t<std::is_same_v<T, float>,
                                         IEEE754f, IEEE754d>;

  // No dragon4 implementation optimized for float instead of double ?
  pretty_print(lines, "dragon4", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    for (const auto d : lines) {
      MantissaType dmantissa;
      int dexp;
      const IEEE754Type fields = decode_ieee754(d);
      dragon4::Dragon4(dmantissa, dexp, fields.mantissa, fields.exponent,
                       true, true);
      char buffer[100];
      volume += to_chars(dmantissa, dexp, fields.sign, buffer);
    }
    return volume;
  }, 10);

#ifdef ERROL_SUPPORTED
  // No errol3 implementation optimized for float instead of double ?
  pretty_print(lines, "errol3", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      errol3_dtoa(d, buffer); // returns the exponent
      volume += std::strlen(buffer);
    }
    return volume;
  });
#else
  std::cout << "# errol not supported" << std::endl;
#endif

  pretty_print(lines, "std::to_string", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    for (const auto d : lines) {
      const std::string s = std::to_string(d);
      volume += s.size();
    }
    return volume;
  });

  pretty_print(lines, "fmt::format", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    for (const auto d : lines) {
      const std::string s = fmt::format("{}", d);
      volume += s.size();
    }
    return volume;
  });

#if NETLIB_SUPPORTED
  // There's no "ftoa", only "dtoa", so not optimized for float.
  pretty_print(lines, "netlib", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char *result;
    int decpt, sign;
    char *rve;
    for (const auto d : lines) {
      char *result = dtoa(d, 0, 0, &decpt, &sign, &rve);
      if (result) {
        volume += (rve - result);
        freedtoa(result);
      } else {
        std::cerr << "problem with " << d << std::endl;
        std::abort();
      }
    }
    return volume;
  }, 10);
#else
  std::cout << "# netlib not supported" << std::endl;
#endif

  pretty_print(lines, "sprintf", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      volume += snprintf(buffer, sizeof(buffer), "%g", d);
    }
    return volume;
  });

  // grisu2::dtoa_impl::grisu2 can take a template type
  // However, grisu2::to_chars is hardcoded for double.
  pretty_print(lines, "grisu2", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const char *newp = grisu2::to_chars(buffer, nullptr, d);
      volume += newp - buffer;
    }
    return volume;
  });

  pretty_print(lines, "grisu_exact", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      auto v = jkj::grisu_exact(d);
      volume += to_chars(v.significand, v.exponent, v.is_negative, buffer);
    }
    return volume;
  });

  pretty_print(lines, "schubfach", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const char* end_ptr = std::is_same_v<T, float>
                                ? schubfach::Ftoa(buffer, d)
                                : schubfach::Dtoa(buffer, d);
      volume += end_ptr - &buffer[0];
    }
    return volume;
  });

  pretty_print(lines, "dragonbox", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const char *end_ptr = jkj::dragonbox::to_chars(d, buffer);
      volume += end_ptr - &buffer[0];
    }
    return volume;
  });

  pretty_print(lines, "ryu", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      volume += std::is_same_v<T, float> ? f2s_buffered_n(d, buffer)
                                         : d2s_buffered_n(d, buffer);
    }
    return volume;
  });

  pretty_print(lines, "teju_jagua", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const auto fields = teju::traits_t<T>::teju(d);
      const bool sign = std::signbit(d);
      volume += to_chars(fields.mantissa, fields.exponent, sign, buffer);
    }
    return volume;
  });

  pretty_print(lines, "double_conversion", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    constexpr int kBufferSize = 100;
    char buffer[kBufferSize];
    const double_conversion::DoubleToStringConverter converter(
        double_conversion::DoubleToStringConverter::NO_FLAGS, "inf", "nan", 'e',
        -4, 6, 0, 0);
    double_conversion::StringBuilder builder(buffer, kBufferSize);

    for (const auto d : lines) {
      builder.Reset();
      const bool valid = std::is_same_v<T, float>
                             ? converter.ToShortestSingle(d, &builder)
                             : converter.ToShortest(d, &builder);
      if (!valid) {
        std::cerr << "problem with " << d << std::endl;
        std::abort();
      }
      volume += strlen(builder.Finalize());
    }
    return volume;
  });

  pretty_print(lines, "abseil", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    std::string buffer;
    for (const auto d : lines) {
      buffer.clear();
      absl::StrAppend(&buffer, d);
      volume += buffer.size();
    }
    return volume;
  });

#if FROM_CHARS_SUPPORTED
  pretty_print(lines, "std::to_chars", [](const std::vector<T> &lines) -> int {
    int volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const auto [p, ec] = std::to_chars(buffer, buffer + sizeof(buffer), d);
      if(ec != std::errc()) {
        std::cerr << "problem with " << d << std::endl;
        std::abort();
      }
      volume += p - buffer;
    }
    return volume;
  });
#else
  std::cout << "# std::to_chars not supported" << std::endl;
#endif
}

void fileload(const char *filename) {
  std::ifstream inputfile(filename);
  if (!inputfile) {
    std::cerr << "can't open " << filename << std::endl;
    return;
  }

  std::vector<double> lines;
  lines.reserve(10000); // let us reserve plenty of memory.
  for (std::string line; getline(inputfile, line);) {
    try {
      lines.push_back(std::stod(line));
    } catch (...) {
      std::cerr << "problem with " << line << std::endl;
      std::cerr << "We expect floating-point numbers (one per line)."
                << std::endl;
      std::abort();
    }
  }
  std::cout << "# read " << lines.size() << " lines " << std::endl;
  process(lines);
}

void parse_random_numbers(size_t howmany, std::string random_model) {
  std::cout << "# parsing random numbers" << std::endl;
  std::vector<double> lines;
  auto g = get_generator_by_name(random_model);
  std::cout << "model: " << g->describe() << std::endl;
  std::cout << "volume: " << howmany << " floats" << std::endl;
  lines.reserve(howmany); // let us reserve plenty of memory.
  for (size_t i = 0; i < howmany; i++) {
    double line = g->new_float();
    lines.push_back(line);
  }
  process(lines);
}

cxxopts::Options
    options("benchmark",
            "Compute the parsing speed of different number parsers.");

int main(int argc, char **argv) {
  try {
    options.add_options()("f,file", "File name.",
                          cxxopts::value<std::string>()->default_value(""))(
        "v,volume", "Volume (number of floats generated).",
        cxxopts::value<size_t>()->default_value("100000"))(
        "m,model", "Random Model.",
        cxxopts::value<std::string>()->default_value("uniform"))(
        "s,single", "Use single precision instead of double.",
        cxxopts::value<bool>()->default_value("false"))(
        "h,help", "Print usage.");
    const auto result = options.parse(argc, argv);
    if (result["help"].as<bool>()) {
      std::cout << options.help() << std::endl;
      return EXIT_SUCCESS;
    }
    const bool single = result["single"].as<bool>();
    const auto filename = result["file"].as<std::string>();
    if (filename.empty()) {
      parse_random_numbers(result["volume"].as<size_t>(),
                           result["model"].as<std::string>());
      std::cout << "# You can also provide a filename (with the -f flag): it "
                   "should contain one "
                   "string per line corresponding to a number"
                << std::endl;
    } else {
      fileload(filename.c_str());
    }
  } catch (const std::exception &e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
