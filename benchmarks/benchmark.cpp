/**
 * Currently, we benchmark the speed of converting floating-point numbers to
 * strings. We compare the speed of different libraries and methods. The
 * benchmark is run on a file containing one floating-point number per line or
 * synthetic data. The synthetic data is generated using a random number
 * generator. We measure the speed of the conversion by computing the volume of
 * data generated.
 */

 #include "champagne_lemire/champagne_lemire.h"
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

cxxopts::Options
    options("benchmark",
            "Compute the parsing speed of different number parsers.");

template <arithmetic_float T>
void evaluateProperties(const std::vector<TestCase<T>> &lines,
                        const std::vector<BenchArgs<T>> &args,
                        const std::vector<std::string> &algo_filter) {
  evaluate_properties_helper<T>(lines, algo_filter, args);
}

struct diy_float_t {
  diy_float_t(uint64_t significand, int exponent, bool is_negative)
    : significand(significand), exponent(exponent), is_negative(is_negative) {}
  uint64_t significand;
  int exponent;
  bool is_negative;
};

template <arithmetic_float T>
void process(const std::vector<TestCase<T>> &lines,
             const std::vector<BenchArgs<T>> &args,
             const std::vector<std::string> &algo_filter,
             bool string_eval) {
  // We have a special algorithm for the string generation:
  if (string_eval && !algo_filtered_out("just_string", algo_filter)) {
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
    T value;
    auto [ptr, ec] = std::from_chars(line.data(), line.data() + line.size(), value);
    if (ec != std::errc()) {
      fmt::println(stderr, "problem with {}\nWe expect floating-point numbers (one per line).", line);
      std::abort();
    }
    lines.emplace_back(value, line);
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

// Checks if a floating-point number is exactly representable as the specified integer type
template <std::integral int_type, std::floating_point float_type>
bool is_exact_integer(float_type x) {
  if (!std::isfinite(x)) {
    return false;
  }
  int_type i = static_cast<int_type>(x);
  return static_cast<float_type>(i) == x;
}

// New template version of describe
template <typename T>
void describe(const std::variant<std::vector<TestCase<float>>,
              std::vector<TestCase<double>>> &numbers,
              std::vector<BenchArgs<T>> args,
              const std::vector<std::string> &algo_filter) {
  if constexpr (std::is_same_v<T, float>) {
    args.push_back(get_std_to_chars_shorter<float>());
  } else if constexpr (std::is_same_v<T, double>) {
    args.push_back(get_std_to_chars_shorter<double>());
  }

  std::visit([&args, &algo_filter](const auto &lines) {
    size_t integers64 = 0;
    size_t integers32 = 0;
    for (const auto &d : lines) {
      integers64 += is_exact_integer<int64_t>(d.value) ? 1 : 0;
      integers32 += is_exact_integer<int32_t>(d.value) ? 1 : 0;
    }
    std::vector<size_t> sizes(lines.size(), std::numeric_limits<size_t>::max());
    std::vector<std::string> shortest(lines.size());
    std::vector<size_t> min_digits(lines.size(), std::numeric_limits<size_t>::max());
    std::vector<std::string> min_digits_str(lines.size());
    std::vector<std::tuple<std::string, size_t, double, size_t, double, bool>> results;
    size_t min_size = std::numeric_limits<size_t>::max();
    for (const auto &algo : args) {
      if (!algo.used) continue;
      if (algo_filtered_out(algo.name, algo_filter)) continue;
      size_t total_size = 0;
      size_t total_digits = 0;

      std::vector<char> buffer(100);
      std::span<char> bufspan(buffer);
      bool precise = true;
      for(size_t i = 0; i < lines.size(); ++i) {
        const auto &d = lines[i];
        int len = algo.func(d.value, bufspan);
        if(sizes[i] > len) {
          sizes[i] = len;
          shortest[i].assign(bufspan.data(), len);
        }
        // Check for minimal number of significant digits
        size_t digits = count_significant_digits(std::string_view(bufspan.data(), len));
        if (min_digits[i] > digits) {
          min_digits[i] = digits;
          min_digits_str[i].assign(bufspan.data(), len);
        }
        total_size += len;
        total_digits += digits;
        std::string_view sv(buffer.data(), len);
        auto parsed = parse_float<T>(sv);
        if (!parsed.has_value() || parsed.value() != d.value) {
          precise = false;
          break;
        }
      }
      double avg = total_size / double(lines.size());
      double avg_digits = total_digits / double(lines.size());

      results.emplace_back(algo.name, total_size, avg, total_digits, avg_digits, precise);
      if (precise && total_size < min_size) min_size = total_size;
    }
    std::map<std::string, std::tuple<bool, bool>> algo_results;
    for (const auto &algo : args) {
      if (!algo.used) continue;
      if (algo_filtered_out(algo.name, algo_filter)) continue;
      size_t howmany = 0;
      size_t howmany_digits = 0;
      std::vector<char> buffer(100);
      std::span<char> bufspan(buffer);
      size_t worse_than_shortest = 0;
      size_t digits_worse_than_min = 0;
      for(size_t i = 0; i < lines.size(); ++i) {
        const auto &d = lines[i];
        int len = algo.func(d.value, bufspan);
        // Case where the string is longer than the shortest
        if(sizes[i] < len) {
          howmany++;
          bool new_record = (len > worse_than_shortest + sizes[i]);
          worse_than_shortest = (std::max)(worse_than_shortest, len - sizes[i]);
          if(new_record) {
            fmt::print(stderr, "Warning: algorithm {} produced a longer string ({}) than the shortest ({}) for value {}\n",
                       algo.name, len, sizes[i], d.value);
            fmt::print(stderr, "  Shortest: '{}'\n", shortest[i]);
            std::string_view this_answer(bufspan.data(), len);
            fmt::print(stderr, "  Produced: '{}'\n", this_answer);
            auto parsed_ref = parse_float<T>(shortest[i]);
            auto parsed_this = parse_float<T>(this_answer);
            if(!parsed_ref.has_value() || !parsed_this.has_value()) {
              fmt::print(stderr, "  BUG! Parsing failed for one of the strings.\n");
            } else if (parsed_ref.value() != parsed_this.value()) {
              fmt::print(stderr, "  BUG! Parsed values differ: {} vs {}\n",
                         parsed_ref.value(), parsed_this.value());
            }
          }
        }
        // Case where the string has more significant digits than the minimum
        size_t digits = count_significant_digits(std::string_view(bufspan.data(), len));
        if(min_digits[i] < digits) {
          howmany_digits++;
          bool new_record = (len > digits_worse_than_min + sizes[i]);
          digits_worse_than_min = (std::max)(digits_worse_than_min, digits - min_digits[i]);
          if(new_record) {
            fmt::print(stderr, "Warning: algorithm {} produced a string with more significant digits ({}) than the minimum ({}) for value {}\n",
                       algo.name, digits, min_digits[i], d.value);
            fmt::print(stderr, "  Min digits: '{}'\n", min_digits_str[i]);
            std::string_view this_answer(bufspan.data(), len);
            fmt::print(stderr, "  Produced: '{}'\n", this_answer);
          }
        }
      }
      if(howmany > 0) {
        fmt::print(stderr, "Warning: algorithm {} produced longer strings than the shortest for {} values, worst gap is {} characters\n",
                   algo.name, howmany, worse_than_shortest);
      }
      if(howmany_digits > 0) {
        fmt::print(stderr, "Warning: algorithm {} produced more significant digits than the minimum for {} values, worst gap is {} digits\n",
                   algo.name, howmany_digits, digits_worse_than_min);
      }
      if(howmany > 0 || howmany_digits > 0) {
        fmt::println("---");
      }
      algo_results[algo.name] = std::make_tuple(howmany == 0, howmany_digits == 0);

    }
    for (const auto &[name, total_size, avg, total_digits, avg_digits, precise] : results) {
      auto [is_shortest, is_min_digits_algo] = algo_results[name];
      fmt::print("{:<18} {:>12} ({:>5.3f} chars/f) {:>12} ({:>5.3f} d/f) {:<18} {:<12} {:<15}\n",
        name, total_size, avg, total_digits, avg_digits,
        is_shortest ? "[minimal string]" : "[non minimal]",
        precise ? "[precise]" : "[imprecise]",
        is_min_digits_algo ? "[min digits]" : "[non min digits]");
    }
    fmt::println("count: {}, 32-bit ints: {}, 64-bit ints: {}", lines.size(), integers32, integers64);
  }, numbers);
}

int main(int argc, char **argv) {
  try {
    options.add_options()
        ("f,file", "File name.",
         cxxopts::value<std::string>()->default_value(""))
        ("F,fixed", "Fixed-point representation.",
         cxxopts::value<size_t>()->default_value("0"))
        ("v,volume", "Volume (number of floats generated).",
         cxxopts::value<size_t>()->default_value("100000"))
        ("m,model", "Random Model.",
         cxxopts::value<std::string>()->default_value("uniform_01"))
        ("a,algo-filter", "Filter algorithms by name substring: you can use multiple filters separated by commas.",
         cxxopts::value<std::vector<std::string>>())
        ("r,repeat", "Force a number of repetitions.",
         cxxopts::value<size_t>()->default_value("0"))
        ("D,data", "Description of the data.")
        ("s,single", "Use single precision instead of double.")
        ("S,string-eval", "Evaluate perf. of string generation from decimal mantissa/exponent")
        ("t,test", "Test the algorithms and find their properties.")
        ("e,errol", "Enable errol3 (current impl. returns invalid values, e.g., for 0).")
        ("h,help", "Print usage.");
    const auto result = options.parse(argc, argv);

    if (result.count("help") > 0) {
      fmt::print("{}\n", options.help());
      return EXIT_SUCCESS;
    }
    const bool single = result.count("single") > 0;
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

    std::variant<std::vector<BenchArgs<float>>, std::vector<BenchArgs<double>>> algorithms;
    const bool errol = result.count("errol") > 0;
    const size_t repeat = result["repeat"].as<size_t>();
    const size_t fixed_size = result["fixed"].as<size_t>();
    if (single)
      algorithms = initArgs<float>(errol, repeat, fixed_size);
    else
      algorithms = initArgs<double>(errol, repeat, fixed_size);
    if (result.count("data") > 0) {
      if (single)
        describe<float>(numbers, std::get<std::vector<BenchArgs<float>>>(algorithms), filter);
      else
        describe<double>(numbers, std::get<std::vector<BenchArgs<double>>>(algorithms), filter);
      return EXIT_SUCCESS;
    }

    const bool test = result.count("test") > 0;
    const bool string_eval = result.count("string-eval") > 0;
    std::visit([test, string_eval, &filter](const auto &lines, const auto &args) {
      using T1 = typename std::decay_t<decltype(lines)>::value_type::Type;
      using T2 = typename std::decay_t<decltype(args)>::value_type::Type;
      if constexpr (std::is_same_v<T1, T2>) {
        if (test)
          evaluateProperties(lines, args, filter);
        else
          process(lines, args, filter, string_eval);
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
    fmt::println("  ./benchmark --fixed=10                  # Test fixed-point representation instead of shortest length");
    fmt::println("  ./benchmark --test                      # Test correctness instead of performance");
    fmt::println("  ./benchmark --volume=1000 --model=uniform_01 # Generate 1000 uniform random numbers in [0, 1]");
    fmt::println("  ./benchmark --algo-filter=ryu,grisu     # Only test algorithms containing 'ryu' or 'grisu'");
    fmt::println("\nFor full options list, run: ./benchmark --help");
    return EXIT_FAILURE;
  }
}
