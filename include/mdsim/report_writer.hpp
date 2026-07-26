#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

#include "mdsim/replay_engine.hpp"

namespace mdsim {

nlohmann::json replay_summary_json(const ReplaySummary& summary);
nlohmann::json replay_books_json(const std::vector<ReplayMarketSnapshot>& books);
nlohmann::json replay_result_json(const ReplayResult& result);
void write_json_file(const std::filesystem::path& path, const nlohmann::json& value);

}  // namespace mdsim