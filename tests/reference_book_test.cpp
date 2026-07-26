#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "mdsim/order_book.hpp"
#include "mdsim/reference_book.hpp"

namespace {

bool same_level(const mdsim::Level& left, const mdsim::Level& right) {
    return left.price_ticks == right.price_ticks && left.quantity == right.quantity;
}

bool same_levels(const std::vector<mdsim::Level>& left, const std::vector<mdsim::Level>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t index = 0; index < left.size(); ++index) {
        if (!same_level(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

void require_same_book(const mdsim::OrderBook& order_book, const mdsim::ReferenceBook& reference_book) {
    REQUIRE(order_book.best_bid().has_value() == reference_book.best_bid().has_value());
    REQUIRE(order_book.best_ask().has_value() == reference_book.best_ask().has_value());
    if (order_book.best_bid()) {
        REQUIRE(same_level(*order_book.best_bid(), *reference_book.best_bid()));
    }
    if (order_book.best_ask()) {
        REQUIRE(same_level(*order_book.best_ask(), *reference_book.best_ask()));
    }
    REQUIRE(same_levels(order_book.levels(mdsim::Side::BUY), reference_book.levels(mdsim::Side::BUY)));
    REQUIRE(same_levels(order_book.levels(mdsim::Side::SELL), reference_book.levels(mdsim::Side::SELL)));
    REQUIRE(order_book.total_quantity(mdsim::Side::BUY) == reference_book.total_quantity(mdsim::Side::BUY));
    REQUIRE(order_book.total_quantity(mdsim::Side::SELL) == reference_book.total_quantity(mdsim::Side::SELL));
}

}  // namespace

TEST_CASE("reference book matches the ordered-map book across valid mutations") {
    mdsim::OrderBook order_book;
    mdsim::ReferenceBook reference_book;

    const mdsim::SnapshotPayload snapshot{
        .bids = {{475, 20}, {480, 10}},
        .asks = {{525, 40}, {520, 30}},
    };
    REQUIRE_FALSE(order_book.replace_snapshot(snapshot));
    REQUIRE_FALSE(reference_book.replace_snapshot(snapshot));
    require_same_book(order_book, reference_book);

    const mdsim::LevelUpdatePayload first_update{
        .changes = {
            {mdsim::Side::BUY, 485, 15},
            {mdsim::Side::SELL, 520, 0},
        },
    };
    REQUIRE_FALSE(order_book.apply_update(first_update));
    REQUIRE_FALSE(reference_book.apply_update(first_update));
    require_same_book(order_book, reference_book);

    const mdsim::LevelUpdatePayload second_update{
        .changes = {
            {mdsim::Side::SELL, 515, 5},
            {mdsim::Side::BUY, 480, 0},
        },
    };
    REQUIRE_FALSE(order_book.apply_update(second_update));
    REQUIRE_FALSE(reference_book.apply_update(second_update));
    require_same_book(order_book, reference_book);
}

TEST_CASE("reference book rejects the same invalid mutations as the ordered-map book") {
    mdsim::OrderBook order_book;
    mdsim::ReferenceBook reference_book;

    REQUIRE_FALSE(order_book.replace_snapshot({{{480, 10}}, {{520, 10}}}));
    REQUIRE_FALSE(reference_book.replace_snapshot({{{480, 10}}, {{520, 10}}}));

    const auto invalid_update = mdsim::LevelUpdatePayload{{{mdsim::Side::BUY, 470, 0}}};
    REQUIRE(order_book.apply_update(invalid_update) == mdsim::BookError::MISSING_LEVEL);
    REQUIRE(reference_book.apply_update(invalid_update) == mdsim::BookError::MISSING_LEVEL);
    require_same_book(order_book, reference_book);
}