#include "mdsim/execution.hpp"

#include <algorithm>
#include <limits>

namespace mdsim {
namespace {

__extension__ using Int128 = __int128;

struct FillState {
    QuoteResult quote;
    std::optional<PriceTicks> first_fill_price_ticks;
    Int128 gross_notional = 0;
};

ExecutionError error(ExecutionError::Code code) {
    return ExecutionError{code};
}

bool valid_side(Side side) {
    return side == Side::BUY || side == Side::SELL;
}

std::optional<ExecutionError> checked_append_notional(FillState& state, PriceTicks price_ticks,
                                                       Quantity fill_quantity) {
    const Int128 increment = static_cast<Int128>(price_ticks) * static_cast<Int128>(fill_quantity);
    const Int128 updated = state.gross_notional + increment;
    if (updated < std::numeric_limits<Money>::min() || updated > std::numeric_limits<Money>::max()) {
        return error(ExecutionError::Code::OVERFLOW);
    }
    state.gross_notional = updated;
    return std::nullopt;
}

template <typename Levels, typename Marketable>
std::variant<FillState, ExecutionError> quote_from_levels(const Levels& levels, Quantity requested,
                                                          PriceTicks limit_price_ticks,
                                                          Marketable marketable) {
    FillState state{{requested, 0, requested, {}}, std::nullopt, 0};
    for (const Level& level : levels) {
        if (state.quote.remaining_quantity == 0) {
            break;
        }
        if (!marketable(level.price_ticks, limit_price_ticks)) {
            break;
        }

        const Quantity fill_quantity = std::min(state.quote.remaining_quantity, level.quantity);
        if (!state.first_fill_price_ticks) {
            state.first_fill_price_ticks = level.price_ticks;
        }
        state.quote.fills.push_back(Fill{level.price_ticks, fill_quantity});
        state.quote.filled_quantity += fill_quantity;
        state.quote.remaining_quantity -= fill_quantity;

        if (const auto error = checked_append_notional(state, level.price_ticks, fill_quantity)) {
            return *error;
        }
    }
    return state;
}

}  // namespace

ExecutionOutcome quote_ioc(const OrderBook& book, const IOCOrder& order, const FeeModel& fee_model) {
    if (!valid_side(order.side)) {
        return error(ExecutionError::Code::INVALID_SIDE);
    }
    if (order.quantity <= 0) {
        return error(ExecutionError::Code::NON_POSITIVE_QUANTITY);
    }
    if (order.limit_price_ticks <= 0) {
        return error(ExecutionError::Code::NON_POSITIVE_LIMIT_PRICE);
    }

    const auto marketable_buy = [](PriceTicks ask_price_ticks, PriceTicks limit_price_ticks) {
        return ask_price_ticks <= limit_price_ticks;
    };
    const auto marketable_sell = [](PriceTicks bid_price_ticks, PriceTicks limit_price_ticks) {
        return bid_price_ticks >= limit_price_ticks;
    };

    // Human study point: this is the depth-walk algorithm for IOC fills.
    const auto& levels = order.side == Side::BUY ? book.levels(Side::SELL) : book.levels(Side::BUY);
    const auto result = order.side == Side::BUY
        ? quote_from_levels(levels, order.quantity, order.limit_price_ticks, marketable_buy)
        : quote_from_levels(levels, order.quantity, order.limit_price_ticks, marketable_sell);

    if (std::holds_alternative<ExecutionError>(result)) {
        return std::get<ExecutionError>(result);
    }

    FillState state = std::get<FillState>(result);
    const Money gross_notional = static_cast<Money>(state.gross_notional);
    const Money fee = fee_model.fee_for_notional(gross_notional);
    const Money net_cash_effect = order.side == Side::BUY ? -(gross_notional + fee)
                                                           : gross_notional - fee;

    return ExecutionResult{
        .quote = state.quote,
        .fees = FeeResult{gross_notional, fee, net_cash_effect},
        .first_fill_price_ticks = state.first_fill_price_ticks,
    };
}

}  // namespace mdsim