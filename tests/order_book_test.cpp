#include <catch2/catch_test_macros.hpp>

#include "mdsim/types.hpp"

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
