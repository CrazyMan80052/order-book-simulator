#include "mdsim/order_book.hpp"

#include <numeric>

namespace mdsim {
namespace {

template <typename Levels>
std::optional<BookError> add_levels(Levels& destination, const std::vector<Level>& source) {
    for (const Level& level : source) {
        if (level.price_ticks <= 0) {
            return BookError::INVALID_PRICE;
        }
        if (level.quantity <= 0) {
            return BookError::NON_POSITIVE_QUANTITY;
        }
        if (!destination.emplace(level.price_ticks, level.quantity).second) {
            return BookError::DUPLICATE_LEVEL;
        }
    }
    return std::nullopt;
}

template <typename Levels>
std::vector<Level> copy_levels(const Levels& source) {
    std::vector<Level> result;
    result.reserve(source.size());
    for (const auto& [price, quantity] : source) {
        result.push_back({price, quantity});
    }
    return result;
}

template <typename Levels>
Quantity sum_quantity(const Levels& source) {
    return std::accumulate(source.begin(), source.end(), Quantity{0},
        [](Quantity total, const auto& level) { return total + level.second; });
}

}  // namespace

std::optional<BookError> OrderBook::replace_snapshot(const SnapshotPayload& snapshot) {
    BidLevels candidate_bids;
    AskLevels candidate_asks;
    if (const auto error = add_levels(candidate_bids, snapshot.bids)) {
        return error;
    }
    if (const auto error = add_levels(candidate_asks, snapshot.asks)) {
        return error;
    }
    bids_ = std::move(candidate_bids);
    asks_ = std::move(candidate_asks);
    return std::nullopt;
}

std::optional<Level> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return Level{bids_.begin()->first, bids_.begin()->second};
}

std::optional<Level> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return Level{asks_.begin()->first, asks_.begin()->second};
}

Quantity OrderBook::total_quantity(Side side) const {
    return side == Side::BUY ? sum_quantity(bids_) : sum_quantity(asks_);
}

std::vector<Level> OrderBook::levels(Side side) const {
    return side == Side::BUY ? copy_levels(bids_) : copy_levels(asks_);
}

}  // namespace mdsim
