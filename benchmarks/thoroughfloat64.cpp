#include <fmt/format.h>

#include <cctype>
#include <string>
#include <fstream>
#include <vector>

#include "algorithms.h"
#include "cxxopts.hpp"
#include "floatutils.h"
#include "benchutil.h"

// Helper function to load doubles from a file
std::vector<TestCase<double>> load_doubles_from_file(const std::string& filename) {
  std::vector<TestCase<double>> numbers;
  std::ifstream file(filename);

  if (!file.is_open()) {
    fmt::println("Error: Could not open file {}", filename);
    return numbers;
  }

  for (std::string line; std::getline(file, line);) {
    if (auto num = parse_float<double>(line))
      numbers.emplace_back(*num, line);
    else
      fmt::println("Warning: Could not parse '{}' as double, skipping", line);
  }

  file.close();
  return numbers;
}

void run_file_test(const std::string& filename, bool errol, const std::vector<std::string>& algo_filter = {}) {
  const auto test_values = load_doubles_from_file(filename);
  if (test_values.empty()) {
    fmt::println("No valid numbers to test");
    return;
  }

  evaluate_properties_helper<double>(test_values, algo_filter, errol);
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
      fmt::println("{}", options.help());
      return EXIT_SUCCESS;
    }
    run_file_test(result["file"].as<std::string>(), result["errol"].as<bool>(), result["algorithm"].as<std::vector<std::string>>());
  } catch (const std::exception &e) {
    fmt::println("error parsing options: {}", e.what());
    return EXIT_FAILURE;
  }
}
