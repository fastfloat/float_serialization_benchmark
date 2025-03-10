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

using Benchmarks::arithmetic_float;
using Benchmarks::BenchArgs;

template <arithmetic_float T>
void evaluateProperties(const std::vector<T> &lines,
                        const std::array<BenchArgs<T>, Benchmarks::COUNT> &args) {
  constexpr auto precision = std::numeric_limits<T>::digits10;
  fmt::println("{:20} {:20}", "Algorithm", "Valid round-trip");

  for (const auto &algo : args) {
    if (!algo.used) {
      std::cout << "# skipping " << algo.name << std::endl;
      continue;
    }

    char buf1[100], buf2[100];
    std::span<char> bufRef(buf1, sizeof(buf1)), bufAlgo(buf2, sizeof(buf2));
    int incorrect = 0;
    for (const auto d : lines) {
      // Reference output
      const int vRef = Benchmarks::std_to_chars(d, bufRef);
      bufRef[vRef] = '\0';
      T dRef;
      auto [ptr, ec] = std::from_chars(bufRef.data(), bufRef.data() + vRef, dRef);
      assert(ptr == bufRef.data() + vRef);
      assert(ec == std::errc());
      assert(d == dRef);

      // Tested algorithm output
      const int vAlgo = algo.func(d, bufAlgo);
      bufAlgo[vAlgo] = '\0';
      T dAlgo;
      auto [ptrAlgo, ecAlgo] = std::from_chars(bufAlgo.data(), bufAlgo.data() + vAlgo, dAlgo);
      assert(ptrAlgo == bufAlgo.data() + vAlgo);
      assert(ecAlgo == std::errc());
      if ((incorrect += (d != dAlgo)) == 1)
        fmt::println("\t{:20} mismatch: d = {:.17f}, bufRef = {}, bufAlgo = {}, dAlgo = {:.17f}",
                     algo.name, d, bufRef.data(), bufAlgo.data(), dAlgo);
    }
    fmt::println("{:20} {:20}", algo.name, incorrect == 0 ? "yes" : "no");
  }
}

template <arithmetic_float T>
void process(const std::vector<T> &lines,
             const std::array<BenchArgs<T>, Benchmarks::COUNT> &args) {
  for (const auto &algo : args) {
    if (!algo.used) {
      std::cout << "# skipping " << algo.name << std::endl;
      continue;
    }

    pretty_print(lines, algo.name, [&algo](const std::vector<T> &lines) -> int {
      int volume = 0;
      char buf[100];
      std::span<char> bufspan(buf, sizeof(buf));
      for (const auto d : lines)
        volume += algo.func(d, bufspan);
      return volume;
    }, algo.testRepeat);
  }
}

template <typename T>
std::vector<T> fileload(const std::string &filename) {
  std::ifstream inputfile(filename);
  if (!inputfile) {
    std::cerr << "can't open " << filename << std::endl;
    return {};
  }

  std::vector<T> lines;
  lines.reserve(10000); // let us reserve plenty of memory.
  for (std::string line; getline(inputfile, line);) {
    try {
      lines.push_back(std::is_same_v<T, float> ? std::stof(line)
                                               : std::stod(line));
    } catch (...) {
      std::cerr << "problem with " << line << "\n"
                << "We expect floating-point numbers (one per line)."
                << std::endl;
      std::abort();
    }
  }
  std::cout << "# read " << lines.size() << " lines " << std::endl;
  return lines;
}

