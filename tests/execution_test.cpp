#include <catch2/catch_test_macros.hpp>

#include <variant>

#include "mdsim/execution.hpp"

namespace {

const mdsim::SnapshotPayload kBook{
    .bids = {{480, 10}, {475, 20}},
    .asks = {{520, 5}, {525, 10}},
};

template <typename T>
const T& require_value(const mdsim::ExecutionOutcome& outcome) {
    REQUIRE(std::holds_alternative<T>(outcome));
    return std::get<T>(outcome);
}

bool same_fill(const mdsim::Fill& left, const mdsim::Fill& right) {
    return left.price_ticks == right.price_ticks && left.quantity == right.quantity;
}

}  // namespace

TEST_CASE("ioc quotes sweep visible depth without mutating the observed book") {
    mdsim::OrderBook book;
    REQUIRE_FALSE(book.replace_snapshot(kBook));

    const mdsim::IOCOrder order{
        .side = mdsim::Side::BUY,
        .limit_price_ticks = 525,
        .quantity = 12,
        .timestamp_ns = 1,
    };
    const mdsim::BasisPointFeeModel fee_model{20};
    const auto outcome = require_value<mdsim::ExecutionResult>(mdsim::quote_ioc(book, order, fee_model));

    REQUIRE(outcome.quote.requested_quantity == 12);
    REQUIRE(outcome.quote.filled_quantity == 12);
    REQUIRE(outcome.quote.remaining_quantity == 0);
    REQUIRE(outcome.quote.fills.size() == 2);
    REQUIRE(same_fill(outcome.quote.fills[0], {520, 5}));
    REQUIRE(same_fill(outcome.quote.fills[1], {525, 7}));
    REQUIRE(outcome.first_fill_price_ticks == 520);
    REQUIRE(outcome.fees.gross_notional == 6'275);
    REQUIRE(outcome.fees.fee == 13);
    REQUIRE(outcome.fees.net_cash_effect == -6'288);

    REQUIRE(book.best_ask()->price_ticks == 520);
    REQUIRE(book.best_ask()->quantity == 5);
    REQUIRE(book.levels(mdsim::Side::SELL).size() == 2);
}

TEST_CASE("ioc quotes can be no-fill and reject invalid requests") {
    mdsim::OrderBook book;
    REQUIRE_FALSE(book.replace_snapshot(kBook));

    const mdsim::NoFeeModel fee_model;
    const auto no_fill = require_value<mdsim::ExecutionResult>(mdsim::quote_ioc(
        book,
        mdsim::IOCOrder{.side = mdsim::Side::BUY, .limit_price_ticks = 519, .quantity = 9, .timestamp_ns = 1},
        fee_model));
    REQUIRE(no_fill.quote.filled_quantity == 0);
    REQUIRE(no_fill.quote.remaining_quantity == 9);
    REQUIRE(no_fill.quote.fills.empty());
    REQUIRE(no_fill.fees.gross_notional == 0);
    REQUIRE(no_fill.fees.fee == 0);
    REQUIRE(no_fill.fees.net_cash_effect == 0);

    const auto invalid = mdsim::quote_ioc(
        book,
        mdsim::IOCOrder{.side = mdsim::Side::BUY, .limit_price_ticks = 0, .quantity = 1, .timestamp_ns = 1},
        fee_model);
    REQUIRE(std::holds_alternative<mdsim::ExecutionError>(invalid));
    REQUIRE(std::get<mdsim::ExecutionError>(invalid).code ==
            mdsim::ExecutionError::Code::NON_POSITIVE_LIMIT_PRICE);
}