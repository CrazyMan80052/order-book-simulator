#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mdsim {

using PriceTicks = int64_t;
using Quantity = int64_t;
using Money = int64_t;
using SequenceNumber = uint64_t;
using TimestampNs = uint64_t;

enum class Side : uint8_t {
    BUY,
    SELL,
};

enum class EventType : uint8_t {
    SNAPSHOT,
    LEVEL_UPDATE,
};

enum class SyncState : uint8_t {
    WAITING_FOR_SNAPSHOT,
    SYNCHRONIZED,
    STALE,
    INVALID,
};

struct Level {
    PriceTicks price_ticks;
    Quantity quantity;
};

struct SnapshotPayload {
    std::vector<Level> bids;
    std::vector<Level> asks;
};

struct LevelChange {
    Side side;
    PriceTicks price_ticks;
    Quantity new_quantity;
};

struct LevelUpdatePayload {
    std::vector<LevelChange> changes;
};

struct MarketEvent {
    EventType type;
    std::string market_id;
    SequenceNumber sequence;
    TimestampNs exchange_timestamp_ns;
    TimestampNs receive_timestamp_ns;

    // Exactly one payload is used per event, based on type.
    SnapshotPayload snapshot;
    LevelUpdatePayload level_update;
};

struct IOCOrder {
    Side side;
    PriceTicks limit_price_ticks;
    Quantity quantity;
    TimestampNs timestamp_ns;
};

struct Fill {
    PriceTicks price_ticks;
    Quantity quantity;
};

struct QuoteResult {
    Quantity requested_quantity;
    Quantity filled_quantity;
    Quantity remaining_quantity;
    std::vector<Fill> fills;
};

enum class FeeModelKind : uint8_t {
    NO_FEE,
    BASIS_POINTS,
};

struct FeeConfig {
    FeeModelKind kind;
    uint32_t fee_bps;
};

struct FeeResult {
    Money gross_notional;
    Money fee;
    Money net_cash_effect;
};

}  // namespace mdsim