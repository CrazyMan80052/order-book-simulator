#include "mdsim/decimal.hpp"

#include <limits>
#include <stdexcept>

namespace mdsim {
namespace {

__extension__ using Int128 = __int128;

int scale_digits(int64_t scale) {
    if (scale <= 0) {
        return -1;
    }

    int digits = 0;
    while (scale > 1 && scale % 10 == 0) {
        scale /= 10;
        ++digits;
    }
    return scale == 1 ? digits : -1;
}

DecimalResult error(DecimalError::Code code) {
    return DecimalError{code};
}

}  // namespace

std::optional<int64_t> parse_nonnegative_integer(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    if (text.front() == '+' || text.front() == '-') {
        return std::nullopt;
    }

    int64_t value = 0;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        const int digit = character - '0';
        if (value > (std::numeric_limits<int64_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }
    return value;
}

DecimalResult parse_scaled_decimal(std::string_view text, int64_t scale) {
    const int fractional_digits = scale_digits(scale);
    if (fractional_digits < 0) {
        return error(DecimalError::Code::INVALID_SCALE);
    }
    if (text.empty()) {
        return error(DecimalError::Code::EMPTY);
    }
    if (text.front() == '-') {
        return error(DecimalError::Code::NEGATIVE);
    }
    if (text.front() == '+') {
        return error(DecimalError::Code::INVALID_CHARACTER);
    }

    const size_t point = text.find('.');
    const size_t whole_end = point == std::string_view::npos ? text.size() : point;
    const size_t fraction_start = point == std::string_view::npos ? text.size() : point + 1;
    const size_t fraction_length = text.size() - fraction_start;
    if (whole_end == 0 || fraction_length > static_cast<size_t>(fractional_digits)) {
        return error(fraction_length > static_cast<size_t>(fractional_digits)
                         ? DecimalError::Code::EXCESS_PRECISION
                         : DecimalError::Code::INVALID_CHARACTER);
    }

    Int128 whole = 0;
    for (size_t index = 0; index < whole_end; ++index) {
        const char character = text[index];
        if (character < '0' || character > '9') {
            return error(DecimalError::Code::INVALID_CHARACTER);
        }
        const int digit = character - '0';
        if (whole > (std::numeric_limits<int64_t>::max() - digit) / 10) {
            return error(DecimalError::Code::OVERFLOW);
        }
        whole = whole * 10 + digit;
    }

    Int128 fraction = 0;
    for (size_t index = fraction_start; index < text.size(); ++index) {
        const char character = text[index];
        if (character < '0' || character > '9') {
            return error(DecimalError::Code::INVALID_CHARACTER);
        }
        fraction = fraction * 10 + (character - '0');
    }
    for (size_t index = fraction_length; index < static_cast<size_t>(fractional_digits); ++index) {
        fraction *= 10;
    }

    const Int128 result = whole * scale + fraction;
    if (result > std::numeric_limits<int64_t>::max()) {
        return error(DecimalError::Code::OVERFLOW);
    }
    return static_cast<int64_t>(result);
}

std::string format_scaled_decimal(int64_t value, int64_t scale) {
    const int fractional_digits = scale_digits(scale);
    if (fractional_digits < 0) {
        throw std::invalid_argument("scale must be a positive power of ten");
    }

    const bool negative = value < 0;
    const uint64_t magnitude = negative
        ? static_cast<uint64_t>(-(value + 1)) + 1U
        : static_cast<uint64_t>(value);
    const uint64_t unsigned_scale = static_cast<uint64_t>(scale);
    const uint64_t whole = magnitude / unsigned_scale;
    const uint64_t fraction = magnitude % unsigned_scale;

    std::string result = negative ? "-" : "";
    result += std::to_string(whole);
    if (fractional_digits == 0) {
        return result;
    }

    result += ".";
    std::string fraction_text = std::to_string(fraction);
    result.append(static_cast<size_t>(fractional_digits) - fraction_text.size(), '0');
    result += fraction_text;
    return result;
}

}  // namespace mdsim
