#include <catch2/catch_test_macros.hpp>

#include "mdsim/sequencer.hpp"

namespace {

mdsim::MarketEvent snapshot(uint64_t sequence, std::string market = "demo") {
    return {
        .type = mdsim::EventType::SNAPSHOT,
        .source = "generated",
        .market_id = std::move(market),
        .sequence = sequence,
        .exchange_timestamp_ns = 0,
        .receive_timestamp_ns = sequence,
        .snapshot = {{{480, 10}}, {{520, 10}}},
        .level_update = {},
    };
}

mdsim::MarketEvent update(uint64_t sequence, std::string market = "demo") {
    return {
        .type = mdsim::EventType::LEVEL_UPDATE,
        .source = "generated",
        .market_id = std::move(market),
        .sequence = sequence,
        .exchange_timestamp_ns = 0,
        .receive_timestamp_ns = sequence,
        .snapshot = {},
        .level_update = {{{mdsim::Side::BUY, 490, 5}}},
    };
}

}  // namespace

TEST_CASE("sequencer requires snapshots and accepts consecutive events") {
    mdsim::MarketSequencer sequencer;
    REQUIRE(sequencer.process(update(1)).action == mdsim::SequenceAction::WAITING_FOR_SNAPSHOT);
    REQUIRE(sequencer.process(snapshot(10)).apply_to_book);
    REQUIRE(sequencer.process(update(11)).action == mdsim::SequenceAction::ACCEPTED);
    REQUIRE(sequencer.state_for("demo")->state == mdsim::SyncState::SYNCHRONIZED);
}

TEST_CASE("sequencer distinguishes exact and conflicting duplicates") {
    mdsim::MarketSequencer sequencer;
    const auto first = snapshot(10);
    REQUIRE(sequencer.process(first).apply_to_book);
    REQUIRE(sequencer.process(first).action == mdsim::SequenceAction::EXACT_DUPLICATE);

    auto conflict = first;
    conflict.receive_timestamp_ns = 99;
    const auto result = sequencer.process(conflict);
    REQUIRE(result.action == mdsim::SequenceAction::CONFLICTING_DUPLICATE);
    REQUIRE(result.state == mdsim::SyncState::INVALID);
    REQUIRE(result.stop_replay);
}

TEST_CASE("strict gaps stale a market and snapshots resynchronize it") {
    mdsim::MarketSequencer sequencer;
    sequencer.process(snapshot(10));
    const auto gap = sequencer.process(update(12));
    REQUIRE(gap.action == mdsim::SequenceAction::GAP);
    REQUIRE(gap.state == mdsim::SyncState::STALE);
    REQUIRE(gap.stop_replay);
    REQUIRE(sequencer.process(update(13)).action == mdsim::SequenceAction::STALE_REJECTED);
    REQUIRE(sequencer.process(snapshot(20)).action == mdsim::SequenceAction::RESYNCHRONIZED);
    REQUIRE(sequencer.process(update(21)).apply_to_book);
}

TEST_CASE("audit policy preserves valid state after unambiguous defects") {
    mdsim::MarketSequencer sequencer(mdsim::SequencePolicy::AUDIT);
    sequencer.process(snapshot(10));
    const auto old = sequencer.process(update(9));
    REQUIRE(old.action == mdsim::SequenceAction::OUT_OF_ORDER);
    REQUIRE_FALSE(old.stop_replay);
    REQUIRE(sequencer.process(update(11)).apply_to_book);
    REQUIRE(sequencer.counters().out_of_order == 1);
}
