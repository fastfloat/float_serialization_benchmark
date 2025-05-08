/**
 * Currently, we benchmark the speed of converting floating-point numbers to
 * strings. We compare the speed of different libraries and methods. The
 * benchmark is run on a file containing one floating-point number per line or
 * synthetic data. The synthetic data is generated using a random number
 * generator. We measure the speed of the conversion by computing the volume of
 * data generated.
 */

#include "algorithms.h"
#define IEEE_8087
#include "benchutil.h"
#include "cxxopts.hpp"
#include "random_generators.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <fast_float/fast_float.h>
#include <fmt/core.h>

using Benchmarks::BenchArgs;

template <arithmetic_float T>
void evaluateProperties(const std::vector<TestCase<T>> &lines,
                        const std::array<BenchArgs<T>, Benchmarks::COUNT> &args,
                        const std::vector<std::string> &algo_filter) {
  evaluate_properties_helper<T>(lines, algo_filter, args);
}

struct diy_float_t {
    diy_float_t(uint64_t significand, int exponent, bool is_negative)
      : significand(significand), exponent(exponent), is_negative(is_negative) {}
		uint64_t	      significand;
		int							exponent;
		bool						is_negative;
};

template <arithmetic_float T>
void process(const std::vector<TestCase<T>> &lines,
             const std::array<BenchArgs<T>, Benchmarks::COUNT> &args,
             const std::vector<std::string> &algo_filter) {
  // We have a special algorithm for the string generation:
  if (!algo_filtered_out("just_string", algo_filter)) {
    std::vector<diy_float_t> parsed;
    for(const auto d : lines) {
      const auto v = jkj::grisu_exact(d.value);
      parsed.emplace_back(v.significand, v.exponent, v.is_negative);
    }
    pretty_print(parsed, "just_string_ours", [](const std::vector<diy_float_t>& parsed) -> int {
      int volume = 0;
      char buf[100];
      std::span<char> bufspan(buf, sizeof(buf));
      for (const auto v : parsed)
        volume += to_chars(v.significand, v.exponent, v.is_negative, bufspan.data());
      return volume;
    }, 100);
    pretty_print(parsed, "just_string_dragonbox", [](const std::vector<diy_float_t>& parsed) -> int {
      using traits = jkj::dragonbox::default_float_traits<T>;
      using carrier_uint = typename traits::carrier_uint;
      int volume = 0;
      char buf[100];
      std::span<char> bufspan(buf, sizeof(buf));
      for (const auto v : parsed) {
        char* ptr = bufspan.data();
        if(v.is_negative) *ptr++ = '-';
        const char* end = jkj::dragonbox::to_chars_detail::to_chars<T, traits>(
            static_cast<carrier_uint>(v.significand), v.exponent, ptr);
        volume += end - bufspan.data();
      }
      return volume;
    }, 100);
  } else {
    fmt::println("# skipping just_string");
  }

  for (const auto &algo : args) {
    if (!algo.used) {
      fmt::println("# skipping {}", algo.name);
      continue;
    }
    if (algo_filtered_out(algo.name, algo_filter)) {
      fmt::println("# filtered out {}", algo.name);
      continue;
    }

    pretty_print(lines, algo.name, [&algo](const std::vector<TestCase<T>> &lines) -> int {
      int volume = 0;
      char buf[100];
      std::span<char> bufspan(buf, sizeof(buf));
      for (const auto d : lines)
        volume += algo.func(d.value, bufspan);
      return volume;
    }, algo.testRepeat);
  }
}

template <arithmetic_float T>
std::vector<TestCase<T>> fileload(const std::string &filename) {
  std::ifstream inputfile(filename);
  if (!inputfile) {
    fmt::println(stderr, "can't open {}", filename);
    return {};
  }

  std::vector<TestCase<T>> lines;
  lines.reserve(10000); // let us reserve plenty of memory.
  for (std::string line; getline(inputfile, line);) {
    try {
      lines.emplace_back(std::is_same_v<T, float> ? std::stof(line) : std::stod(line), line);
    } catch (...) {
      fmt::println(stderr, "problem with {}\nWe expect floating-point numbers (one per line).", line);
      std::abort();
    }
  }
  fmt::println("# read {} lines", lines.size());
  return lines;
}

template <arithmetic_float T>
std::vector<TestCase<T>> get_random_numbers(size_t howmany,
                                            const std::string &random_model) {
  fmt::println("# parsing random numbers");
  std::vector<TestCase<T>> lines;
  auto g = get_generator_by_name<T>(random_model);
  fmt::println("model: {}\nvolume: {} floats", g->describe(), howmany);
  lines.reserve(howmany); // let us reserve plenty of memory.
  for (size_t i = 0; i < howmany; i++) {
    const T line = g->new_float();
    lines.emplace_back(line, std::nullopt);
  }
  return lines;
}

