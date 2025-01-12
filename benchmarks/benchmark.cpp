/**
 * Currently, we benchmark the speed of converting floating-point numbers to
 * strings. We compare the speed of different libraries and methods. The
 * benchmark is run on a file containing one floating-point number per line or
 * synthetic data. The synthetic data is generated using a random number
 * generator. We measure the speed of the conversion by computing the volume of
 * data generated.
 */

#ifndef __CYGWIN__
#include "absl/strings/str_cat.h"
#endif

#include "dragonbox/dragonbox_to_chars.h"
#include "ryu/ryu.h"
#include "double-conversion/double-conversion.h"

#define IEEE_8087
#include "benchutil.h"
#include "cxxopts.hpp"

#if NETLIB_SUPPORTED
#include "gdtoa.h"
#endif

#include "grisu2.h"
#include "random_generators.h"

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
  size_t repeat = 100;
  pretty_print(lines, "std::to_string", [](std::vector<double> &lines) {
    double volume = 0;
    for (auto d : lines) {
      std::string s = std::to_string(d);
      volume += s.size();
    }
    return volume;
  });
  pretty_print(lines, "fmt::format", [](std::vector<double> &lines) {
    double volume = 0;
    for (auto d : lines) {
      std::string s = fmt::format("{}", d);
      volume += s.size();
    }
    return volume;
  });
#if NETLIB_SUPPORTED
  pretty_print(
      lines, "netlib",
      [](std::vector<double> &lines) {
        double volume = 0;
        char *result;
        int decpt, sign;
        char *rve;
        for (auto d : lines) {
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
      },
      10);
#else
  std::cout << "# netlib not supported" << std::endl;
#endif
  pretty_print(lines, "sprintf", [](std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (auto d : lines) {
      volume += snprintf(buffer, sizeof(buffer), "%g", d);
    }
    return volume;
  });
  pretty_print(lines, "grisu2", [](std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (auto d : lines) {
      char *newp = grisu2::to_chars(buffer, nullptr, d);
      volume += newp - buffer;
    }
    return volume;
  });
#if FROM_CHARS_DOUBLE_SUPPORTED
  pretty_print(lines, "std::to_chars", [](std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (auto d : lines) {
      auto [p, ec] = std::to_chars(buffer, buffer + sizeof(buffer), d);
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
  pretty_print(lines, "dragonbox", [](std::vector<double> &lines) {
    double volume = 0;
    char buffer[100];
    for (auto d : lines) {
      char *end_ptr = jkj::dragonbox::to_chars(d, buffer);
      volume += end_ptr - &buffer[0];
    }
    return volume;
  });

  pretty_print(lines, "double_conversion", [](std::vector<double> &lines) {
    double volume = 0;
    double_conversion::DoubleToStringConverter converter(
        double_conversion::DoubleToStringConverter::NO_FLAGS, "inf", "nan", 'e',
        -4, 6, 0, 0);
    const int kBufferSize = 100;
    char buffer[kBufferSize];
    double_conversion::StringBuilder builder(buffer, kBufferSize);

    for (auto d : lines) {
      builder.Reset();
      if (!converter.ToShortest(d, &builder)) {
        std::cerr << "problem with " << d << std::endl;
        std::abort();
      }
      volume += strlen(builder.Finalize());
    }
    return volume;
  });

  pretty_print(lines, "abseil", [](std::vector<double> &lines) {
    double volume = 0;
    std::string buffer;
    for (auto d : lines) {
      buffer.clear();
      absl::StrAppend(&buffer, d);
      volume += buffer.size();
    }
    return volume;
  });
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
