#include <fmt/format.h>

#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string_view>
#include <charconv>
#include <vector>

#include "algorithms.h"
#include "cxxopts.hpp"
#include "floatutils.h"

void run_exhaustive32(bool errol, const std::vector<std::string>& algo_filter = {}) {
  fmt::println("{:20} {:20}", "Algorithm", "Valid shortest serialization");

  std::array<Benchmarks::BenchArgs<float>, Benchmarks::COUNT> args;
  args = Benchmarks::initArgs<float>(errol);

  for (const auto &algo : args) {
    if (!algo.used) {
      fmt::print("# skipping {}\n", algo.name);
      continue;
    }
    if (algo.func == Benchmarks::dragonbox<float>) {
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
      // Reference output, we cannot use std::to_chars here, because it produces
      // the shortest representation, which is not necessarily the same as the
      // representation using the fewest significant digits.
      // So we use dragonbox, which serves as the reference implementation.
      const size_t vRef = Benchmarks::dragonbox(d, bufRef);
      const size_t vAlgo = algo.func(d, bufAlgo);

      std::string_view svRef{bufRef.data(), vRef};
      std::string_view svAlgo{bufAlgo.data(), vAlgo};

      auto countRef = count_significant_digits(svRef);
      auto countAlgo = count_significant_digits(svAlgo);
      auto backRef = parse_float<float>(svRef);
      auto backAlgo = parse_float<float>(svAlgo);
      if(!backRef || !backAlgo) {
        incorrect = true;
        fmt::print(" parse error: d = {}, bufRef = {}, bufAlgo = {}",
                   float_to_hex<float>(d), svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if(*backRef != d || *backAlgo != d) {
        fmt::println("\n# Error: parsing the output with std::from_chars does not bring back the input.");
      }
      if(*backRef != d) {
        incorrect = true;
        fmt::print(" ref mismatch: d = {}, backRef = {}; svRef = {}, svAlgo = {}",
                   float_to_hex<float>(d), *backRef, svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if(*backAlgo != d) {
        incorrect = true;
        fmt::print(" algo mismatch: d = {}, backAlgo = {}; svRef = {}, svAlgo = {}, "
                   "parsing the output with std::from_chars does not recover the original",
                   float_to_hex<float>(d), *backAlgo, svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if (countRef != countAlgo) {
        incorrect = true;
        fmt::print(" mismatch: d = {}, bufRef = {}, bufAlgo = {}",
                   float_to_hex<float>(d), svRef, svAlgo);
        fflush(stdout);
        break;
      }
    }
    fmt::print("\n");
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
        cxxopts::value<bool>()->default_value("false"))(
        "a,algorithm",
        "Specify which algorithm(s) to test (comma-separated).",
        cxxopts::value<std::vector<std::string>>()->default_value({}))(
        "h,help",
        "Print usage.");
    const auto result = options.parse(argc, argv);

    if (result["help"].as<bool>()) {
      fmt::print("{}\n", options.help());
      return EXIT_SUCCESS;
    }

    auto algo_filter = result["algorithm"].as<std::vector<std::string>>();
    run_exhaustive32(result["errol"].as<bool>(), algo_filter);
  } catch (const std::exception &e) {
    fmt::print("error parsing options: {}\n", e.what());
    return EXIT_FAILURE;
  }
}
