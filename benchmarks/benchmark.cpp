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
#include "schubfach_64.h"
#if __has_include("errol.h")
#include "errol.h"
#define ERROL_SUPPORTED 1
#else
#define ERROL_SUPPORTED 0
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

#include <charconv>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <stdio.h>
#include <string>
#include <vector>

#if FROM_CHARS_DOUBLE_SUPPORTED
#include <charconv>
#endif

void process(std::vector<double> &lines) {
  pretty_print(lines, "dragon4", [](const std::vector<double> &lines) {
    double volume = 0;
    for (const auto d : lines) {
      uint64_t dmantissa;
      int dexp;
      const IEEE754d fields = decode_ieee754(d);
      dragon4::Dragon4(dmantissa, dexp, fields.mantissa, fields.exponent,
                       true, true);
      char buffer[100];
      volume += to_chars(dmantissa, dexp, fields.sign, buffer);
    }
    return volume;
  });
  
#if ERROL_SUPPORTED
  pretty_print(lines, "errol3", [](const std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      errol3_dtoa(d, buffer); // returns the exponent?
      volume += std::strlen(buffer);
    }
    return volume;
  });
#else
  std::cout << "# errol not supported" << std::endl;
#endif // ERROL_SUPPORTED
  pretty_print(lines, "std::to_string", [](const std::vector<double> &lines) {
    double volume = 0;
    for (const auto d : lines) {
      const std::string s = std::to_string(d);
      volume += s.size();
    }
    return volume;
  });

  pretty_print(lines, "fmt::format", [](const std::vector<double> &lines) {
    double volume = 0;
    for (const auto d : lines) {
      const std::string s = fmt::format("{}", d);
      volume += s.size();
    }
    return volume;
  });

#if NETLIB_SUPPORTED
  pretty_print(lines, "netlib", [](const std::vector<double> &lines) {
    double volume = 0;
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

  pretty_print(lines, "sprintf", [](const std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      volume += snprintf(buffer, sizeof(buffer), "%g", d);
    }
    return volume;
  });

  pretty_print(lines, "grisu2", [](const std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const char *newp = grisu2::to_chars(buffer, nullptr, d);
      volume += newp - buffer;
    }
    return volume;
  });

  pretty_print(lines, "grisu_exact", [](const std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      auto v = jkj::grisu_exact(d);
      volume += to_chars(v.significand, v.exponent, v.is_negative, buffer);
    }
    return volume;
  });

  pretty_print(lines, "schubfach", [](const std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const char *end_ptr = schubfach::Dtoa(buffer, d);
      volume += end_ptr - &buffer[0];
    }
    return volume;
  });

  pretty_print(lines, "dragonbox", [](const std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const char *end_ptr = jkj::dragonbox::to_chars(d, buffer);
      volume += end_ptr - &buffer[0];
    }
    return volume;
  });

  pretty_print(lines, "ryu", [](const std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      volume += d2s_buffered_n(d, buffer);
    }
    return volume;
  });

  pretty_print(lines, "teju_jagua", [](const std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (const auto d : lines) {
      const auto fields = teju::traits_t<double>::teju(d);
      const bool sign = (*reinterpret_cast<const uint64_t*>(&d) >> 63) & 1;
      volume += to_chars(fields.mantissa, fields.exponent, sign, buffer);
    }
    return volume;
  });

  pretty_print(lines, "double_conversion", [](const std::vector<double> &lines) {
    double volume = 0;
    const double_conversion::DoubleToStringConverter converter(
        double_conversion::DoubleToStringConverter::NO_FLAGS, "inf", "nan", 'e',
        -4, 6, 0, 0);
    const int kBufferSize = 100;
    char buffer[kBufferSize];
    double_conversion::StringBuilder builder(buffer, kBufferSize);

    for (const auto d : lines) {
      builder.Reset();
      if (!converter.ToShortest(d, &builder)) {
        std::cerr << "problem with " << d << std::endl;
        std::abort();
      }
      volume += strlen(builder.Finalize());
    }
    return volume;
  });

  pretty_print(lines, "abseil", [](const std::vector<double> &lines) {
    double volume = 0;
    std::string buffer;
    for (const auto d : lines) {
      buffer.clear();
      absl::StrAppend(&buffer, d);
      volume += buffer.size();
    }
    return volume;
  });


#if FROM_CHARS_DOUBLE_SUPPORTED
  pretty_print(lines, "std::to_chars", [](const std::vector<double> &lines) {
    double volume = 0;
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
        "h,help", "Print usage.");
    auto result = options.parse(argc, argv);
    if (result["help"].as<bool>()) {
      std::cout << options.help() << std::endl;
      return EXIT_SUCCESS;
    }
    auto filename = result["file"].as<std::string>();
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
