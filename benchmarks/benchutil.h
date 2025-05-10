#ifndef BENCHUTIL_H
#define BENCHUTIL_H

#include <fmt/format.h>

#include <atomic>
#include <cfloat>
#include <cstdio>
#include <ranges>
#include <type_traits>
#include <variant>

#include "algorithms.h"
#include "counters/event_counter.h"

event_collector collector;

bool algo_filtered_out(const std::string &algo_name,
                       const std::vector<std::string> &algo_filter) {
  if (algo_filter.empty())
    return false;

  for (const auto &f : algo_filter)
    if (algo_name.find(f) != std::string::npos)
      return false;

  return true;
}

template <arithmetic_float T>
struct TestCase {
  using Type = T;
  T value;
  std::optional<std::string> str_value;
};

template<typename E, typename T>
concept TestCaseConcept = arithmetic_float<T> && requires(E e) {
  { e.value } -> std::convertible_to<T>;
  { e.str_value } -> std::convertible_to<std::optional<std::string>>;
};

template<typename R, typename T>
concept TestCaseRange
    = std::ranges::input_range<R>
      && TestCaseConcept<std::ranges::range_reference_t<R>, T>;

template<arithmetic_float T, typename Range> requires TestCaseRange<Range, T>
void evaluate_properties_helper(Range&& cases,
                                const std::vector<std::string> &algo_filter,
                                std::variant<std::vector<BenchArgs<T>>, bool> argsOpt) {
  fmt::println("{:20} {:20}", "Algorithm", "Valid shortest serialization");
  const auto args = std::holds_alternative<bool>(argsOpt)
                  ? initArgs<T>(std::get<bool>(argsOpt))
                  : std::get<std::vector<BenchArgs<T>>>(argsOpt);

  // Get number of cases for progress display
  uint64_t total = 0;
  if constexpr (std::ranges::sized_range<Range>)
    total = static_cast<uint64_t>(std::ranges::size(cases));
  else if constexpr (std::is_same_v<T, float>)
    total = (1ULL << 32);
  const uint64_t progress_interval = (total > 0 ? total / 100 : 0);

  for (const auto &algo : args) {
    if (!algo.used) {
      fmt::println("# skipping {}", algo.name);
      continue;
    }
    if (algo.name == "dragonbox") {
      fmt::println("# skipping {} because it is the reference.", algo.name);
      continue;
    }
    if (algo_filtered_out(algo.name, algo_filter)) {
      fmt::println("# filtered out {}", algo.name);
      continue;
    }

    fmt::print("# processing {}", algo.name);
    fflush(stdout);

    bool incorrect = false;
    char buf1[100], buf2[100];
    std::span<char> bufRef(buf1, sizeof buf1), bufAlgo(buf2, sizeof buf2);

    uint64_t count = 0;
    for (const auto &tc : cases) {
      if (progress_interval > 0 && (count++ % progress_interval) == 0) {
        std::printf(".");
        std::fflush(stdout);
      }

      const T d = tc.value;
      const std::string sv = tc.str_value ? fmt::format("case: {};", *tc.str_value) : "";

      if (std::isnan(d) || std::isinf(d))
        continue;

      // Reference output, we cannot use std::to_chars here, because it produces
      // the shortest representation, which is not necessarily the same as the
      // representation using the fewest significant digits.
      // So we use dragonbox, which serves as the reference implementation.
      const size_t vRef  = BenchmarkShortest::dragonbox(d, bufRef);
      const size_t vAlgo = algo.func(d, bufAlgo);

      std::string_view svRef{bufRef.data(), vRef},
                       svAlgo{bufAlgo.data(), vAlgo};

      auto countRef  = count_significant_digits(svRef);
      auto countAlgo = count_significant_digits(svAlgo);
      auto backRef   = parse_float<T>(svRef);
      auto backAlgo  = parse_float<T>(svAlgo);

      if(!backRef || !backAlgo) {
        incorrect = true;
        fmt::print(" parse error: {} d = {}, ref={}, algo={}",
            sv, float_to_hex<T>(d), svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if(*backRef != d || *backAlgo != d)
        fmt::println("\n# Error: parsing the output with std::from_chars does not bring back the input.");
      if(*backRef != d) {
        incorrect = true;
        fmt::print(" ref mismatch: {} d = {}, backRef = {}; svRef = {}, svAlgo = {}",
            sv, float_to_hex<T>(d), *backRef, svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if(*backAlgo != d) {
        incorrect = true;
        fmt::print(" algo mismatch: {} d = {}, backAlgo = {}; svRef = {}, svAlgo = {}, "
            "parsing the output with std::from_chars does not recover the original",
            sv, float_to_hex<T>(d), *backAlgo, svRef, svAlgo);
        fflush(stdout);
        break;
      }
      if (countRef != countAlgo) {
        incorrect = true;
        fmt::print(" mismatch: {} d = {}, bufRef = {}, bufAlgo = {}",
            sv, float_to_hex<T>(d), svRef, svAlgo);
        fflush(stdout);
        break;
      }
    }

    fmt::print("\n");
    fmt::println("{:20} {:20}", algo.name, incorrect ? "no" : "yes");
  }
}

template <class function_type>
event_aggregate bench(const function_type &&function, size_t min_repeat = 10,
                      size_t min_time_ns = 400'000'000,
                      size_t max_repeat = 1000000) {
  size_t N = min_repeat;
  if (N == 0) {
    N = 1;
  }
  volatile double dontoptimize = 0.0;
  // We warm up first. We warmup for at least 0.4s (by default). This makes
  // sure that the processor is in a consistent state.
  event_aggregate warm_aggregate{};
  for (size_t i = 0; i < N; i++) {
    std::atomic_thread_fence(std::memory_order_acquire);
    collector.start();
    dontoptimize = double(function());
    std::atomic_thread_fence(std::memory_order_release);
    event_count allocate_count = collector.end();
    warm_aggregate << allocate_count;
    if ((i + 1 == N) && (warm_aggregate.total_elapsed_ns() < min_time_ns) &&
        (N < max_repeat)) {
      N *= 10;
    }
  }
  // Actual measure, another 0.4s (by default), this time with a processor
  // warmed up.
  event_aggregate aggregate{};
  for (size_t i = 0; i < N; i++) {
    std::atomic_thread_fence(std::memory_order_acquire);
    collector.start();
    dontoptimize = double(function());
    std::atomic_thread_fence(std::memory_order_release);
    event_count allocate_count = collector.end();
    aggregate << allocate_count;
    if ((i + 1 == N) && (aggregate.total_elapsed_ns() < min_time_ns) &&
        (N < max_repeat)) {
      N *= 10;
    }
  }
  return aggregate;
}

template <class T, class Func>
void pretty_print(const std::vector<T> &lines, const std::string &name,
                  Func &&function, size_t repeat = 100) {
  const size_t number_of_floats = lines.size();
  const double volume = static_cast<double>(function(lines));
  const double volumeMB = volume / 1'000'000;
  auto agg = bench([&function, &lines]() { return function(lines); }, repeat);

  printf("%-30s: %8.2f MB/s (+/- %.1f %%) ", name.data(),
         volumeMB * 1000'000'000 / agg.fastest_elapsed_ns(),
         (agg.elapsed_ns() - agg.fastest_elapsed_ns()) * 100.0 /
             agg.elapsed_ns());
  printf("%8.2f MB ", volumeMB);
  printf(" %8.2f ns/f ", agg.fastest_elapsed_ns() / number_of_floats);
  printf("%8.2f Mfloat/s\n",
         number_of_floats * 1000 / agg.fastest_elapsed_ns());
  // We only print out performance counters if they are available.
  if (collector.has_events()) {
    // Somewhat arbitrarily, we use two new lines for the counters.
    printf("                               ");
    printf(" %8.2f i/B %8.2f i/f (+/- %.1f %%) ",
           agg.fastest_instructions() / volume,
           agg.fastest_instructions() / number_of_floats,
           (agg.instructions() - agg.fastest_instructions()) * 100.0 /
               agg.instructions());

    printf(" %8.2f c/B %8.2f c/f (+/- %.1f %%)\n",
           agg.fastest_cycles() / volume,
           agg.fastest_cycles() / number_of_floats,
           (agg.cycles() - agg.fastest_cycles()) * 100.0 / agg.cycles());
    printf("                               ");
    printf(" %8.2f i/c ", agg.fastest_instructions() / agg.fastest_cycles());
    printf(" %8.2f b/f ", agg.branches() / number_of_floats);
    printf("           ");
    printf(" %8.2f bm/f ", agg.branch_misses() / number_of_floats);
    printf(" %8.2f GHz ", agg.fastest_cycles() / agg.fastest_elapsed_ns());
    printf("\n");
  }
}

#endif //// BENCHUTIL_H