template <typename T>
std::vector<T> get_random_numbers(size_t howmany,
                                  const std::string &random_model) {
  std::cout << "# parsing random numbers" << std::endl;
  std::vector<T> lines;
  auto g = get_generator_by_name<T>(random_model);
  std::cout << "model: " << g->describe() << "\n"
            << "volume: " << howmany << " floats" << std::endl;
  lines.reserve(howmany); // let us reserve plenty of memory.
  for (size_t i = 0; i < howmany; i++) {
    const T line = g->new_float();
    lines.push_back(line);
  }
  return lines;
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
        "t,test", "Test the algorithms and find their properties.",
        cxxopts::value<bool>()->default_value("false"))(
        "d,dragon", "Enable dragon4 (current impl. triggers some asserts).",
        cxxopts::value<bool>()->default_value("false"))(
        "e,errol", "Enable errol3 (current impl. returns invalid values, e.g., for 0).",
        cxxopts::value<bool>()->default_value("false"))(
        "h,help", "Print usage.");
    const auto result = options.parse(argc, argv);

    if (result["help"].as<bool>()) {
      std::cout << options.help() << std::endl;
      return EXIT_SUCCESS;
    }

    const bool single = result["single"].as<bool>();
    std::cout << "number type: binary"
              << (single ? "32 (float)" : "64 (double)") << std::endl;

    std::variant<std::vector<float>, std::vector<double>> numbers;
    const auto filename = result["file"].as<std::string>();
    if (filename.empty()) {
      const auto volume = result["volume"].as<size_t>();
      const auto model = result["model"].as<std::string>();
      if (single)
        numbers = get_random_numbers<float>(volume, model);
      else
        numbers = get_random_numbers<double>(volume, model);
      std::cout << "# You can also provide a filename (with the -f flag): "
                   "it should contain one string per line corresponding to a number"
                << std::endl;
    }
    else {
      if (single)
        numbers = fileload<float>(filename);
      else
        numbers = fileload<double>(filename);
    }

    auto initArgs = [&](auto type) {
      using T = decltype(type);
      std::array<BenchArgs<T>, Benchmarks::COUNT> args;
      args[Benchmarks::DRAGON4]           = { "dragon4"           , Benchmarks::dragon4<T>           , result["dragon"].as<bool>()   , 10 };
      args[Benchmarks::ERROL3]            = { "errol3"            , Benchmarks::errol3<T>            , result["errol"].as<bool>() };
      args[Benchmarks::TO_STRING]         = { "std::to_string"    , Benchmarks::to_string<T>         , ERROL_SUPPORTED };
      args[Benchmarks::FMT_FORMAT]        = { "fmt::format"       , Benchmarks::fmt_format<T>        , true };
      args[Benchmarks::NETLIB]            = { "netlib"            , Benchmarks::netlib<T>            , NETLIB_SUPPORTED              , 10 };
      args[Benchmarks::SNPRINTF]          = { "snprintf"          , Benchmarks::snprintf<T>          , true };
      args[Benchmarks::GRISU2]            = { "grisu2"            , Benchmarks::grisu2<T>            , true };
      args[Benchmarks::GRISU_EXACT]       = { "grisu_exact"       , Benchmarks::grisu_exact<T>       , true };
      args[Benchmarks::SCHUBFACH]         = { "schubfach"         , Benchmarks::schubfach<T>         , true };
      args[Benchmarks::DRAGONBOX]         = { "dragonbox"         , Benchmarks::dragonbox<T>         , true };
      args[Benchmarks::RYU]               = { "ryu"               , Benchmarks::ryu<T>               , true };
      args[Benchmarks::TEJU_JAGUA]        = { "teju_jagua"        , Benchmarks::teju_jagua<T>        , true };
      args[Benchmarks::DOUBLE_CONVERSION] = { "double_conversion" , Benchmarks::double_conversion<T> , true };
      args[Benchmarks::ABSEIL]            = { "abseil"            , Benchmarks::abseil<T>            , ABSEIL_SUPPORTED };
      args[Benchmarks::STD_TO_CHARS]      = { "std::to_chars"     , Benchmarks::std_to_chars<T>      , FROM_CHARS_SUPPORTED };
      return args;
    };

    std::variant<std::array<BenchArgs<float>, Benchmarks::COUNT>,
                 std::array<BenchArgs<double>, Benchmarks::COUNT>> algorithms;
    if (single)
      algorithms = initArgs(float{});
    else
      algorithms = initArgs(double{});

    const bool test = result["test"].as<bool>();
    std::visit([test](const auto &lines, const auto &args) {
      using T1 = typename std::decay_t<decltype(lines)>::value_type;
      using T2 = typename std::decay_t<decltype(args)>::value_type::Type;
      if constexpr (std::is_same_v<T1, T2>) {
        if (test)
          evaluateProperties(lines, args);
        else
          process(lines, args);
      }
    }, numbers, algorithms);
  } catch (const std::exception &e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
