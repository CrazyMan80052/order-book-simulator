#pragma once

#include <optional>
#include <vector>

#include "mdsim/order_book.hpp"

namespace mdsim {

class ReferenceBook {
public:
    using BidLevels = std::vector<Level>;
    using AskLevels = std::vector<Level>;

    std::optional<BookError> replace_snapshot(const SnapshotPayload& snapshot);
    std::optional<BookError> apply_update(const LevelUpdatePayload& update);

    std::optional<Level> best_bid() const;
    std::optional<Level> best_ask() const;
    Quantity total_quantity(Side side) const;
    std::vector<Level> levels(Side side) const;

private:
    BidLevels bids_;
    AskLevels asks_;
};

}  // namespace mdsim