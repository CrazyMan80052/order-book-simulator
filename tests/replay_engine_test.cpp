#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include "mdsim/replay_engine.hpp"

namespace {

constexpr char kReplayInput[] = R"({"schema_version":1,"source":"generated","market_id":"demo","sequence":1,"exchange_timestamp_ns":0,"receive_timestamp_ns":1,"type":"snapshot","payload":{"bids":[{"price":"0.480","quantity":"10"}],"asks":[{"price":"0.520","quantity":"5"}]}}
{"schema_version":1,"source":"generated","market_id":"demo","sequence":2,"exchange_timestamp_ns":0,"receive_timestamp_ns":2,"type":"level_update","payload":{"changes":[{"side":"bid","price":"0.485","new_quantity":"15"}]}})";

}  // namespace

TEST_CASE("replay engine streams valid events into a final book") {
    std::istringstream input(kReplayInput);
    mdsim::ReplayEngine engine;
    const auto result = engine.replay_stream(input);

    REQUIRE(result.summary.parsed_lines == 2);
    REQUIRE(result.summary.accepted_events == 2);
    REQUIRE(result.summary.rejected_events == 0);
    REQUIRE(result.books.size() == 1);
    REQUIRE(result.books.front().market_id == "demo");
    REQUIRE(result.books.front().bids.front().price_ticks == 485);
    REQUIRE(result.books.front().asks.front().quantity == 5);
}