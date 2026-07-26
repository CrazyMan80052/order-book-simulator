#pragma once

#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "mdsim/types.hpp"

namespace mdsim {

enum class BookError : uint8_t {
    INVALID_PRICE,
    NON_POSITIVE_QUANTITY,
    DUPLICATE_LEVEL,
    MISSING_LEVEL,
    LOCKED_OR_CROSSED,
};

class OrderBook {
public:
    using BidLevels = std::map<PriceTicks, Quantity, std::greater<PriceTicks>>;
    using AskLevels = std::map<PriceTicks, Quantity, std::less<PriceTicks>>;

    std::optional<BookError> replace_snapshot(const SnapshotPayload& snapshot);

    std::optional<Level> best_bid() const;
    std::optional<Level> best_ask() const;
    Quantity total_quantity(Side side) const;
    std::vector<Level> levels(Side side) const;

private:
    BidLevels bids_;
    AskLevels asks_;
};

}  // namespace mdsim
