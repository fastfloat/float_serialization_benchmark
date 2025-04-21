#include <fmt/format.h>

#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <charconv>
#include <fstream>
#include <vector>

#include "algorithms.h"
#include "cxxopts.hpp"

size_t count_significant_digits(std::string_view num_str) {
  size_t count = 0;
  size_t trailing_zeros = 0;
  bool leading_zero = true;

  for (char c : num_str) {
    if (c == '.')
      continue;
    if (c == 'e' || c == 'E')
      break; // Stop counting at exponent
    if (std::isdigit(static_cast<unsigned char>(c))) {
      if (c == '0') {
        if (!leading_zero)
          trailing_zeros++;
        continue;
      }
      leading_zero = false;
      count += trailing_zeros + 1;
      trailing_zeros = 0;
    }
  }

  return count;
}

std::string double_to_hex(double d) {
  std::ostringstream oss;
  oss << std::hexfloat << d;
  return oss.str();
}

std::optional<double> parse_double(std::string_view sv) {
  double result;
  const char* begin = sv.data();
  const char* end = sv.data() + sv.size();

  auto [ptr, ec] = std::from_chars(begin, end, result);

  // Check if parsing succeeded and consumed the entire string
  if (ec == std::errc{} && ptr == end) {
      return result;
  }

  // Return nullopt if parsing failed or didn't consume all input
  return std::nullopt;
}

struct test_case {
  double value;
  std::string str_value;
};

// Helper function to load doubles from a file
std::vector<test_case> load_doubles_from_file(const std::string& filename) {
  std::vector<test_case> numbers;
  std::ifstream file(filename);
  std::string line;

  if (!file.is_open()) {
    fmt::print("Error: Could not open file {}\n", filename);
    return numbers;
  }

  while (std::getline(file, line)) {
    if (auto num = parse_double(line)) {
      numbers.emplace_back(*num,line);
    } else {
      fmt::print("Warning: Could not parse '{}' as double, skipping\n", line);
    }
  }

  file.close();
  return numbers;
}

void run_file_test(const std::string& filename, bool errol, const std::vector<std::string>& algo_filter = {}) {
  constexpr auto precision = std::numeric_limits<double>::digits10;
  fmt::println("{:20} {:20}", "Algorithm", "Valid shortest serialization");

  std::array<Benchmarks::BenchArgs<double>, Benchmarks::COUNT> args;
  args = Benchmarks::initArgs<double>(errol);

  // Load the doubles from file
  auto test_values = load_doubles_from_file(filename);
  if (test_values.empty()) {
    fmt::print("No valid numbers to test\n");
    return;
  }

  for (const auto &algo : args) {
    if (!algo.used) {
      fmt::print("# skipping {}\n", algo.name);
      continue;
    }
    if (algo.func == Benchmarks::dragonbox<double>) {
      fmt::print("# skipping {} because it is the reference.\n", algo.name);
      continue;
    }

    // Apply filter if provided
    if (!algo_filter.empty()) {
      bool matched = false;
      for (const auto &f : algo_filter) {
        if (algo.name.find(f) != std::string::npos) {
          matched = true;
          break;
        }
      }
      if (!matched) {
        fmt::print("# filtered out {}\n", algo.name);
        continue;
      }
    }

    bool incorrect = false;
    char buf1[100], buf2[100];
    std::span<char> bufRef(buf1, sizeof(buf1)), bufAlgo(buf2, sizeof(buf2));
    fmt::print("# processing {}", algo.name);
    fflush(stdout);

    size_t total = test_values.size();
    for (size_t i = 0; i < total; ++i) {
      if (i % (total/10) == 0 && total > 10) {
        printf(".");
        fflush(stdout);
      }
      double d = test_values[i].value;
      const std::string& str_value = test_values[i].str_value;
      if (std::isnan(d) || std::isinf(d))
        continue;

      const size_t vRef = Benchmarks::dragonbox(d, bufRef);
      const size_t vAlgo = algo.func(d, bufAlgo);

      std::string_view svRef{bufRef.data(), vRef};
      std::string_view svAlgo{bufAlgo.data(), vAlgo};
      //fmt::print(" RESULT {}: {} ", algo.name, svAlgo);

      auto countRef = count_significant_digits(svRef);
      auto countAlgo = count_significant_digits(svAlgo);
      auto backRef = parse_double(svRef);
      auto backAlgo = parse_double(svAlgo);

      if(!backRef || !backAlgo) {
        incorrect = true;
        fmt::print(" parse error: case: {}; d = {}, bufRef = {}, bufAlgo = {}", str_value, double_to_hex(d),
                   svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if(*backRef != d || *backAlgo != d) {
        fmt::println("\n# Error: parsing the output with std::from_chars does not bring back the input.");
      }
      if(*backRef != d) {
        incorrect = true;
        fmt::print(" ref mismatch:case: {};  d = {}, backRef = {}; svRef = {}, svAlgo = {}", str_value, double_to_hex(d), *backRef, svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if(*backAlgo != d) {
        incorrect = true;
        fmt::print(" algo mismatch: case: {}; d = {}, backAlgo = {}; svRef = {}, svAlgo = {}, parsing the output with std::from_chars does not recover the original", str_value, double_to_hex(d), *backAlgo, svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if (countRef != countAlgo) {
        incorrect = true;
        fmt::print(" mismatch: case: {}; d = {}, bufRef = {}, bufAlgo = {}", str_value, double_to_hex(d),
                   svRef, svAlgo);
        fflush(stdout);
        break;
      }
    }
    fmt::print("\n");
    fmt::println("{:20} {:20}", algo.name, incorrect == 0 ? "yes" : "no");
  }
}

cxxopts::Options
    options("exhaustivedouble",
            "Verify serialization of double values from a file.");

int main(int argc, char **argv) {
  try {
    options.add_options()
        ("e,errol",
         "Enable errol3 (current impl. returns invalid values, e.g., for 0).",
         cxxopts::value<bool>()->default_value("false"))
        ("f,file",
         "Input file containing doubles (one per line)",
         cxxopts::value<std::string>()->default_value(THOROUGH_DATA_FILE))
        ("a,algorithm",
         "Filter algorithms to test (comma-separated)",
         cxxopts::value<std::vector<std::string>>()->default_value(""))
        ("h,help",
         "Print usage.");
    const auto result = options.parse(argc, argv);

    if (result["help"].as<bool>()) {
      fmt::print("{}\n", options.help());
      return EXIT_SUCCESS;
    }
    run_file_test(result["file"].as<std::string>(), result["errol"].as<bool>(), result["algorithm"].as<std::vector<std::string>>());
  } catch (const std::exception &e) {
    fmt::print("error parsing options: {}\n", e.what());
    return EXIT_FAILURE;
  }
}