#pragma once

#include <Protocol.hpp>
#include <Types.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace seabattle {

struct ParsedMessage {
    std::string type;
    std::string action;
    nlohmann::json payload;
};

struct ParseError {
    int code;
    std::string message;
};

using ParseResult = std::variant<ParsedMessage, ParseError>;

class MessageParser {
   public:
    static ParseResult parse(const std::string& rawMessage);

    static std::optional<std::string> extractString(const nlohmann::json& payload,
                                                    const std::string& key);

    static std::optional<int> extractInt(const nlohmann::json& payload, const std::string& key);

    static std::optional<bool> extractBool(const nlohmann::json& payload, const std::string& key);

    static std::optional<std::vector<ShipPlacement>> extractShips(const nlohmann::json& payload);

    static std::optional<std::pair<int, int>> extractCoordinates(const nlohmann::json& payload);

    static bool isValidMessageType(const std::string& type);
    static bool isValidAction(const std::string& action);

    static std::vector<uint8_t> packMessage(const std::string& jsonStr);
    static std::optional<std::string> unpackMessage(const std::vector<uint8_t>& data,
                                                    size_t& bytesConsumed);
};

}  // namespace seabattle