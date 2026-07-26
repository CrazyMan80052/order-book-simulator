#pragma once

#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include <vector>

#include "mdsim/order_book.hpp"
#include "mdsim/sequencer.hpp"

namespace mdsim {

struct ReplaySummary {
    uint64_t parsed_lines = 0;
    uint64_t accepted_events = 0;
    uint64_t rejected_events = 0;
    uint64_t malformed_events = 0;
    uint64_t duplicates = 0;
    uint64_t out_of_order = 0;
    uint64_t gaps = 0;
    uint64_t resynchronizations = 0;
    uint64_t book_errors = 0;
};

struct ReplayMarketSnapshot {
    std::string market_id;
    std::vector<Level> bids;
    std::vector<Level> asks;
};

struct ReplayResult {
    ReplaySummary summary;
    std::vector<ReplayMarketSnapshot> books;
};

class ReplayEngine {
public:
    explicit ReplayEngine(SequencePolicy policy = SequencePolicy::STRICT);

    ReplayResult replay_stream(std::istream& input);

private:
    SequencePolicy policy_;
    MarketSequencer sequencer_;
    std::map<std::string, OrderBook> books_;
};

}  // namespace mdsim