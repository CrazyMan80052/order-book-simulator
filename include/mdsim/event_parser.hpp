#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "mdsim/types.hpp"

namespace mdsim {

struct ParseError {
    enum class Code : uint8_t {
        INVALID_JSON,
        MISSING_FIELD,
        WRONG_TYPE,
        UNKNOWN_FIELD,
        INVALID_VALUE,
        LIMIT_EXCEEDED,
    };

    Code code;
    size_t line;
    std::string path;
    std::string message;
};

using EventParseResult = std::variant<MarketEvent, ParseError>;

inline constexpr int64_t kPriceScale = 1'000;
inline constexpr int64_t kQuantityScale = 1;
inline constexpr size_t kMaxLineLength = 1U << 20U;
inline constexpr size_t kMaxIdentifierLength = 256;
inline constexpr size_t kMaxNumericLength = 64;
inline constexpr size_t kMaxLevels = 100'000;
inline constexpr size_t kMaxChanges = 100'000;

EventParseResult parse_event_line(std::string_view line, size_t line_number = 0);

}  // namespace mdsim
