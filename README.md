# float_serialization_benchmark

This repository contains benchmarking code for floating-point serialization.
The goal is to compare different approaches to serializing floating-point
numbers, i.e., converting them from an IEEE 754 binary representation to a
string decimal representation.

Currently, the following approaches are compared:
  - [dragon4](https://github.com/ahaldane/Dragon4)
  - [std::to_string](https://en.cppreference.com/w/cpp/string/basic_string/to_string)
  - [fmt::format](https://github.com/fmtlib/fmt)
  - [netlib](https://github.com/jwiegley/gdtoa)
  - [sprintf](https://en.cppreference.com/w/c/io/fprintf)
  - [grisu2](https://github.com/simdjson/simdjson/blob/master/src/to_chars.cpp)
  - [grisu-exact](https://github.com/jk-jeon/Grisu-Exact)
  - [Errol](https://github.com/marcandrysco/Errol)
  - [std::to_chars](https://en.cppreference.com/w/cpp/utility/to_chars)
  - [schubfach](https://github.com/abolz/Drachennest/blob/master/src/schubfach_64.cc)
  - [Dragonbox](https://github.com/jk-jeon/dragonbox)
  - [Ryu](https://github.com/ulfjack/ryu)
  - [double-conversion](https://github.com/google/double-conversion)
  - [Abseil](https://github.com/abseil/abseil-cpp)
  - [Teju Jagua](https://github.com/cassioneri/teju_jagua)
  - [yy_double](https://github.com/ibireme/yyjson/issues/200#issuecomment-2726783097)
  - [SwiftDtoa](https://github.com/swiftlang/swift/blob/main/stdlib/public/runtime/SwiftDtoa.cpp)

If you have a recent version of CMake under linux or macOS,  you can simply
go in the directory and type the following commands:

```
cmake -B build .
cmake --build build
./build/benchmarks/benchmark 
```

Some functionality might be disabled under some systems. For example, under macOS with
libc++, some tests might be omitted because it does not yet fully support C++17.

We also support Visual Studio, please refer to the cmake documentation.

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

## Exhaustive 32-bit check

We also include an exhaustive check of all 32-bit floats, to verify
that we can produce the shortest string representation (measured by
the number of significant digits).

Under Linux, you may run the following check:

```
cmake -B build .
cmake --build build --config Release
./build/benchmarks/exhaustivefloat32
```

## Thorough 64-bit check

We also include a thorough check of many 64-bit floats, to verify
that we can produce the shortest string representation (measured by 
the number of significant digits).

Under Linux, you may run the following check:

```
cmake -B build .
cmake --build build --config Release
./build/benchmarks/thoroughfloat64
```

## Fixed-precision evaluation

By default, we compare algorithms that output shortest-significand representation
which can round-trip to the original floating-point value. We can also compare
algorithms that output fixed-precision representation of a given precision:

```
./build/benchmarks/benchmark -f data/canada.txt -F [precision]
```

Note that this only works when we are comparing speeds, not measuring properties
of the algorithms, i.e., we can't use both `-F/--fixed` and `-t/--test` at the same time.

## Other existing benchmarks

- [dtoa Benchmark](https://github.com/miloyip/dtoa-benchmark)
- [parse-bench](https://github.com/alugowski/parse-bench)
- [Drackennest](https://github.com/abolz/Drachennest)
