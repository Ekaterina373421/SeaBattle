#include "MessageParser.hpp"

#include <cstring>
#include <set>

namespace seabattle {

ParseResult MessageParser::parse(const std::string& rawMessage) {
    nlohmann::json json;

    try {
        json = nlohmann::json::parse(rawMessage);
    } catch (const nlohmann::json::parse_error&) {
        return ParseError{-1, "Невалидный JSON"};
    }

    if (!json.contains("type") || !json["type"].is_string()) {
        return ParseError{-1, "Отсутствует поле 'type'"};
    }

    if (!json.contains("action") || !json["action"].is_string()) {
        return ParseError{-1, "Отсутствует поле 'action'"};
    }

    std::string type = json["type"].get<std::string>();
    std::string action = json["action"].get<std::string>();

    if (!isValidMessageType(type)) {
        return ParseError{-1, "Неизвестный тип сообщения: " + type};
    }

    if (!isValidAction(action)) {
        return ParseError{-1, "Неизвестное действие: " + action};
    }

    nlohmann::json payload = json.contains("payload") ? json["payload"] : nlohmann::json::object();

    return ParsedMessage{type, action, payload};
}

std::optional<std::string> MessageParser::extractString(const nlohmann::json& payload,
                                                        const std::string& key) {
    if (payload.contains(key) && payload[key].is_string()) {
        return payload[key].get<std::string>();
    }
    return std::nullopt;
}

std::optional<int> MessageParser::extractInt(const nlohmann::json& payload,
                                             const std::string& key) {
    if (payload.contains(key) && payload[key].is_number_integer()) {
        return payload[key].get<int>();
    }
    return std::nullopt;
}

std::optional<bool> MessageParser::extractBool(const nlohmann::json& payload,
                                               const std::string& key) {
    if (payload.contains(key) && payload[key].is_boolean()) {
        return payload[key].get<bool>();
    }
    return std::nullopt;
}

std::optional<std::vector<ShipPlacement>> MessageParser::extractShips(
    const nlohmann::json& payload) {
    if (!payload.contains("ships") || !payload["ships"].is_array()) {
        return std::nullopt;
    }

    std::vector<ShipPlacement> ships;

    for (const auto& shipJson : payload["ships"]) {
        if (!shipJson.is_object()) {
            return std::nullopt;
        }

        if (!shipJson.contains("x") || !shipJson["x"].is_number_integer() ||
            !shipJson.contains("y") || !shipJson["y"].is_number_integer() ||
            !shipJson.contains("size") || !shipJson["size"].is_number_integer() ||
            !shipJson.contains("horizontal") || !shipJson["horizontal"].is_boolean()) {
            return std::nullopt;
        }

        ShipPlacement ship;
        ship.x = shipJson["x"].get<int>();
        ship.y = shipJson["y"].get<int>();
        ship.size = shipJson["size"].get<int>();
        ship.horizontal = shipJson["horizontal"].get<bool>();

        if (ship.x < 0 || ship.x >= GameConfig::BOARD_SIZE || ship.y < 0 ||
            ship.y >= GameConfig::BOARD_SIZE || ship.size < 1 || ship.size > 4) {
            return std::nullopt;
        }

        ships.push_back(ship);
    }

    return ships;
}

std::optional<std::pair<int, int>> MessageParser::extractCoordinates(
    const nlohmann::json& payload) {
    auto x = extractInt(payload, "x");
    auto y = extractInt(payload, "y");

    if (!x.has_value() || !y.has_value()) {
        return std::nullopt;
    }

    if (*x < 0 || *x >= GameConfig::BOARD_SIZE || *y < 0 || *y >= GameConfig::BOARD_SIZE) {
        return std::nullopt;
    }

    return std::make_pair(*x, *y);
}

bool MessageParser::isValidMessageType(const std::string& type) {
    static const std::set<std::string> validTypes = {protocol::message_type::REQUEST,
                                                     protocol::message_type::RESPONSE,
                                                     protocol::message_type::EVENT};
    return validTypes.count(type) > 0;
}

bool MessageParser::isValidAction(const std::string& action) {
    static const std::set<std::string> validActions = {protocol::action::AUTH,
                                                       protocol::action::GET_PLAYERS,
                                                       protocol::action::PLAYER_STATUS_CHANGED,
                                                       protocol::action::ADD_FRIEND,
                                                       protocol::action::REMOVE_FRIEND,
                                                       protocol::action::GET_FRIENDS,
                                                       protocol::action::GET_STATS,
                                                       protocol::action::INVITE,
                                                       protocol::action::INVITE_RECEIVED,
                                                       protocol::action::INVITE_RESPONSE,
                                                       protocol::action::FIND_GAME,
                                                       protocol::action::CANCEL_SEARCH,
                                                       protocol::action::GAME_STARTED,
                                                       protocol::action::SPECTATE,
                                                       protocol::action::SPECTATE_UPDATE,
                                                       protocol::action::PLACE_SHIPS,
                                                       protocol::action::PLAYER_READY,
                                                       protocol::action::BATTLE_START,
                                                       protocol::action::SHOOT,
                                                       protocol::action::OPPONENT_SHOT,
                                                       protocol::action::SURRENDER,
                                                       protocol::action::GAME_OVER};
    return validActions.count(action) > 0;
}

std::vector<uint8_t> MessageParser::packMessage(const std::string& jsonStr) {
    uint32_t length = static_cast<uint32_t>(jsonStr.size());

    std::vector<uint8_t> result(protocol::LENGTH_HEADER_SIZE + length);

    result[0] = static_cast<uint8_t>((length >> 24) & 0xFF);
    result[1] = static_cast<uint8_t>((length >> 16) & 0xFF);
    result[2] = static_cast<uint8_t>((length >> 8) & 0xFF);
    result[3] = static_cast<uint8_t>(length & 0xFF);

    std::memcpy(result.data() + protocol::LENGTH_HEADER_SIZE, jsonStr.data(), length);

    return result;
}

std::optional<std::string> MessageParser::unpackMessage(const std::vector<uint8_t>& data,
                                                        size_t& bytesConsumed) {
    bytesConsumed = 0;

    if (data.size() < protocol::LENGTH_HEADER_SIZE) {
        return std::nullopt;
    }

    uint32_t length = (static_cast<uint32_t>(data[0]) << 24) |
                      (static_cast<uint32_t>(data[1]) << 16) |
                      (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);

    if (length > protocol::MAX_MESSAGE_SIZE) {
        return std::nullopt;
    }

    if (data.size() < protocol::LENGTH_HEADER_SIZE + length) {
        return std::nullopt;
    }

    bytesConsumed = protocol::LENGTH_HEADER_SIZE + length;

    return std::string(reinterpret_cast<const char*>(data.data() + protocol::LENGTH_HEADER_SIZE),
                       length);
}

}  // namespace seabattle