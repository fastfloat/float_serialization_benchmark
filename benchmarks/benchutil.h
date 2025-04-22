#ifndef BENCHUTIL_H
#define BENCHUTIL_H

#include "counters/event_counter.h"
#include <cfloat>
#include <cstdio>

#include <atomic>
event_collector collector;

template <class function_type>
event_aggregate bench(const function_type &&function, size_t min_repeat = 10,
                      size_t min_time_ns = 400'000'000,
                      size_t max_repeat = 1000000) {
  size_t N = min_repeat;
  if (N == 0) {
    N = 1;
  }
  volatile double dontoptimize = 0.0;
  // We warmm up first. We warmup for at least 0.4s (by default). This makes
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
