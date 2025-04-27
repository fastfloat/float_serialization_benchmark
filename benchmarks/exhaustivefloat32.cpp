#include <fmt/format.h>

#include <cctype>
#include <vector>

#include "algorithms.h"
#include "cxxopts.hpp"
#include "floatutils.h"
#include "benchutil.h"

using Benchmarks::BenchArgs;

void run_exhaustive32(bool errol, const std::vector<std::string>& algo_filter = {}) {
  static_assert(sizeof(float) == sizeof(uint32_t));
  auto floats_view
    = std::views::iota(uint32_t{0})
    | std::views::take(1ULL << 32)
    | std::views::transform([](uint32_t i) {
        const float d = std::bit_cast<float>(i);
        return TestCase<float>{ d, std::nullopt };
      });

  evaluate_properties_helper<float>(floats_view, algo_filter, errol);
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
      fmt::println("{}", options.help());
      return EXIT_SUCCESS;
    }

    auto algo_filter = result["algorithm"].as<std::vector<std::string>>();
    run_exhaustive32(result["errol"].as<bool>(), algo_filter);
  } catch (const std::exception &e) {
    fmt::println("error parsing options: {}", e.what());
    return EXIT_FAILURE;
  }
}
