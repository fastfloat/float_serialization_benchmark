#ifndef FLOATUTILS_H
#define FLOATUTILS_H

#include <charconv>
#include <string>
#include <sstream>
#include <optional>

template<typename T>
concept arithmetic_float
    = std::is_same_v<T, float> || std::is_same_v<T, double>;

size_t count_significant_digits(std::string_view num_str) {
  size_t count = 0;
  size_t trailing_zeros = 0;
  bool leading_zero = true;

  for (char c : num_str) {
    if (c == '.')
      continue;
    if (c == 'e' || c == 'E')
      break; // Stop counting at exponent
    if (std::isdigit(static_cast<unsigned char>(c))) {
      if (c == '0') {
        if (!leading_zero)
          trailing_zeros++;
        continue;
      }
      leading_zero = false;
      count += trailing_zeros + 1;
      trailing_zeros = 0;
    }
  }

  return count;
}

template <arithmetic_float T>
std::string float_to_hex(const T f) {
  std::ostringstream oss;
  oss << std::hexfloat << f;
  return oss.str();
}

template <arithmetic_float T>
std::optional<T> parse_float(std::string_view sv) {
  T result;
  const char* begin = sv.data();
  const char* end = sv.data() + sv.size();

  auto [ptr, ec] = std::from_chars(begin, end, result);

  // Check if parsing succeeded and consumed the entire string
  if (ec == std::errc{} && ptr == end) {
      return result;
  }

  // Return nullopt if parsing failed or didn't consume all input
  return std::nullopt;
}

#endif
