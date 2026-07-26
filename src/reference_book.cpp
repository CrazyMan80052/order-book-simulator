#include "mdsim/reference_book.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <set>

namespace mdsim {
namespace {

bool compare_bid_levels(const Level& left, const Level& right) {
    return left.price_ticks > right.price_ticks;
}

bool compare_ask_levels(const Level& left, const Level& right) {
    return left.price_ticks < right.price_ticks;
}

template <typename Levels>
std::optional<size_t> find_level_index(const Levels& levels, PriceTicks price_ticks) {
    for (size_t index = 0; index < levels.size(); ++index) {
        if (levels[index].price_ticks == price_ticks) {
            return index;
        }
    }
    return std::nullopt;
}

template <typename Levels>
Quantity sum_quantity(const Levels& levels) {
    return std::accumulate(levels.begin(), levels.end(), Quantity{0},
        [](Quantity total, const Level& level) { return total + level.quantity; });
}

bool locked_or_crossed(const ReferenceBook::BidLevels& bids, const ReferenceBook::AskLevels& asks) {
    return !bids.empty() && !asks.empty() && bids.front().price_ticks >= asks.front().price_ticks;
}

template <typename Levels, typename Compare>
void insert_sorted(Levels& levels, const Level& level, Compare compare) {
    const auto iterator = std::lower_bound(levels.begin(), levels.end(), level, compare);
    levels.insert(iterator, level);
}

template <typename Levels>
std::optional<BookError> validate_levels(const Levels& source) {
    std::set<PriceTicks> seen;
    for (const Level& level : source) {
        if (level.price_ticks <= 0) {
            return BookError::INVALID_PRICE;
        }
        if (level.quantity <= 0) {
            return BookError::NON_POSITIVE_QUANTITY;
        }
        if (!seen.insert(level.price_ticks).second) {
            return BookError::DUPLICATE_LEVEL;
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<BookError> ReferenceBook::replace_snapshot(const SnapshotPayload& snapshot) {
    if (const auto error = validate_levels(snapshot.bids)) {
        return error;
    }
    if (const auto error = validate_levels(snapshot.asks)) {
        return error;
    }

    BidLevels candidate_bids = snapshot.bids;
    AskLevels candidate_asks = snapshot.asks;
    std::sort(candidate_bids.begin(), candidate_bids.end(), compare_bid_levels);
    std::sort(candidate_asks.begin(), candidate_asks.end(), compare_ask_levels);

    if (locked_or_crossed(candidate_bids, candidate_asks)) {
        return BookError::LOCKED_OR_CROSSED;
    }

    bids_ = std::move(candidate_bids);
    asks_ = std::move(candidate_asks);
    return std::nullopt;
}

std::optional<BookError> ReferenceBook::apply_update(const LevelUpdatePayload& update) {
    std::set<std::pair<Side, PriceTicks>> touched;
    BidLevels candidate_bids = bids_;
    AskLevels candidate_asks = asks_;

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

        auto& levels = change.side == Side::BUY ? candidate_bids : candidate_asks;
        const auto index = find_level_index(levels, change.price_ticks);
        if (change.new_quantity == 0) {
            if (!index) {
                return BookError::MISSING_LEVEL;
            }
            levels.erase(levels.begin() + static_cast<std::ptrdiff_t>(*index));
            continue;
        }

        if (index) {
            levels[*index].quantity = change.new_quantity;
            continue;
        }

        const Level new_level{change.price_ticks, change.new_quantity};
        if (change.side == Side::BUY) {
            insert_sorted(levels, new_level, compare_bid_levels);
        } else {
            insert_sorted(levels, new_level, compare_ask_levels);
        }
    }

    if (locked_or_crossed(candidate_bids, candidate_asks)) {
        return BookError::LOCKED_OR_CROSSED;
    }

    bids_ = std::move(candidate_bids);
    asks_ = std::move(candidate_asks);
    return std::nullopt;
}

std::optional<Level> ReferenceBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.front();
}

std::optional<Level> ReferenceBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.front();
}

Quantity ReferenceBook::total_quantity(Side side) const {
    return side == Side::BUY ? sum_quantity(bids_) : sum_quantity(asks_);
}

std::vector<Level> ReferenceBook::levels(Side side) const {
    return side == Side::BUY ? std::vector<Level>{bids_.begin(), bids_.end()}
                             : std::vector<Level>{asks_.begin(), asks_.end()};
}

}  // namespace mdsim