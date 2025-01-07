#ifndef RANDOM_GENERATORS_H
#define RANDOM_GENERATORS_H

#include <random>
#include <iostream>

struct float_number_generator {
  virtual double new_float() { return 0; }
  virtual std::string describe() { return "abstract class"; }
};

struct uniform_generator : float_number_generator {
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_real_distribution<double> dis;
  explicit uniform_generator(double a = 0, double b = 1)
      : rd(), gen(rd()), dis(a, b) {}
  std::string describe() override {
    return std::string("generate random numbers uniformly in the interval [") +
           std::to_string((dis.min)()) + std::string(",") +
           std::to_string((dis.max)()) + std::string("]");
  }
  double new_float() override { return dis(gen); }
};

struct integer_uniform_generator : float_number_generator {
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<uint64_t> dis;
  explicit integer_uniform_generator(uint64_t a = 0, uint64_t b = 1)
      : rd(), gen(rd()), dis(a, b) {}
  std::string describe() override {
    return std::string(
               "generate random untegers numbers uniformly in the interval [") +
           std::to_string((dis.min)()) + std::string(",") +
           std::to_string((dis.max)()) + std::string("]");
  }
  double new_float() override { return dis(gen); }
};

struct one_over_rand32 : float_number_generator {
  std::random_device rd;
  std::mt19937 gen;
  explicit one_over_rand32() : rd(), gen(rd()) {}
  std::string describe() override { return "1 / rand()"; }
  double new_float() override {
    auto g = gen();
    while (g == 0) {
      g = gen();
    }
    double x = 1.0 / double(g);
    return x;
  }
};

struct simple_uniform32 : float_number_generator {
  std::random_device rd;
  std::mt19937 gen;
  explicit simple_uniform32() : rd(), gen(rd()) {}
  std::string describe() override { return "rand() / 0xFFFFFFFF "; }
  double new_float() override {
    double x = double(gen()) / double((std::mt19937::max)());
    return x;
  }
};

struct simple_int32 : float_number_generator {
  std::random_device rd;
  std::mt19937 gen;
  explicit simple_int32() : rd(), gen(rd()) {}
  std::string describe() override { return "rand()"; }
  double new_float() override { return gen(); }
};

struct simple_int64 : float_number_generator {
  std::random_device rd;
  std::mt19937_64 gen;
  std::string describe() override { return "rand64()"; }
  explicit simple_int64() : rd(), gen(rd()) {}
  double new_float() override { return gen(); }
};

std::vector<std::string> model_names = {"uniform",          "one_over_rand32",
                                        "simple_uniform32", "simple_int32",
                                        "int_e_int",        "simple_int64"};
std::unique_ptr<float_number_generator>
get_generator_by_name(std::string name) {
  std::cout << "available models (-m): ";
  for (std::string name : model_names) {
    std::cout << name << " ";
  }
  std::cout << std::endl;
  // This is naive, but also not very important.
  if (name == "uniform") {
    return std::unique_ptr<float_number_generator>(new uniform_generator());
  }
  if (name == "one_over_rand32") {
    return std::unique_ptr<float_number_generator>(new one_over_rand32());
  }
  if (name == "simple_uniform32") {
    return std::unique_ptr<float_number_generator>(new simple_uniform32());
  }
  if (name == "simple_int32") {
    return std::unique_ptr<float_number_generator>(new simple_int32());
  }
  if (name == "simple_int64") {
    return std::unique_ptr<float_number_generator>(new simple_int64());
  }
  std::cerr << " I do not recognize " << name << std::endl;
  std::cerr << " Warning: falling back on uniform generator. " << std::endl;
  return std::unique_ptr<float_number_generator>(new uniform_generator());
}

#endif