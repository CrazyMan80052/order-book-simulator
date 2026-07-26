#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "mdsim/types.hpp"

namespace mdsim {

enum class SequencePolicy : uint8_t {
    STRICT,
    AUDIT,
};

enum class SequenceAction : uint8_t {
    ACCEPTED,
    RESYNCHRONIZED,
    EXACT_DUPLICATE,
    WAITING_FOR_SNAPSHOT,
    OUT_OF_ORDER,
    GAP,
    STALE_REJECTED,
    CONFLICTING_DUPLICATE,
    INVALID_MARKET,
};

struct SequenceResult {
    SequenceAction action;
    SyncState state;
    bool apply_to_book;
    bool stop_replay;
};

struct SequenceCounters {
    uint64_t accepted = 0;
    uint64_t duplicates = 0;
    uint64_t out_of_order = 0;
    uint64_t gaps = 0;
    uint64_t resynchronizations = 0;
    uint64_t rejected = 0;
};

struct MarketSequenceState {
    SyncState state = SyncState::WAITING_FOR_SNAPSHOT;
    SequenceNumber last_sequence = 0;
    std::optional<MarketEvent> last_event;
};

class MarketSequencer {
public:
    explicit MarketSequencer(SequencePolicy policy = SequencePolicy::STRICT);

    SequenceResult process(const MarketEvent& event);
    const MarketSequenceState* state_for(const std::string& market_id) const;
    const SequenceCounters& counters() const;

private:
    SequencePolicy policy_;
    std::map<std::string, MarketSequenceState> markets_;
    SequenceCounters counters_;
};

}  // namespace mdsim
