# float_serialization_benchmark

This repository contains benchmarking code for floating-point serialization.
The goal is to compare different approaches to serializing floating-point
numbers, i.e., converting them from an IEEE 754 binary representation to a
string decimal representation.

Currently, the following approaches are compared:
    - [std::to_string](https://en.cppreference.com/w/cpp/string/basic_string/to_string)
    - [fmt::format](https://github.com/fmtlib/fmt)
    - [netlib](https://github.com/jwiegley/gdtoa)
    - [sprintf](https://en.cppreference.com/w/c/io/fprintf)
    - [grisu2](https://github.com/simdjson/simdjson/blob/master/src/to_chars.cpp)
    - [std::to_chars](https://en.cppreference.com/w/cpp/utility/to_chars)
    - [Dragonbox](https://github.com/jk-jeon/dragonbox)
    - [Ryu](https://github.com/ulfjack/ryu)
    - [double-conversion](https://github.com/google/double-conversion)
    - [Abseil](https://github.com/abseil/abseil-cpp)

If you have a recent version of CMake (3.15 or better) under linux,  you can simply
go in the directory and type the following commands:

```
cmake -B build .
cmake --build build
./build/benchmarks/benchmark 
```

You may use docker to run these benchmarks easily on a variety of platforms: see https://github.com/lemire/docker_programming_station

## Windows

Usage under Windows is similar. After installing cmake and Visual Studio 2019, one might type in the appropriate shell:

```
cmake -B build .
cmake --build build --config Release
.\build\benchmarks\Release\benchmark.exe
```

## Advanced Usage

Serialize the strings (one per line) included in a text file:

```
./build/benchmarks/benchmark -f data/canada.txt
```

Serialize strings generated from floats in (0,1):

```
./build/benchmarks/benchmark
```

## Other existing benchmarks

- [dtoa Benchmark](https://github.com/miloyip/dtoa-benchmark)
- [parse-bench](https://github.com/alugowski/parse-bench)
- [Drackennest](https://github.com/abolz/Drachennest)
