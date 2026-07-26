#include "mdsim/replay_engine.hpp"

#include <algorithm>
#include <istream>
#include <string>

#include "mdsim/event_parser.hpp"

namespace mdsim {
namespace {

void copy_counters(ReplaySummary& summary, const SequenceCounters& counters) {
    summary.accepted_events = counters.accepted;
    summary.duplicates = counters.duplicates;
    summary.out_of_order = counters.out_of_order;
    summary.gaps = counters.gaps;
    summary.resynchronizations = counters.resynchronizations;
    summary.rejected_events += counters.rejected;
}

}  // namespace

ReplayEngine::ReplayEngine(SequencePolicy policy) : policy_(policy), sequencer_(policy) {}

ReplayResult ReplayEngine::replay_stream(std::istream& input) {
    ReplayResult result;
    std::string line;
    size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        ++result.summary.parsed_lines;

        const EventParseResult parsed = parse_event_line(line, line_number);
        if (std::holds_alternative<ParseError>(parsed)) {
            ++result.summary.malformed_events;
            ++result.summary.rejected_events;
            if (policy_ == SequencePolicy::STRICT) {
                break;
            }
            continue;
        }

        const MarketEvent& event = std::get<MarketEvent>(parsed);
        const SequenceResult sequence = sequencer_.process(event);
        if (sequence.apply_to_book) {
            OrderBook& book = books_[event.market_id];
            const std::optional<BookError> error = event.type == EventType::SNAPSHOT
                ? book.replace_snapshot(event.snapshot)
                : book.apply_update(event.level_update);
            if (error) {
                ++result.summary.book_errors;
                ++result.summary.rejected_events;
                if (policy_ == SequencePolicy::STRICT) {
                    break;
                }
                continue;
            }
        }

        if (sequence.stop_replay) {
            break;
        }
    }

    copy_counters(result.summary, sequencer_.counters());
    result.summary.rejected_events += result.summary.malformed_events + result.summary.book_errors;

    result.books.reserve(books_.size());
    for (const auto& [market_id, book] : books_) {
        result.books.push_back(ReplayMarketSnapshot{
            market_id,
            book.levels(Side::BUY),
            book.levels(Side::SELL),
        });
    }
    return result;
}

}  // namespace mdsim