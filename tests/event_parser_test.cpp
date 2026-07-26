#include <catch2/catch_test_macros.hpp>

#include <variant>

#include "mdsim/event_parser.hpp"

namespace {

constexpr std::string_view kSnapshot = R"({
  "schema_version": 1, "source": "generated", "market_id": "demo",
  "sequence": 10, "exchange_timestamp_ns": 0, "receive_timestamp_ns": 1,
  "type": "snapshot", "payload": {
    "bids": [{"price": "0.480", "quantity": "100"}],
    "asks": [{"price": "0.520", "quantity": "80"}]
  }
})";

template <typename T>
const T& require_value(const mdsim::EventParseResult& result) {
    REQUIRE(std::holds_alternative<T>(result));
    return std::get<T>(result);
}

void require_error(std::string_view line, mdsim::ParseError::Code code) {
    const auto result = mdsim::parse_event_line(line, 7);
    const auto& error = require_value<mdsim::ParseError>(result);
    REQUIRE(error.code == code);
    REQUIRE(error.line == 7);
}

}  // namespace

TEST_CASE("valid snapshot and level update events normalize to domain values") {
    const auto snapshot = require_value<mdsim::MarketEvent>(mdsim::parse_event_line(kSnapshot));
    REQUIRE(snapshot.type == mdsim::EventType::SNAPSHOT);
    REQUIRE(snapshot.source == "generated");
    REQUIRE(snapshot.snapshot.bids.front().price_ticks == 480);
    REQUIRE(snapshot.snapshot.asks.front().quantity == 80);

    const auto update = require_value<mdsim::MarketEvent>(mdsim::parse_event_line(R"({
      "schema_version": 1, "source": "generated", "market_id": "demo",
      "sequence": 11, "exchange_timestamp_ns": 0, "receive_timestamp_ns": 2,
      "type": "level_update", "payload": {
        "changes": [{"side": "bid", "price": "0.490", "new_quantity": "25"}]
      }
    })"));
    REQUIRE(update.type == mdsim::EventType::LEVEL_UPDATE);
    REQUIRE(update.level_update.changes.front().price_ticks == 490);
}

TEST_CASE("event parser reports compact validation errors") {
    require_error("not json", mdsim::ParseError::Code::INVALID_JSON);
    require_error(R"({"schema_version":1})", mdsim::ParseError::Code::MISSING_FIELD);
    require_error(R"({"schema_version":1,"source":"x","market_id":"m","sequence":1,"exchange_timestamp_ns":0,"receive_timestamp_ns":0,"type":"snapshot","payload":{"bids":[],"asks":[],"extra":1}})", mdsim::ParseError::Code::UNKNOWN_FIELD);
    require_error(R"({"schema_version":1,"source":"x","market_id":"m","sequence":1,"exchange_timestamp_ns":0,"receive_timestamp_ns":0,"type":"snapshot","payload":{"bids":[{"price":"0.4801","quantity":"1"}],"asks":[]}})", mdsim::ParseError::Code::INVALID_VALUE);
}
