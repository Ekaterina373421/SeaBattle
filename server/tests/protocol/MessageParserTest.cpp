#include <gtest/gtest.h>

#include "protocol/MessageParser.hpp"

using namespace seabattle;

class MessageParserTest : public ::testing::Test {
   protected:
    std::string createValidMessage(const std::string& type, const std::string& action,
                                   const nlohmann::json& payload = {}) {
        nlohmann::json msg;
        msg["type"] = type;
        msg["action"] = action;
        msg["payload"] = payload;
        return msg.dump();
    }
};

TEST_F(MessageParserTest, Parse_ValidAuthRequest_Success) {
    std::string msg = createValidMessage("request", "auth", {{"nickname", "Player1"}});

    auto result = MessageParser::parse(msg);

    ASSERT_TRUE(std::holds_alternative<ParsedMessage>(result));
    auto parsed = std::get<ParsedMessage>(result);
    EXPECT_EQ(parsed.type, "request");
    EXPECT_EQ(parsed.action, "auth");
    EXPECT_EQ(parsed.payload["nickname"], "Player1");
}

TEST_F(MessageParserTest, Parse_ValidShootRequest_Success) {
    std::string msg = createValidMessage("request", "shoot", {{"x", 5}, {"y", 3}});

    auto result = MessageParser::parse(msg);

    ASSERT_TRUE(std::holds_alternative<ParsedMessage>(result));
    auto parsed = std::get<ParsedMessage>(result);
    EXPECT_EQ(parsed.action, "shoot");
    EXPECT_EQ(parsed.payload["x"], 5);
    EXPECT_EQ(parsed.payload["y"], 3);
}

TEST_F(MessageParserTest, Parse_InvalidJson_ReturnsError) {
    std::string msg = "not valid json {";

    auto result = MessageParser::parse(msg);

    ASSERT_TRUE(std::holds_alternative<ParseError>(result));
    auto error = std::get<ParseError>(result);
    EXPECT_EQ(error.code, -1);
}

TEST_F(MessageParserTest, Parse_MissingType_ReturnsError) {
    nlohmann::json msg;
    msg["action"] = "auth";
    msg["payload"] = {};

    auto result = MessageParser::parse(msg.dump());

    ASSERT_TRUE(std::holds_alternative<ParseError>(result));
}

TEST_F(MessageParserTest, Parse_MissingAction_ReturnsError) {
    nlohmann::json msg;
    msg["type"] = "request";
    msg["payload"] = {};

    auto result = MessageParser::parse(msg.dump());

    ASSERT_TRUE(std::holds_alternative<ParseError>(result));
}

TEST_F(MessageParserTest, Parse_UnknownType_ReturnsError) {
    std::string msg = createValidMessage("unknown_type", "auth");

    auto result = MessageParser::parse(msg);

    ASSERT_TRUE(std::holds_alternative<ParseError>(result));
}

TEST_F(MessageParserTest, Parse_UnknownAction_ReturnsError) {
    std::string msg = createValidMessage("request", "unknown_action");

    auto result = MessageParser::parse(msg);

    ASSERT_TRUE(std::holds_alternative<ParseError>(result));
}

TEST_F(MessageParserTest, Parse_MissingPayload_UsesEmptyObject) {
    nlohmann::json msg;
    msg["type"] = "request";
    msg["action"] = "auth";

    auto result = MessageParser::parse(msg.dump());

    ASSERT_TRUE(std::holds_alternative<ParsedMessage>(result));
    auto parsed = std::get<ParsedMessage>(result);
    EXPECT_TRUE(parsed.payload.is_object());
}

TEST_F(MessageParserTest, ExtractString_Exists_ReturnsValue) {
    nlohmann::json payload = {{"nickname", "TestPlayer"}};

    auto result = MessageParser::extractString(payload, "nickname");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "TestPlayer");
}

TEST_F(MessageParserTest, ExtractString_NotExists_ReturnsEmpty) {
    nlohmann::json payload = {};

    auto result = MessageParser::extractString(payload, "nickname");

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractString_WrongType_ReturnsEmpty) {
    nlohmann::json payload = {{"nickname", 123}};

    auto result = MessageParser::extractString(payload, "nickname");

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractInt_Exists_ReturnsValue) {
    nlohmann::json payload = {{"x", 5}};

    auto result = MessageParser::extractInt(payload, "x");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 5);
}

TEST_F(MessageParserTest, ExtractInt_NotExists_ReturnsEmpty) {
    nlohmann::json payload = {};

    auto result = MessageParser::extractInt(payload, "x");

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractBool_Exists_ReturnsValue) {
    nlohmann::json payload = {{"accepted", true}};

    auto result = MessageParser::extractBool(payload, "accepted");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(MessageParserTest, ExtractShips_ValidFormat_ReturnsShips) {
    nlohmann::json payload = {{"ships",
                               {{{"x", 0}, {"y", 0}, {"size", 4}, {"horizontal", true}},
                                {{"x", 0}, {"y", 2}, {"size", 3}, {"horizontal", false}}}}};

    auto result = MessageParser::extractShips(payload);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2);
    EXPECT_EQ((*result)[0].x, 0);
    EXPECT_EQ((*result)[0].size, 4);
    EXPECT_TRUE((*result)[0].horizontal);
}

