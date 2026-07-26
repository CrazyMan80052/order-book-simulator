#include "mdsim/event_parser.hpp"

#include <initializer_list>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "mdsim/decimal.hpp"

namespace mdsim {
namespace {

using Json = nlohmann::json;

struct Failure : std::exception {
    explicit Failure(ParseError error_value) : error(std::move(error_value)) {}
    ParseError error;
};

[[noreturn]] void fail(ParseError::Code code, std::string path, std::string message) {
    throw Failure{{code, 0, std::move(path), std::move(message)}};
}

void require_object(const Json& value, const std::string& path) {
    if (!value.is_object()) {
        fail(ParseError::Code::WRONG_TYPE, path, "expected an object");
    }
}

void require_array(const Json& value, const std::string& path) {
    if (!value.is_array()) {
        fail(ParseError::Code::WRONG_TYPE, path, "expected an array");
    }
}

Json field(const Json& object, std::string_view name, const std::string& path) {
    const auto iterator = object.find(name);
    if (iterator == object.end()) {
        fail(ParseError::Code::MISSING_FIELD, path + "." + std::string(name), "missing field");
    }
    return *iterator;
}

void reject_unknown(const Json& object, std::initializer_list<std::string_view> allowed,
                    const std::string& path) {
    for (const auto& item : object.items()) {
        bool known = false;
        for (const std::string_view name : allowed) {
            known = known || item.key() == name;
        }
        if (!known) {
            fail(ParseError::Code::UNKNOWN_FIELD, path + "." + item.key(), "unknown field");
        }
    }
}

std::string string_field(const Json& object, std::string_view name, const std::string& path) {
    const Json value = field(object, name, path);
    if (!value.is_string()) {
        fail(ParseError::Code::WRONG_TYPE, path + "." + std::string(name), "expected a string");
    }
    const std::string result = value.get<std::string>();
    if (result.empty()) {
        fail(ParseError::Code::INVALID_VALUE, path + "." + std::string(name), "must not be empty");
    }
    if (result.size() > kMaxIdentifierLength) {
        fail(ParseError::Code::LIMIT_EXCEEDED, path + "." + std::string(name), "identifier too long");
    }
    return result;
}

uint64_t unsigned_field(const Json& object, std::string_view name, const std::string& path) {
    const Json value = field(object, name, path);
    if (!value.is_number_unsigned()) {
        fail(ParseError::Code::WRONG_TYPE, path + "." + std::string(name), "expected an unsigned integer");
    }
    return value.get<uint64_t>();
}

int64_t decimal_field(const Json& object, std::string_view name, int64_t scale,
                      const std::string& path) {
    const std::string value = string_field(object, name, path);
    if (value.size() > kMaxNumericLength) {
        fail(ParseError::Code::LIMIT_EXCEEDED, path + "." + std::string(name), "numeric value too long");
    }
    const DecimalResult parsed = parse_scaled_decimal(value, scale);
    if (const auto* error = std::get_if<DecimalError>(&parsed)) {
        fail(ParseError::Code::INVALID_VALUE, path + "." + std::string(name),
             "invalid decimal code " + std::to_string(static_cast<int>(error->code)));
    }
    return std::get<int64_t>(parsed);
}

Level parse_level(const Json& value, const std::string& path) {
    require_object(value, path);
    reject_unknown(value, {"price", "quantity"}, path);
    return Level{
        decimal_field(value, "price", kPriceScale, path),
        decimal_field(value, "quantity", kQuantityScale, path),
    };
}

SnapshotPayload parse_snapshot(const Json& payload) {
    require_object(payload, "payload");
    reject_unknown(payload, {"bids", "asks"}, "payload");
    SnapshotPayload result;
    for (const auto& [name, destination] : {std::pair{"bids", &result.bids}, std::pair{"asks", &result.asks}}) {
        const Json levels = field(payload, name, "payload");
        require_array(levels, "payload." + std::string(name));
        if (levels.size() > kMaxLevels) {
            fail(ParseError::Code::LIMIT_EXCEEDED, "payload." + std::string(name), "too many levels");
        }
        destination->reserve(levels.size());
        for (size_t index = 0; index < levels.size(); ++index) {
            destination->push_back(parse_level(levels[index], "payload." + std::string(name) + "[" +
                                                       std::to_string(index) + "]"));
        }
    }
    return result;
}

LevelUpdatePayload parse_update(const Json& payload) {
    require_object(payload, "payload");
    reject_unknown(payload, {"changes"}, "payload");
    const Json changes = field(payload, "changes", "payload");
    require_array(changes, "payload.changes");
    if (changes.size() > kMaxChanges) {
        fail(ParseError::Code::LIMIT_EXCEEDED, "payload.changes", "too many changes");
    }

    LevelUpdatePayload result;
    result.changes.reserve(changes.size());
    for (size_t index = 0; index < changes.size(); ++index) {
        const std::string path = "payload.changes[" + std::to_string(index) + "]";
        require_object(changes[index], path);
        reject_unknown(changes[index], {"side", "price", "new_quantity"}, path);
        const std::string side = string_field(changes[index], "side", path);
        if (side != "bid" && side != "ask") {
            fail(ParseError::Code::INVALID_VALUE, path + ".side", "expected bid or ask");
        }
        result.changes.push_back({
            side == "bid" ? Side::BUY : Side::SELL,
            decimal_field(changes[index], "price", kPriceScale, path),
            decimal_field(changes[index], "new_quantity", kQuantityScale, path),
        });
    }
    return result;
}

}  // namespace

EventParseResult parse_event_line(std::string_view line, size_t line_number) {
    if (line.size() > kMaxLineLength) {
        return ParseError{ParseError::Code::LIMIT_EXCEEDED, line_number, "$", "line too long"};
    }
    try {
        const Json object = Json::parse(line);
        require_object(object, "$" );
        reject_unknown(object, {"schema_version", "source", "market_id", "sequence",
                                "exchange_timestamp_ns", "receive_timestamp_ns", "type", "payload"}, "$" );

        const Json schema = field(object, "schema_version", "$" );
        if (!schema.is_number_integer() || schema.get<int64_t>() != 1) {
            fail(ParseError::Code::INVALID_VALUE, "$.schema_version", "unsupported schema version");
        }
        const std::string source = string_field(object, "source", "$" );
        const std::string market_id = string_field(object, "market_id", "$" );
        const SequenceNumber sequence = unsigned_field(object, "sequence", "$" );
        const TimestampNs exchange_timestamp_ns = unsigned_field(object, "exchange_timestamp_ns", "$" );
        const TimestampNs receive_timestamp_ns = unsigned_field(object, "receive_timestamp_ns", "$" );
        const std::string type = string_field(object, "type", "$" );
        const Json payload = field(object, "payload", "$" );

        MarketEvent result{};
        result.source = source;
        result.market_id = market_id;
        result.sequence = sequence;
        result.exchange_timestamp_ns = exchange_timestamp_ns;
        result.receive_timestamp_ns = receive_timestamp_ns;
        if (type == "snapshot") {
            result.type = EventType::SNAPSHOT;
            result.snapshot = parse_snapshot(payload);
        } else if (type == "level_update") {
            result.type = EventType::LEVEL_UPDATE;
            result.level_update = parse_update(payload);
        } else {
            fail(ParseError::Code::INVALID_VALUE, "$.type", "unsupported event type");
        }
        return result;
    } catch (const Failure& failure) {
        ParseError result = failure.error;
        result.line = line_number;
        return result;
    } catch (const nlohmann::json::parse_error&) {
        return ParseError{ParseError::Code::INVALID_JSON, line_number, "$", "invalid JSON"};
    }
}

}  // namespace mdsim