cxxopts::Options
    options("benchmark",
            "Compute the parsing speed of different number parsers.");

int main(int argc, char **argv) {
  try {
    options.add_options()
        ("f,file", "File name.",
        cxxopts::value<std::string>()->default_value(""))
        ("v,volume", "Volume (number of floats generated).",
        cxxopts::value<size_t>()->default_value("100000"))
        ("m,model", "Random Model.",
        cxxopts::value<std::string>()->default_value("uniform"))
        ("s,single", "Use single precision instead of double.",
        cxxopts::value<bool>()->default_value("false"))
        ("t,test", "Test the algorithms and find their properties.",
        cxxopts::value<bool>()->default_value("false"))
        ("e,errol", "Enable errol3 (current impl. returns invalid values, e.g., for 0).",
        cxxopts::value<bool>()->default_value("false"))
        ("a,algo-filter", "Filter algorithms by name substring: you can use multiple filters separated by commas.",
        cxxopts::value<std::vector<std::string>>())
        ("r,repeat", "Force a number of repetitions.",
        cxxopts::value<size_t>()->default_value("0"))
        ("h,help", "Print usage.");
    const auto result = options.parse(argc, argv);

    if (result["help"].as<bool>()) {
      fmt::print("{}\n", options.help());
      return EXIT_SUCCESS;
    }
    const size_t repeat = result["repeat"].as<size_t>();
    const bool single = result["single"].as<bool>();
    const auto filter = result.count("algo-filter")
                      ? result["algo-filter"].as<std::vector<std::string>>()
                      : std::vector<std::string>{};
    fmt::println("number type: binary{}", (single ? "32 (float)" : "64 (double)"));

    std::variant<std::vector<TestCase<float>>,
                 std::vector<TestCase<double>>> numbers;
    const auto filename = result["file"].as<std::string>();
    if (filename.empty()) {
      const auto volume = result["volume"].as<size_t>();
      const auto model = result["model"].as<std::string>();
      if (single)
        numbers = get_random_numbers<float>(volume, model);
      else
        numbers = get_random_numbers<double>(volume, model);
      fmt::println("# You can also provide a filename (with the -f flag): "
                   "it should contain one string per line corresponding to a number");
    }
    else {
      if (single)
        numbers = fileload<float>(filename);
      else
        numbers = fileload<double>(filename);
    }

    std::variant<std::array<BenchArgs<float>, Benchmarks::COUNT>,
                 std::array<BenchArgs<double>, Benchmarks::COUNT>> algorithms;
    const bool errol = result["errol"].as<bool>();
    if (single)
      algorithms = Benchmarks::initArgs<float>(errol);
    else
      algorithms = Benchmarks::initArgs<double>(errol);

    if(repeat > 0) {
      fmt::println("# forcing repeat count to {}", repeat);
      std::visit([repeat](auto &args) {
        for (auto &arg : args)
          arg.testRepeat = repeat;
      }, algorithms);
    }

    const bool test = result["test"].as<bool>();
    std::visit([test, &filter](const auto &lines, const auto &args) {
      using T1 = typename std::decay_t<decltype(lines)>::value_type::Type;
      using T2 = typename std::decay_t<decltype(args)>::value_type::Type;
      if constexpr (std::is_same_v<T1, T2>) {
        if (test)
          evaluateProperties(lines, args, filter);
        else
          process(lines, args, filter);
      }
    }, numbers, algorithms);
  } catch (const std::exception &e) {
    fmt::println("Error parsing options: {}", e.what());
    fmt::println("\nUSAGE GUIDE:");
    fmt::println("  ./benchmark [OPTIONS]");
    fmt::println("\nCOMMAND SUMMARY:");
    fmt::println("  The benchmark tool evaluates the performance of different floating-point to string");
    fmt::println("  conversion algorithms. It can use either synthetic data or a file containing");
    fmt::println("  floating-point numbers (one per line).");
    fmt::println("\nEXAMPLES:");
    fmt::println("  ./benchmark --single                    # Run benchmark with single precision (float)");
    fmt::println("  ./benchmark --file=data/canada.txt      # Run benchmark using numbers from a file");
    fmt::println("  ./benchmark --test                      # Test correctness instead of performance");
    fmt::println("  ./benchmark --volume=1000 --model=uniform # Generate 1000 uniform random numbers");
    fmt::println("  ./benchmark --algo-filter=ryu,grisu     # Only test algorithms containing 'ryu' or 'grisu'");
    fmt::println("\nFor full options list, run: ./benchmark --help");
    return EXIT_FAILURE;
  }
}
