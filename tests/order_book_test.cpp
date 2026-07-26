#include <catch2/catch_test_macros.hpp>

#include <variant>

#include "mdsim/decimal.hpp"
#include "mdsim/order_book.hpp"
#include "mdsim/types.hpp"

namespace {

constexpr int64_t kScale = 1'000'000;

void require_error(std::string_view text, mdsim::DecimalError::Code code) {
    const auto result = mdsim::parse_scaled_decimal(text, kScale);
    REQUIRE(std::holds_alternative<mdsim::DecimalError>(result));
    REQUIRE(std::get<mdsim::DecimalError>(result).code == code);
}

}  // namespace

TEST_CASE("domain enums and fixed-point aliases are constructible") {
    const mdsim::Side side = mdsim::Side::BUY;
    const mdsim::EventType event_type = mdsim::EventType::SNAPSHOT;
    const mdsim::SyncState sync_state = mdsim::SyncState::WAITING_FOR_SNAPSHOT;
    const mdsim::PriceTicks price = 48'000;
    const mdsim::Quantity quantity = 100'000;

    REQUIRE(side == mdsim::Side::BUY);
    REQUIRE(event_type == mdsim::EventType::SNAPSHOT);
    REQUIRE(sync_state == mdsim::SyncState::WAITING_FOR_SNAPSHOT);
    REQUIRE(price == 48'000);
    REQUIRE(quantity == 100'000);
}

TEST_CASE("snapshot and level update payloads retain their values") {
    const mdsim::SnapshotPayload snapshot{
        .bids = {{48'000, 100'000}},
        .asks = {{52'000, 80'000}},
    };
    const mdsim::LevelUpdatePayload update{
        .changes = {{mdsim::Side::BUY, 49'000, 25'000}},
    };

    REQUIRE(snapshot.bids.size() == 1);
    REQUIRE(snapshot.bids.front().price_ticks == 48'000);
    REQUIRE(snapshot.asks.front().quantity == 80'000);
    REQUIRE(update.changes.size() == 1);
    REQUIRE(update.changes.front().side == mdsim::Side::BUY);
    REQUIRE(update.changes.front().new_quantity == 25'000);
}

TEST_CASE("IOC orders and fee configuration retain their values") {
    const mdsim::IOCOrder order{
        .side = mdsim::Side::SELL,
        .limit_price_ticks = 52'000,
        .quantity = 10'000,
        .timestamp_ns = 123,
    };
    const mdsim::FeeConfig fee{
        .kind = mdsim::FeeModelKind::BASIS_POINTS,
        .fee_bps = 20,
    };

    REQUIRE(order.side == mdsim::Side::SELL);
    REQUIRE(order.limit_price_ticks == 52'000);
    REQUIRE(order.quantity == 10'000);
    REQUIRE(fee.kind == mdsim::FeeModelKind::BASIS_POINTS);
    REQUIRE(fee.fee_bps == 20);
}

TEST_CASE("scaled decimals parse and format exactly") {
    REQUIRE(std::get<int64_t>(mdsim::parse_scaled_decimal("0.480000", kScale)) == 480'000);
    REQUIRE(std::get<int64_t>(mdsim::parse_scaled_decimal("12", kScale)) == 12'000'000);
    REQUIRE(mdsim::format_scaled_decimal(480'000, kScale) == "0.480000");
    REQUIRE(mdsim::format_scaled_decimal(-1, kScale) == "-0.000001");
}

TEST_CASE("scaled decimal errors identify invalid input") {
    require_error("", mdsim::DecimalError::Code::EMPTY);
    require_error("-1", mdsim::DecimalError::Code::NEGATIVE);
    require_error("1.0000001", mdsim::DecimalError::Code::EXCESS_PRECISION);
    require_error("1e2", mdsim::DecimalError::Code::INVALID_CHARACTER);
    require_error("9223372036854775808", mdsim::DecimalError::Code::OVERFLOW);

    const auto invalid_scale = mdsim::parse_scaled_decimal("1", 3);
    REQUIRE(std::get<mdsim::DecimalError>(invalid_scale).code ==
            mdsim::DecimalError::Code::INVALID_SCALE);
}

TEST_CASE("order book snapshots expose deterministic best levels and depth") {
    mdsim::OrderBook book;
    const mdsim::SnapshotPayload snapshot{
        .bids = {{480, 10}, {475, 20}},
        .asks = {{520, 30}, {525, 40}},
    };

    REQUIRE_FALSE(book.replace_snapshot(snapshot));
    REQUIRE(book.best_bid()->price_ticks == 480);
    REQUIRE(book.best_ask()->price_ticks == 520);
    REQUIRE(book.total_quantity(mdsim::Side::BUY) == 30);
    REQUIRE(book.levels(mdsim::Side::SELL).front().price_ticks == 520);
}

TEST_CASE("invalid snapshots leave the previous book unchanged") {
    mdsim::OrderBook book;
    REQUIRE_FALSE(book.replace_snapshot({{{480, 10}}, {{520, 10}}}));

    const auto error = book.replace_snapshot({{{480, 0}}, {{520, 10}}});
    REQUIRE(error == mdsim::BookError::NON_POSITIVE_QUANTITY);
    REQUIRE(book.best_bid()->quantity == 10);
}

TEST_CASE("absolute updates add modify and cancel levels atomically") {
    mdsim::OrderBook book;
    REQUIRE_FALSE(book.replace_snapshot({{{480, 10}}, {{520, 10}}}));
    REQUIRE_FALSE(book.apply_update({{{mdsim::Side::BUY, 485, 15},
                                      {mdsim::Side::SELL, 520, 0}}}));
    REQUIRE(book.best_bid()->price_ticks == 485);
    REQUIRE_FALSE(book.best_ask());

    REQUIRE_FALSE(book.apply_update({{{mdsim::Side::SELL, 520, 10}}}));
    const auto error = book.apply_update({{{mdsim::Side::BUY, 520, 1}}});
    REQUIRE(error == mdsim::BookError::LOCKED_OR_CROSSED);
    REQUIRE(book.best_bid()->price_ticks == 485);
}

TEST_CASE("locked snapshots and invalid cancellations are rejected") {
    mdsim::OrderBook book;
    REQUIRE(book.replace_snapshot({{{520, 1}}, {{520, 1}}}) == mdsim::BookError::LOCKED_OR_CROSSED);
    REQUIRE_FALSE(book.replace_snapshot({{{480, 1}}, {{520, 1}}}));
    REQUIRE(book.apply_update({{{mdsim::Side::BUY, 470, 0}}}) == mdsim::BookError::MISSING_LEVEL);
}
