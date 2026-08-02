#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace mdsim {

struct DecimalError {
    enum class Code : uint8_t {
        EMPTY,
        INVALID_CHARACTER,
        NEGATIVE,
        EXCESS_PRECISION,
        OVERFLOW,
        INVALID_SCALE,
    };

    Code code;
};

using DecimalResult = std::variant<int64_t, DecimalError>;

DecimalResult parse_scaled_decimal(std::string_view text, int64_t scale);
std::string format_scaled_decimal(int64_t value, int64_t scale);
std::optional<int64_t> parse_nonnegative_integer(std::string_view text);

}  // namespace mdsim
