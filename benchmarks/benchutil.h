#ifndef BENCHUTIL_H
#define BENCHUTIL_H

#include <cfloat>
#include <cstdio>

#if defined(__linux__) || (__APPLE__ && __aarch64__)
  #define USING_COUNTERS
  #include "counters/event_counter.h"
#else
  #include <chrono>
#endif

#ifdef USING_COUNTERS
template <class T, class Func>
std::vector<event_count> time_it_ns(const std::vector<T> &lines,
                                    Func&& function, size_t repeat) {
  std::vector<event_count> aggregate;
  event_collector collector;
  bool printed_bug = false;
  for (size_t i = 0; i < repeat; i++) {
    collector.start();
    if (function(lines) == 0 && !printed_bug) {
      printf("bug\n");
      printed_bug = true;
    }
    aggregate.push_back(collector.end());
  }
  return aggregate;
}

template <class T, class Func>
void pretty_print(const std::vector<T> &lines, const std::string &name,
                  Func&& function, size_t repeat = 100) {
  const size_t number_of_floats = lines.size();
  const double volume = static_cast<double>(function(lines));
  const double volumeMB = volume / (1024. * 1024.);
  const std::vector<event_count> events = time_it_ns(lines, function, repeat);
  double average_ns{0};
  double min_ns{DBL_MAX};
  double cycles_min{DBL_MAX};
  double instructions_min{DBL_MAX};
  double cycles_avg{0};
  double instructions_avg{0};
  double branches_min{0};
  double branches_avg{0};
  double branch_misses_min{0};
  double branch_misses_avg{0};
  for (event_count e : events) {
    const double ns = e.elapsed_ns();
    average_ns += ns;
    min_ns = std::min(min_ns, ns);

    const double cycles = e.cycles();
    cycles_avg += cycles;
    cycles_min = std::min(cycles_min, cycles);

    const double instructions = e.instructions();
    instructions_avg += instructions;
    instructions_min = std::min(instructions_min, instructions);

    const double branches = e.branches();
    branches_avg += branches;
    branches_min = std::min(branches_min, branches);

    const double branch_misses = e.missed_branches();
    branch_misses_avg += branch_misses;
    branch_misses_min = std::min(branch_misses_min, branch_misses);
  }
  cycles_avg /= events.size();
  instructions_avg /= events.size();
  average_ns /= events.size();
  branches_avg /= events.size();

  printf("%-30s: %8.2f MB/s (+/- %.1f %%) ", name.data(),
         volumeMB * 1000000000 / min_ns,
         (average_ns - min_ns) * 100.0 / average_ns);
  printf("%8.2f MB ", volumeMB);
  printf("%8.2f Mfloat/s  ", number_of_floats * 1000 / min_ns);
  if (instructions_min > 0) {
    printf(" %8.2f i/B %8.2f i/f (+/- %.1f %%) ", instructions_min / volume,
           instructions_min / number_of_floats,
           (instructions_avg - instructions_min) * 100.0 / instructions_avg);

    printf(" %8.2f c/B %8.2f c/f (+/- %.1f %%) ", cycles_min / volume,
           cycles_min / number_of_floats,
           (cycles_avg - cycles_min) * 100.0 / cycles_avg);
    printf(" %8.2f i/c ", instructions_min / cycles_min);
    printf(" %8.2f b/f ", branches_avg / number_of_floats);
    printf(" %8.2f bm/f ", branch_misses_avg / number_of_floats);
    printf(" %8.2f GHz ", cycles_min / min_ns);
  }
  printf("\n");
}
#else
template <class T, class Func>
std::pair<double, double> time_it_ns(const std::vector<T> &lines,
                                     Func&& function, size_t repeat) {
  typename std::chrono::high_resolution_clock::time_point t1, t2;
  double average = 0;
  double min_value = DBL_MAX;
  bool printed_bug = false;
  for (size_t i = 0; i < repeat; i++) {
    t1 = std::chrono::high_resolution_clock::now();
    if (function(lines) == 0 && !printed_bug) {
      printf("bug\n");
      printed_bug = true;
    }
    t2 = std::chrono::high_resolution_clock::now();
    const double dif =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    average += dif;
    min_value = std::min(min_value, dif);
  }
  average /= repeat;
  return std::make_pair(min_value, average);
}

template <class T, class Func>
void pretty_print(const std::vector<T> &lines, const std::string &name,
                  Func&& function, size_t repeat = 100) {
  const size_t number_of_floats = lines.size();
  const double volume = static_cast<double>(function(lines));
  const double volumeMB = volume / (1024. * 1024.);
  const std::pair<double, double> result = time_it_ns(lines, function, repeat);

  printf("%-30s: %8.2f MB/s (+/- %.1f %%) ", name.data(),
         volumeMB * 1000000000 / result.first,
         (result.second - result.first) * 100.0 / result.second);
  printf("%8.2f MB ", volumeMB);
  printf("%8.2f Mfloat/s  ", number_of_floats * 1000 / result.first);
  printf(" %8.2f ns/f \n", double(result.first) / number_of_floats);
}

#endif
#endif //// BENCHUTIL_H
