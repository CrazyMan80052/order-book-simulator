#include "mdsim/order_book.hpp"

#include <numeric>
#include <set>

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

bool locked_or_crossed(const OrderBook::BidLevels& bids, const OrderBook::AskLevels& asks) {
    return !bids.empty() && !asks.empty() && bids.begin()->first >= asks.begin()->first;
}

struct OriginalLevel {
    Side side;
    PriceTicks price;
    std::optional<Quantity> quantity;
};

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
    if (locked_or_crossed(candidate_bids, candidate_asks)) {
        return BookError::LOCKED_OR_CROSSED;
    }
    bids_ = std::move(candidate_bids);
    asks_ = std::move(candidate_asks);
    return std::nullopt;
}

std::optional<BookError> OrderBook::apply_update(const LevelUpdatePayload& update) {
    std::set<std::pair<Side, PriceTicks>> touched;
    for (const LevelChange& change : update.changes) {
        if (change.price_ticks <= 0) {
            return BookError::INVALID_PRICE;
        }
        if (change.new_quantity < 0) {
            return BookError::NON_POSITIVE_QUANTITY;
        }
        if (!touched.emplace(change.side, change.price_ticks).second) {
            return BookError::DUPLICATE_LEVEL;
        }
        const bool exists = change.side == Side::BUY
            ? bids_.contains(change.price_ticks)
            : asks_.contains(change.price_ticks);
        if (change.new_quantity == 0 && !exists) {
            return BookError::MISSING_LEVEL;
        }
    }

    std::vector<OriginalLevel> journal;
    journal.reserve(update.changes.size());
    for (const LevelChange& change : update.changes) {
        if (change.side == Side::BUY) {
            const auto iterator = bids_.find(change.price_ticks);
            journal.push_back({change.side, change.price_ticks,
                               iterator == bids_.end() ? std::nullopt : std::optional{iterator->second}});
            if (change.new_quantity == 0) {
                bids_.erase(change.price_ticks);
            } else {
                bids_[change.price_ticks] = change.new_quantity;
            }
        } else {
            const auto iterator = asks_.find(change.price_ticks);
            journal.push_back({change.side, change.price_ticks,
                               iterator == asks_.end() ? std::nullopt : std::optional{iterator->second}});
            if (change.new_quantity == 0) {
                asks_.erase(change.price_ticks);
            } else {
                asks_[change.price_ticks] = change.new_quantity;
            }
        }
    }

    if (!locked_or_crossed(bids_, asks_)) {
        return std::nullopt;
    }
    for (auto iterator = journal.rbegin(); iterator != journal.rend(); ++iterator) {
        if (iterator->side == Side::BUY && iterator->quantity) {
            bids_[iterator->price] = *iterator->quantity;
        } else if (iterator->side == Side::BUY) {
            bids_.erase(iterator->price);
        } else if (iterator->quantity) {
            asks_[iterator->price] = *iterator->quantity;
        } else {
            asks_.erase(iterator->price);
        }
    }
    return BookError::LOCKED_OR_CROSSED;
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
