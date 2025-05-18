#ifndef RANDOM_GENERATORS_H
#define RANDOM_GENERATORS_H

#include <array>
#include <bit>
#include <memory>
#include <random>
#include <iostream>
#include <type_traits>

template <typename T>
struct float_number_generator {
  virtual T new_float() = 0;
  virtual std::string describe() = 0;
  virtual ~float_number_generator() = default;
};

template <typename T>
struct uniform_generator : float_number_generator<T> {
  std::random_device rd;
  std::mt19937_64 gen;
  std::uniform_real_distribution<T> dis;
  explicit uniform_generator(T a = 0.0, T b = 1.0)
      : rd(), gen(rd()), dis(a, b) {}
  std::string describe() override {
    return "generate random numbers uniformly in the interval [" +
           std::to_string((dis.min)()) + std::string(",") +
           std::to_string((dis.max)()) + std::string("]");
  }
  T new_float() override { return dis(gen); }
};

enum centering { centered, non_centered };
template <std::floating_point T, centering C>
struct centered_generator : float_number_generator<T> {
  constexpr static int MAN_BITS = sizeof(T) == 4 ? 23 : 52;
  constexpr static int EXP_BITS = sizeof(T) == 4 ? 8 : 11;
  std::random_device rd;
  std::mt19937_64 gen;
  std::uniform_int_distribution<uint32_t> dist_sign;
  std::uniform_int_distribution<uint32_t> dist_exp; // exclut 0=subnormal et max=inf/NaN
  std::uniform_int_distribution<uint64_t> dist_man; // mantisse, sauf le LSB
  explicit centered_generator()
      : rd(), gen(rd()), dist_sign(0, 1),
        dist_exp(1u << (EXP_BITS - 1), (1u << EXP_BITS) - 2),
        dist_man(1ull, (1ull << MAN_BITS) - 1) {}
  std::string describe() override {
    return "generate random "
      + (C == centered ? std::string("centered") : std::string("non-centered"))
      + " numbers uniformly among all normal floating-point numbers";
  }
  T new_float() override {
    using type_t = typename std::conditional_t< sizeof(T) == 4, uint32_t, uint64_t>;
    const type_t sign = dist_sign(gen);
    const type_t exp  = dist_exp(gen);
    const type_t man  = C == centered ? dist_man(gen) : 0u;
    const type_t bits = (sign << (EXP_BITS + MAN_BITS)) | (exp << MAN_BITS) | man;
    return std::bit_cast<T>(bits);
  }
};

template <typename T>
struct integer_uniform_generator : float_number_generator<T> {
  std::random_device rd;
  std::mt19937_64 gen;
  std::uniform_int_distribution<long> dis;
  explicit integer_uniform_generator(long a = LONG_MIN, long b = LONG_MAX)
      : rd(), gen(rd()), dis(a, b) {}
  std::string describe() override {
    return "generate random integers numbers uniformly in the interval [" +
           std::to_string((dis.min)()) + std::string(",") +
           std::to_string((dis.max)()) + std::string("]");
  }
  T new_float() override { return dis(gen); }
};

template <typename T>
struct simple_uniform : float_number_generator<T> {
  std::random_device rd;
  std::mt19937_64 gen;
  explicit simple_uniform() : rd(), gen(rd()) {}
  std::string describe() override { return "rand() / 0xFFFFFFFF "; }
  T new_float() override {
    const T x = T(gen()) / gen.max();
    return x;
  }
};

template <typename T>
struct simple_int : float_number_generator<T> {
  std::random_device rd;
  std::mt19937_64 gen;
  std::string describe() override { return "rand()"; }
  explicit simple_int() : rd(), gen(rd()) {}
  T new_float() override { return gen(); }
};

template <typename T>
struct one_over_rand : float_number_generator<T> {
  std::random_device rd;
  std::mt19937_64 gen;
  explicit one_over_rand() : rd(), gen(rd()) {}
  std::string describe() override { return "1 / rand()"; }
  T new_float() override {
    auto g = gen();
    while (g == 0) {
      g = gen();
    }
    const T x = T(1.0) / T(g);
    return x;
  }
};

constexpr std::array<const char*, 8> model_names = {
  "uniform_01"     , "uniform_all"  , "integer_uniform" ,
  "centered"       , "non_centered" ,
  "simple_uniform" , "simple_int"   ,
  "one_over_rand"
};

template <typename T>
inline std::unique_ptr<float_number_generator<T>>
get_generator_by_name(std::string name) {
  std::cout << "available models (-m): ";
  for (std::string name : model_names) {
    std::cout << name << " ";
  }
  std::cout << std::endl;

  // This is naive, but also not very important.
  if (name == "uniform_01")
    return std::unique_ptr<float_number_generator<T>>(new uniform_generator<T>());
  if (name == "uniform_all") {
    return std::unique_ptr<float_number_generator<T>>(
        new uniform_generator<T>(std::numeric_limits<T>::lowest(),
                                 std::numeric_limits<T>::max())
    );
  }
  if (name == "centered")
    return std::unique_ptr<float_number_generator<T>>(new centered_generator<T, centered>());
  if (name == "non_centered")
    return std::unique_ptr<float_number_generator<T>>(new centered_generator<T, non_centered>());
  if (name == "integer_uniform")
    return std::unique_ptr<float_number_generator<T>>(new integer_uniform_generator<T>());
  if (name == "simple_uniform")
    return std::unique_ptr<float_number_generator<T>>(new simple_uniform<T>());
  if (name == "simple_int")
    return std::unique_ptr<float_number_generator<T>>(new simple_int<T>());
  if (name == "one_over_rand")
    return std::unique_ptr<float_number_generator<T>>(new one_over_rand<T>());

  std::cerr << " I do not recognize " << name << std::endl;
  std::cerr << " Warning: falling back on uniform_01 generator. " << std::endl;
  return std::unique_ptr<float_number_generator<T>>(new uniform_generator<T>());
}

#endif