TEST_F(MessageParserTest, ExtractShips_MissingField_ReturnsEmpty) {
    nlohmann::json payload = {{"ships", {{{"x", 0}, {"y", 0}, {"size", 4}}}}};

    auto result = MessageParser::extractShips(payload);

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractShips_InvalidCoordinates_ReturnsEmpty) {
    nlohmann::json payload = {
        {"ships", {{{"x", -1}, {"y", 0}, {"size", 4}, {"horizontal", true}}}}};

    auto result = MessageParser::extractShips(payload);

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractShips_InvalidSize_ReturnsEmpty) {
    nlohmann::json payload = {{"ships", {{{"x", 0}, {"y", 0}, {"size", 5}, {"horizontal", true}}}}};

    auto result = MessageParser::extractShips(payload);

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractShips_NotArray_ReturnsEmpty) {
    nlohmann::json payload = {{"ships", "not an array"}};

    auto result = MessageParser::extractShips(payload);

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractCoordinates_Valid_ReturnsCoords) {
    nlohmann::json payload = {{"x", 5}, {"y", 3}};

    auto result = MessageParser::extractCoordinates(payload);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, 5);
    EXPECT_EQ(result->second, 3);
}

TEST_F(MessageParserTest, ExtractCoordinates_OutOfBounds_ReturnsEmpty) {
    nlohmann::json payload = {{"x", 10}, {"y", 3}};

    auto result = MessageParser::extractCoordinates(payload);

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractCoordinates_Negative_ReturnsEmpty) {
    nlohmann::json payload = {{"x", -1}, {"y", 3}};

    auto result = MessageParser::extractCoordinates(payload);

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, ExtractCoordinates_Missing_ReturnsEmpty) {
    nlohmann::json payload = {{"x", 5}};

    auto result = MessageParser::extractCoordinates(payload);

    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageParserTest, IsValidMessageType_Valid_ReturnsTrue) {
    EXPECT_TRUE(MessageParser::isValidMessageType("request"));
    EXPECT_TRUE(MessageParser::isValidMessageType("response"));
    EXPECT_TRUE(MessageParser::isValidMessageType("event"));
}

TEST_F(MessageParserTest, IsValidMessageType_Invalid_ReturnsFalse) {
    EXPECT_FALSE(MessageParser::isValidMessageType("invalid"));
    EXPECT_FALSE(MessageParser::isValidMessageType(""));
}

TEST_F(MessageParserTest, IsValidAction_Valid_ReturnsTrue) {
    EXPECT_TRUE(MessageParser::isValidAction("auth"));
    EXPECT_TRUE(MessageParser::isValidAction("shoot"));
    EXPECT_TRUE(MessageParser::isValidAction("game_over"));
}

TEST_F(MessageParserTest, IsValidAction_Invalid_ReturnsFalse) {
    EXPECT_FALSE(MessageParser::isValidAction("invalid_action"));
    EXPECT_FALSE(MessageParser::isValidAction(""));
}

TEST_F(MessageParserTest, PackMessage_CreatesCorrectFormat) {
    std::string json = R"({"test":"value"})";

    auto packed = MessageParser::packMessage(json);

    EXPECT_EQ(packed.size(), protocol::LENGTH_HEADER_SIZE + json.size());

    uint32_t length = (static_cast<uint32_t>(packed[0]) << 24) |
                      (static_cast<uint32_t>(packed[1]) << 16) |
                      (static_cast<uint32_t>(packed[2]) << 8) | static_cast<uint32_t>(packed[3]);
    EXPECT_EQ(length, json.size());
}

TEST_F(MessageParserTest, UnpackMessage_ValidData_ReturnsMessage) {
    std::string original = R"({"test":"value"})";
    auto packed = MessageParser::packMessage(original);

    size_t consumed = 0;
    auto result = MessageParser::unpackMessage(packed, consumed);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
    EXPECT_EQ(consumed, packed.size());
}

TEST_F(MessageParserTest, UnpackMessage_InsufficientHeader_ReturnsEmpty) {
    std::vector<uint8_t> data = {0, 0, 0};

    size_t consumed = 0;
    auto result = MessageParser::unpackMessage(data, consumed);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(consumed, 0);
}

TEST_F(MessageParserTest, UnpackMessage_InsufficientBody_ReturnsEmpty) {
    std::vector<uint8_t> data = {0, 0, 0, 10, 'a', 'b', 'c'};

    size_t consumed = 0;
    auto result = MessageParser::unpackMessage(data, consumed);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(consumed, 0);
}

TEST_F(MessageParserTest, PackUnpack_RoundTrip_PreservesData) {
    std::string original = createValidMessage("request", "auth", {{"nickname", "Player1"}});

    auto packed = MessageParser::packMessage(original);
    size_t consumed = 0;
    auto unpacked = MessageParser::unpackMessage(packed, consumed);

    ASSERT_TRUE(unpacked.has_value());
    EXPECT_EQ(*unpacked, original);
}
