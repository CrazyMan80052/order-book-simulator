#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "mdsim/decimal.hpp"
#include "mdsim/execution.hpp"
#include "mdsim/fee_model.hpp"
#include "mdsim/order_book.hpp"
#include "mdsim/report_writer.hpp"
#include "mdsim/replay_engine.hpp"

namespace {

using Json = nlohmann::json;
constexpr int64_t kCliScale = 1'000'000;

enum class Command {
    HELP,
    VALIDATE,
    REPLAY,
    QUOTE,
    ANALYZE_PAIR,
};

struct ParsedArgs {
    Command command = Command::HELP;
    std::map<std::string, std::string> flags;
};

bool is_flag(std::string_view value) {
    return value.rfind("--", 0) == 0;
}

std::optional<std::string> flag_value(const ParsedArgs& args, std::string_view name) {
    const auto iterator = args.flags.find(std::string(name));
    if (iterator == args.flags.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

ParsedArgs parse_args(int argc, char* argv[]) {
    ParsedArgs result;
    if (argc < 2) {
        return result;
    }

    const std::string_view command = argv[1];
    if (command == "validate") {
        result.command = Command::VALIDATE;
    } else if (command == "replay") {
        result.command = Command::REPLAY;
    } else if (command == "quote") {
        result.command = Command::QUOTE;
    } else if (command == "analyze-pair") {
        result.command = Command::ANALYZE_PAIR;
    } else {
        return result;
    }

    for (int index = 2; index < argc; ++index) {
        const std::string_view key = argv[index];
        if (!is_flag(key) || index + 1 >= argc || is_flag(argv[index + 1])) {
            result.command = Command::HELP;
            result.flags.clear();
            return result;
        }
        result.flags.emplace(std::string(key.substr(2)), argv[++index]);
    }
    return result;
}

std::optional<int64_t> parse_units(std::string_view text, int64_t divisor) {
    const mdsim::DecimalResult parsed = mdsim::parse_scaled_decimal(text, kCliScale);
    if (const auto* error = std::get_if<mdsim::DecimalError>(&parsed)) {
        (void)error;
        return std::nullopt;
    }
    const int64_t raw = std::get<int64_t>(parsed);
    if (raw <= 0 || raw % divisor != 0) {
        return std::nullopt;
    }
    return raw / divisor;
}

std::optional<int64_t> parse_nonnegative_units(std::string_view text, int64_t divisor) {
    const mdsim::DecimalResult parsed = mdsim::parse_scaled_decimal(text, kCliScale);
    if (const auto* error = std::get_if<mdsim::DecimalError>(&parsed)) {
        (void)error;
        return std::nullopt;
    }
    const int64_t raw = std::get<int64_t>(parsed);
    if (raw < 0 || raw % divisor != 0) {
        return std::nullopt;
    }
    return raw / divisor;
}

std::optional<mdsim::PriceTicks> parse_price_ticks(std::string_view text) {
    const auto units = parse_units(text, 1'000);
    if (!units) {
        return std::nullopt;
    }
    return static_cast<mdsim::PriceTicks>(*units);
}

std::optional<mdsim::Quantity> parse_quantity_units(std::string_view text) {
    const auto units = parse_units(text, kCliScale);
    if (!units) {
        return std::nullopt;
    }
    return static_cast<mdsim::Quantity>(*units);
}

std::optional<mdsim::Money> parse_money_units(std::string_view text) {
    const auto units = parse_units(text, 1'000);
    if (!units) {
        return std::nullopt;
    }
    return static_cast<mdsim::Money>(*units);
}

std::optional<mdsim::Side> parse_side(std::string_view text) {
    if (text == "buy") {
        return mdsim::Side::BUY;
    }
    if (text == "sell") {
        return mdsim::Side::SELL;
    }
    return std::nullopt;
}

std::optional<mdsim::SequencePolicy> parse_policy(std::string_view text) {
    if (text == "strict") {
        return mdsim::SequencePolicy::STRICT;
    }
    if (text == "audit") {
        return mdsim::SequencePolicy::AUDIT;
    }
    return std::nullopt;
}

bool load_books_file(const std::filesystem::path& path, std::map<std::string, mdsim::OrderBook>& books,
                     std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "unable to open book file: " + path.string();
        return false;
    }

    Json value;
    try {
        input >> value;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }

    if (!value.contains("markets") || !value["markets"].is_array()) {
        error = "book file must contain a markets array";
        return false;
    }

    for (const Json& market : value["markets"]) {
        if (!market.is_object() || !market.contains("market_id") || !market.contains("bids") ||
            !market.contains("asks")) {
            error = "invalid market snapshot";
            return false;
        }
        const std::string market_id = market["market_id"].get<std::string>();
        mdsim::SnapshotPayload snapshot;
        for (const auto& side_name : {std::pair{"bids", &snapshot.bids}, std::pair{"asks", &snapshot.asks}}) {
            if (!market[side_name.first].is_array()) {
                error = "book side must be an array";
                return false;
            }
            for (const Json& level : market[side_name.first]) {
                if (!level.is_object() || !level.contains("price_ticks") || !level.contains("quantity")) {
                    error = "invalid book level";
                    return false;
                }
                side_name.second->push_back({level["price_ticks"].get<mdsim::PriceTicks>(),
                                            level["quantity"].get<mdsim::Quantity>()});
            }
        }
        auto& book = books[market_id];
        if (const auto book_error = book.replace_snapshot(snapshot)) {
            error = "book file contains invalid market state";
            (void)book_error;
            return false;
        }
    }

    return true;
}

Json quote_to_json(const mdsim::IOCOrder& order, const mdsim::ExecutionResult& result) {
    Json fills = Json::array();
    for (const mdsim::Fill& fill : result.quote.fills) {
        fills.push_back({{"price_ticks", fill.price_ticks}, {"quantity", fill.quantity}});
    }

    const mdsim::PriceTicks vwap_price_ticks = result.quote.filled_quantity > 0
        ? static_cast<mdsim::PriceTicks>(result.fees.gross_notional / result.quote.filled_quantity)
        : 0;
    const mdsim::PriceTicks first_fill_price_ticks = result.first_fill_price_ticks.value_or(0);
    const mdsim::PriceTicks slippage_ticks = result.quote.filled_quantity > 0
        ? vwap_price_ticks - first_fill_price_ticks
        : 0;

    return Json{
        {"requested_quantity", order.quantity},
        {"filled_quantity", result.quote.filled_quantity},
        {"remaining_quantity", result.quote.remaining_quantity},
        {"fills", fills},
        {"gross_notional", result.fees.gross_notional},
        {"fee", result.fees.fee},
        {"net_cash_effect", result.fees.net_cash_effect},
        {"vwap_price_ticks", vwap_price_ticks},
        {"first_fill_price_ticks", first_fill_price_ticks},
        {"slippage_ticks", slippage_ticks},
        {"status", result.quote.remaining_quantity == 0
            ? "full"
            : (result.quote.filled_quantity == 0 ? "no_fill" : "partial")},
    };
}

int run_validate(const ParsedArgs& args) {
    const auto input_path = flag_value(args, "input");
    const auto policy_text = flag_value(args, "policy").value_or("strict");
    const auto output_path = flag_value(args, "output");
    if (!input_path || !output_path) {
        return 2;
    }

    const auto policy = parse_policy(policy_text);
    if (!policy) {
        return 2;
    }

    std::ifstream input(*input_path);
    if (!input) {
        return 3;
    }

    mdsim::ReplayEngine engine(*policy);
    const auto result = engine.replay_stream(input);
    mdsim::write_json_file(*output_path, mdsim::replay_summary_json(result.summary));
    return 0;
}

int run_replay(const ParsedArgs& args) {
    const auto input_path = flag_value(args, "input");
    const auto policy_text = flag_value(args, "policy").value_or("strict");
    const auto summary_path = flag_value(args, "summary");
    const auto final_books_path = flag_value(args, "final-books");
    if (!input_path || !summary_path || !final_books_path) {
        return 2;
    }

    const auto policy = parse_policy(policy_text);
    if (!policy) {
        return 2;
    }

    std::ifstream input(*input_path);
    if (!input) {
        return 3;
    }

    mdsim::ReplayEngine engine(*policy);
    const auto result = engine.replay_stream(input);
    mdsim::write_json_file(*summary_path, mdsim::replay_summary_json(result.summary));
    mdsim::write_json_file(*final_books_path, mdsim::replay_books_json(result.books));
    return 0;
}

int run_quote(const ParsedArgs& args) {
    const auto book_path = flag_value(args, "book");
    const auto market_id = flag_value(args, "market");
    const auto side_text = flag_value(args, "side");
    const auto quantity_text = flag_value(args, "quantity");
    const auto limit_price_text = flag_value(args, "limit-price");
    const auto fee_bps_text = flag_value(args, "fee-bps");
    if (!book_path || !market_id || !side_text || !quantity_text || !limit_price_text || !fee_bps_text) {
        return 2;
    }

    std::map<std::string, mdsim::OrderBook> books;
    std::string error;
    if (!load_books_file(*book_path, books, error)) {
        std::cerr << error << '\n';
        return 3;
    }

    const auto side = parse_side(*side_text);
    const auto quantity = parse_quantity_units(*quantity_text);
    const auto limit_price = parse_price_ticks(*limit_price_text);
    const auto fee_bps_value = parse_nonnegative_units(*fee_bps_text, 1);
    if (!side || !quantity || !limit_price || !fee_bps_value) {
        return 2;
    }

    const auto book_iterator = books.find(*market_id);
    if (book_iterator == books.end()) {
        return 2;
    }

    const mdsim::IOCOrder order{*side, *limit_price, *quantity, 0};
    const mdsim::NoFeeModel no_fee_model;
    const mdsim::BasisPointFeeModel fee_model(static_cast<uint32_t>(*fee_bps_value));
    const mdsim::FeeModel& model = *fee_bps_value == 0 ? static_cast<const mdsim::FeeModel&>(no_fee_model)
                                                       : static_cast<const mdsim::FeeModel&>(fee_model);
    const mdsim::ExecutionOutcome outcome = mdsim::quote_ioc(book_iterator->second, order, model);
    if (std::holds_alternative<mdsim::ExecutionError>(outcome)) {
        std::cerr << "execution request error\n";
        return 7;
    }

    std::cout << quote_to_json(order, std::get<mdsim::ExecutionResult>(outcome)).dump(2) << '\n';
    return 0;
}

int run_analyze_pair(const ParsedArgs& args) {
    const auto book_path = flag_value(args, "book");
    const auto market_a = flag_value(args, "market-a");
    const auto market_b = flag_value(args, "market-b");
    const auto quantity_text = flag_value(args, "quantity");
    const auto payout_text = flag_value(args, "settlement-payout");
    const auto fee_bps_a_text = flag_value(args, "fee-bps-a");
    const auto fee_bps_b_text = flag_value(args, "fee-bps-b");
    if (!book_path || !market_a || !market_b || !quantity_text || !payout_text || !fee_bps_a_text ||
        !fee_bps_b_text) {
        return 2;
    }

    std::map<std::string, mdsim::OrderBook> books;
    std::string error;
    if (!load_books_file(*book_path, books, error)) {
        std::cerr << error << '\n';
        return 3;
    }

    const auto quantity = parse_quantity_units(*quantity_text);
    const auto payout = parse_money_units(*payout_text);
    const auto fee_bps_a = parse_nonnegative_units(*fee_bps_a_text, 1);
    const auto fee_bps_b = parse_nonnegative_units(*fee_bps_b_text, 1);
    if (!quantity || !payout || !fee_bps_a || !fee_bps_b) {
        return 2;
    }

    const auto iterator_a = books.find(*market_a);
    const auto iterator_b = books.find(*market_b);
    if (iterator_a == books.end() || iterator_b == books.end()) {
        return 2;
    }

    const auto best_ask_a = iterator_a->second.best_ask();
    const auto best_ask_b = iterator_b->second.best_ask();
    Json output;
    output["displayed"] = {
        {"displayed_discrepancy_per_unit",
         best_ask_a && best_ask_b ? (*payout - best_ask_a->price_ticks - best_ask_b->price_ticks) : 0},
        {"best_ask_a", best_ask_a ? best_ask_a->price_ticks : 0},
        {"best_ask_b", best_ask_b ? best_ask_b->price_ticks : 0},
    };

    const mdsim::IOCOrder order_a{mdsim::Side::BUY, std::numeric_limits<mdsim::PriceTicks>::max(), *quantity, 0};
    const mdsim::IOCOrder order_b{mdsim::Side::BUY, std::numeric_limits<mdsim::PriceTicks>::max(), *quantity, 0};
    const mdsim::BasisPointFeeModel fee_model_a(static_cast<uint32_t>(*fee_bps_a));
    const mdsim::BasisPointFeeModel fee_model_b(static_cast<uint32_t>(*fee_bps_b));
    const auto quote_a = mdsim::quote_ioc(iterator_a->second, order_a, fee_model_a);
    const auto quote_b = mdsim::quote_ioc(iterator_b->second, order_b, fee_model_b);
    if (std::holds_alternative<mdsim::ExecutionError>(quote_a) || std::holds_alternative<mdsim::ExecutionError>(quote_b)) {
        std::cerr << "execution request error\n";
        return 7;
    }

    const auto result_a = std::get<mdsim::ExecutionResult>(quote_a);
    const auto result_b = std::get<mdsim::ExecutionResult>(quote_b);
    const mdsim::Quantity common_quantity = std::min(result_a.quote.filled_quantity, result_b.quote.filled_quantity);
    if (common_quantity == 0) {
        output["simulated_executable"] = nullptr;
        std::cout << output.dump(2) << '\n';
        return 0;
    }

    const mdsim::IOCOrder common_order_a{mdsim::Side::BUY, std::numeric_limits<mdsim::PriceTicks>::max(), common_quantity, 0};
    const mdsim::IOCOrder common_order_b{mdsim::Side::BUY, std::numeric_limits<mdsim::PriceTicks>::max(), common_quantity, 0};
    const auto common_quote_a = mdsim::quote_ioc(iterator_a->second, common_order_a, fee_model_a);
    const auto common_quote_b = mdsim::quote_ioc(iterator_b->second, common_order_b, fee_model_b);
    if (std::holds_alternative<mdsim::ExecutionError>(common_quote_a) ||
        std::holds_alternative<mdsim::ExecutionError>(common_quote_b)) {
        std::cerr << "execution request error\n";
        return 7;
    }

    const auto final_a = std::get<mdsim::ExecutionResult>(common_quote_a);
    const auto final_b = std::get<mdsim::ExecutionResult>(common_quote_b);
    const mdsim::Money settlement_value = *payout * common_quantity;
    const mdsim::Money gross_cost = final_a.fees.gross_notional + final_b.fees.gross_notional;
    const mdsim::Money total_fees = final_a.fees.fee + final_b.fees.fee;
    output["simulated_executable"] = {
        {"common_executable_quantity", common_quantity},
        {"settlement_value", settlement_value},
        {"gross_cost", gross_cost},
        {"fees", total_fees},
        {"net_difference", settlement_value - gross_cost - total_fees},
        {"excluded_effects", Json::array({"latency", "adverse_movement", "rejection", "settlement_risk", "real_venue_execution"})},
    };

    std::cout << output.dump(2) << '\n';
    return 0;
}

void print_usage() {
    std::cerr << "usage: mdsim <validate|replay|quote|analyze-pair> [options]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    const ParsedArgs args = parse_args(argc, argv);
    switch (args.command) {
    case Command::VALIDATE:
        return run_validate(args);
    case Command::REPLAY:
        return run_replay(args);
    case Command::QUOTE:
        return run_quote(args);
    case Command::ANALYZE_PAIR:
        return run_analyze_pair(args);
    case Command::HELP:
    default:
        print_usage();
        return 2;
    }
}
