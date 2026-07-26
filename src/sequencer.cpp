#include "mdsim/sequencer.hpp"

#include <algorithm>
#include <limits>

namespace mdsim {
namespace {

bool same_levels(const std::vector<Level>& left, const std::vector<Level>& right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
        [](const Level& a, const Level& b) {
            return a.price_ticks == b.price_ticks && a.quantity == b.quantity;
        });
}

bool same_events(const MarketEvent& left, const MarketEvent& right) {
    if (left.type != right.type || left.source != right.source || left.market_id != right.market_id ||
        left.sequence != right.sequence || left.exchange_timestamp_ns != right.exchange_timestamp_ns ||
        left.receive_timestamp_ns != right.receive_timestamp_ns) {
        return false;
    }
    if (left.type == EventType::SNAPSHOT) {
        return same_levels(left.snapshot.bids, right.snapshot.bids) &&
               same_levels(left.snapshot.asks, right.snapshot.asks);
    }
    if (left.level_update.changes.size() != right.level_update.changes.size()) {
        return false;
    }
    return std::equal(left.level_update.changes.begin(), left.level_update.changes.end(),
        right.level_update.changes.begin(), [](const LevelChange& a, const LevelChange& b) {
            return a.side == b.side && a.price_ticks == b.price_ticks && a.new_quantity == b.new_quantity;
        });
}

bool strict_failure(SequencePolicy policy, SequenceAction action) {
    if (policy == SequencePolicy::STRICT) {
        return action != SequenceAction::ACCEPTED && action != SequenceAction::RESYNCHRONIZED &&
               action != SequenceAction::EXACT_DUPLICATE;
    }
    return action == SequenceAction::CONFLICTING_DUPLICATE || action == SequenceAction::INVALID_MARKET;
}

}  // namespace

MarketSequencer::MarketSequencer(SequencePolicy policy) : policy_(policy) {}

SequenceResult MarketSequencer::process(const MarketEvent& event) {
    MarketSequenceState& market = markets_[event.market_id];
    auto result = [&](SequenceAction action, bool apply) {
        return SequenceResult{action, market.state, apply, strict_failure(policy_, action)};
    };

    if (market.state == SyncState::INVALID) {
        ++counters_.rejected;
        return result(SequenceAction::INVALID_MARKET, false);
    }

    const bool is_snapshot = event.type == EventType::SNAPSHOT;
    if (market.state == SyncState::WAITING_FOR_SNAPSHOT) {
        if (!is_snapshot) {
            ++counters_.rejected;
            return result(SequenceAction::WAITING_FOR_SNAPSHOT, false);
        }
        market.state = SyncState::SYNCHRONIZED;
        market.last_sequence = event.sequence;
        market.last_event = event;
        ++counters_.accepted;
        return result(SequenceAction::ACCEPTED, true);
    }

    if (market.state == SyncState::STALE && event.type == EventType::LEVEL_UPDATE) {
        ++counters_.rejected;
        return result(SequenceAction::STALE_REJECTED, false);
    }

    if (event.sequence == market.last_sequence) {
        if (market.last_event && same_events(*market.last_event, event)) {
            ++counters_.duplicates;
            return result(SequenceAction::EXACT_DUPLICATE, false);
        }
        market.state = SyncState::INVALID;
        ++counters_.rejected;
        return result(SequenceAction::CONFLICTING_DUPLICATE, false);
    }

    if (event.sequence < market.last_sequence) {
        ++counters_.out_of_order;
        ++counters_.rejected;
        return result(SequenceAction::OUT_OF_ORDER, false);
    }

    if (is_snapshot) {
        market.state = SyncState::SYNCHRONIZED;
        market.last_sequence = event.sequence;
        market.last_event = event;
        ++counters_.accepted;
        ++counters_.resynchronizations;
        return result(SequenceAction::RESYNCHRONIZED, true);
    }

    const bool consecutive = market.last_sequence != std::numeric_limits<SequenceNumber>::max() &&
                             event.sequence == market.last_sequence + 1;
    if (!consecutive) {
        market.state = SyncState::STALE;
        ++counters_.gaps;
        ++counters_.rejected;
        return result(SequenceAction::GAP, false);
    }
    market.last_sequence = event.sequence;
    market.last_event = event;
    ++counters_.accepted;
    return result(SequenceAction::ACCEPTED, true);
}

const MarketSequenceState* MarketSequencer::state_for(const std::string& market_id) const {
    const auto iterator = markets_.find(market_id);
    return iterator == markets_.end() ? nullptr : &iterator->second;
}

const SequenceCounters& MarketSequencer::counters() const {
    return counters_;
}

}  // namespace mdsim
