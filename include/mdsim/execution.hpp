#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "mdsim/fee_model.hpp"
#include "mdsim/order_book.hpp"

namespace mdsim {

struct ExecutionError {
    enum class Code : uint8_t {
        INVALID_SIDE,
        NON_POSITIVE_QUANTITY,
        NON_POSITIVE_LIMIT_PRICE,
        OVERFLOW,
    };

    Code code;
};

struct ExecutionResult {
    QuoteResult quote;
    FeeResult fees;
    std::optional<PriceTicks> first_fill_price_ticks;
};

using ExecutionOutcome = std::variant<ExecutionResult, ExecutionError>;

ExecutionOutcome quote_ioc(const OrderBook& book, const IOCOrder& order, const FeeModel& fee_model);

}  // namespace mdsim