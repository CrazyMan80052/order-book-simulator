#include "mdsim/report_writer.hpp"

#include <fstream>

namespace mdsim {
namespace {

nlohmann::json level_json(const Level& level) {
    return nlohmann::json{{"price_ticks", level.price_ticks}, {"quantity", level.quantity}};
}

}  // namespace

nlohmann::json replay_summary_json(const ReplaySummary& summary) {
    return nlohmann::json{
        {"parsed_lines", summary.parsed_lines},
        {"accepted_events", summary.accepted_events},
        {"rejected_events", summary.rejected_events},
        {"malformed_events", summary.malformed_events},
        {"duplicates", summary.duplicates},
        {"out_of_order", summary.out_of_order},
        {"gaps", summary.gaps},
        {"resynchronizations", summary.resynchronizations},
        {"book_errors", summary.book_errors},
    };
}

nlohmann::json replay_books_json(const std::vector<ReplayMarketSnapshot>& books) {
    nlohmann::json markets = nlohmann::json::array();
    for (const ReplayMarketSnapshot& book : books) {
        markets.push_back({
            {"market_id", book.market_id},
            {"bids", nlohmann::json::array()},
            {"asks", nlohmann::json::array()},
        });
        auto& entry = markets.back();
        for (const Level& level : book.bids) {
            entry["bids"].push_back(level_json(level));
        }
        for (const Level& level : book.asks) {
            entry["asks"].push_back(level_json(level));
        }
    }
    return nlohmann::json{{"markets", markets}};
}

nlohmann::json replay_result_json(const ReplayResult& result) {
    return nlohmann::json{
        {"summary", replay_summary_json(result.summary)},
        {"books", replay_books_json(result.books)},
    };
}

void write_json_file(const std::filesystem::path& path, const nlohmann::json& value) {
    std::ofstream output(path);
    output << value.dump(2) << '\n';
}

}  // namespace mdsim