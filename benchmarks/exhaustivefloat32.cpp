
#include "algorithms.h"
#include "cxxopts.hpp"
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fmt/format.h>
#include <iostream>
#include <string_view>

size_t count_significant_digits(std::string_view num_str) {
  size_t count = 0;
  bool has_decimal = false;
  bool in_exponent = false;
  bool leading_zero = true;

  for (char c : num_str) {
    if (c == '.') {
      has_decimal = true;
      continue;
    }
    if (c == 'e' || c == 'E') {
      in_exponent = true;
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
      if (!in_exponent) {
        if (leading_zero && c == '0') {
          // Skip leading zeros before decimal
          continue;
        }
        leading_zero = false;
        count++;
      }
    }
  }

  // Special case: "X.0" should count as 1 digit
  if (has_decimal && count > 1) {
    auto last_digit_pos = num_str.find_last_not_of("0eE+-");
    if (last_digit_pos != std::string_view::npos &&
        num_str[last_digit_pos] == '.' && count == 2) {
      return 1;
    }
  }

  return count;
}

std::string float_to_hex(float f) {
    if (std::isnan(f) || std::isinf(f)) {
        return fmt::format("{}", f); // Handle special cases
    }

    uint32_t bits = std::bit_cast<uint32_t>(f);
    int exponent;
    float mantissa = std::frexp(f, &exponent); // Get mantissa and exponent
    uint32_t mantissa_bits = bits & 0x7FFFFF;  // 23-bit mantissa
    int exp_bits = (bits >> 23) & 0xFF;        // 8-bit exponent
    bool sign = bits >> 31;                    // Sign bit

    // Adjust for IEEE 754 representation
    if (exp_bits == 0 && mantissa_bits == 0) {
        return "0x0p+0"; // Zero case
    }

    // Convert to hex format
    return fmt::format("0x1.{:06x}p{:+d}", mantissa_bits, exponent - 23);
}
void run_exhaustive32(bool errol) {
  constexpr auto precision = std::numeric_limits<float>::digits10;
  fmt::println("{:20} {:20}", "Algorithm", "Valid shortest serialization");

  std::array<Benchmarks::BenchArgs<float>, Benchmarks::COUNT> args;
  args = Benchmarks::initArgs<float>(errol);

  for (const auto &algo : args) {
    if (!algo.used) {
      std::cout << "# skipping " << algo.name << std::endl;
      continue;
    }
    bool incorrect = false;
    char buf1[100], buf2[100];
    std::span<char> bufRef(buf1, sizeof(buf1)), bufAlgo(buf2, sizeof(buf2));
    fmt::print("# processing {}", algo.name);
    fflush(stdout);
    for (uint64_t i = 0; i < (1ULL << 32); ++i) {
      if (i % 0x2000000 == 0) {
        printf(".");
        fflush(stdout);
      }
      static_assert(sizeof(float) == sizeof(uint32_t));
      uint32_t i32(i);
      float d;
      std::memcpy(&d, &i32, sizeof(float));
      if (std::isnan(d) || std::isinf(d))
        continue;
      // Reference output
      const size_t vRef = Benchmarks::std_to_chars(d, bufRef);
      const size_t vAlgo = algo.func(d, bufAlgo);

      std::string_view svRef{bufRef.data(), vRef};
      std::string_view svAlgo{bufAlgo.data(), vAlgo};

      auto countRef = count_significant_digits(svRef);
      auto countAlgo = count_significant_digits(svAlgo);
      if (countRef != countAlgo) {
        incorrect = true;
        fmt::print(" mismatch: d = {}, bufRef = {}, bufAlgo = {}", float_to_hex(d),
                   svRef, svAlgo);
        fflush(stdout);
        break;
      }
    }
    printf("\n");
    fmt::println("{:20} {:20}", algo.name, incorrect == 0 ? "yes" : "no");
  }
}

cxxopts::Options
    options("exhaustivefloat32",
            "Verify serialization of all possible float32 values.");

int main(int argc, char **argv) {
  try {
    options.add_options()(
        "e,errol",
        "Enable errol3 (current impl. returns invalid values, e.g., for 0).",
        cxxopts::value<bool>()->default_value("false"))("h,help",
                                                        "Print usage.");
    const auto result = options.parse(argc, argv);

    if (result["help"].as<bool>()) {
      std::cout << options.help() << std::endl;
      return EXIT_SUCCESS;
    }
    run_exhaustive32(result["errol"].as<bool>());
  } catch (const std::exception &e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
