#include <gtest/gtest.h>

#include <cstring>
#include <random>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "protocol/MessageParser.hpp"

namespace seabattle {
namespace test {

class ProtocolEndianTest : public ::testing::Test {};

TEST_F(ProtocolEndianTest, PackMessage_HeaderIsNetworkByteOrder) {
    std::string json = R"({"test": true})";
    auto packed = MessageParser::packMessage(json);

    ASSERT_GE(packed.size(), 4u);

    uint32_t netLength;
    std::memcpy(&netLength, packed.data(), sizeof(netLength));
    uint32_t length = ntohl(netLength);

    EXPECT_EQ(length, static_cast<uint32_t>(json.size()));
}

TEST_F(ProtocolEndianTest, PackUnpack_RoundTrip) {
    std::string original = R"({"action": "auth", "payload": {"nickname": "test"}})";

    auto packed = MessageParser::packMessage(original);

    size_t consumed = 0;
    auto unpacked = MessageParser::unpackMessage(packed, consumed);

    ASSERT_TRUE(unpacked.has_value());
    EXPECT_EQ(*unpacked, original);
    EXPECT_EQ(consumed, packed.size());
}

TEST_F(ProtocolEndianTest, PackUnpack_SmallMessage) {
    std::string smallMsg = "{}";
    auto packed = MessageParser::packMessage(smallMsg);

    size_t consumed = 0;
    auto unpacked = MessageParser::unpackMessage(packed, consumed);

    ASSERT_TRUE(unpacked.has_value());
    EXPECT_EQ(*unpacked, smallMsg);
}

TEST_F(ProtocolEndianTest, Unpack_ExceedsMaxSize_ReturnsNullopt) {
    uint32_t hugeSize = protocol::MAX_MESSAGE_SIZE + 1;
    uint32_t netSize = htonl(hugeSize);

    std::vector<uint8_t> data(4 + 10, 0);
    std::memcpy(data.data(), &netSize, sizeof(netSize));

    size_t consumed = 0;
    auto result = MessageParser::unpackMessage(data, consumed);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(consumed, 0u);
}

TEST_F(ProtocolEndianTest, Unpack_InsufficientData_ReturnsNullopt) {
    uint32_t bodyLen = 100;
    uint32_t netLen = htonl(bodyLen);

    std::vector<uint8_t> data(4 + 50, 0);
    std::memcpy(data.data(), &netLen, sizeof(netLen));

    size_t consumed = 0;
    auto result = MessageParser::unpackMessage(data, consumed);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(consumed, 0u);
}

TEST_F(ProtocolEndianTest, Unpack_TooFewBytesForHeader_ReturnsNullopt) {
    std::vector<uint8_t> data = {0x00, 0x00};

    size_t consumed = 0;
    auto result = MessageParser::unpackMessage(data, consumed);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ProtocolEndianTest, Unpack_EmptyData_ReturnsNullopt) {
    std::vector<uint8_t> data;

    size_t consumed = 0;
    auto result = MessageParser::unpackMessage(data, consumed);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ProtocolEndianTest, PackUnpack_ZeroLengthBody) {
    std::string empty = "";
    auto packed = MessageParser::packMessage(empty);

    size_t consumed = 0;
    auto unpacked = MessageParser::unpackMessage(packed, consumed);

    ASSERT_TRUE(unpacked.has_value());
    EXPECT_TRUE(unpacked->empty());
}

TEST_F(ProtocolEndianTest, Header_ByteOrder_KnownValue_256) {
    // Длина 256 = 0x00000100
    // Big-endian: [0x00][0x00][0x01][0x00]
    std::string msg(256, 'X');
    auto packed = MessageParser::packMessage(msg);

    EXPECT_EQ(packed[0], 0x00);
    EXPECT_EQ(packed[1], 0x00);
    EXPECT_EQ(packed[2], 0x01);
    EXPECT_EQ(packed[3], 0x00);
}

TEST_F(ProtocolEndianTest, Header_ByteOrder_OneByteLength) {
    // Длина 1
    // Big-endian: [0x00][0x00][0x00][0x01]
    std::string msg = "X";
    auto packed = MessageParser::packMessage(msg);

    EXPECT_EQ(packed[0], 0x00);
    EXPECT_EQ(packed[1], 0x00);
    EXPECT_EQ(packed[2], 0x00);
    EXPECT_EQ(packed[3], 0x01);
}

TEST_F(ProtocolEndianTest, Header_ByteOrder_LargeLength) {
    // Длина 70000 = 0x00011170
    // Big-endian: [0x00][0x01][0x11][0x70]
    std::string msg(70000, 'Z');
    auto packed = MessageParser::packMessage(msg);

    EXPECT_EQ(packed[0], 0x00);
    EXPECT_EQ(packed[1], 0x01);
    EXPECT_EQ(packed[2], 0x11);
    EXPECT_EQ(packed[3], 0x70);
}

TEST_F(ProtocolEndianTest, MultipleMessages_InOneBuffer) {
    std::string msg1 = R"({"a":1})";
    std::string msg2 = R"({"b":2})";
    std::string msg3 = R"({"c":3})";

    auto p1 = MessageParser::packMessage(msg1);
    auto p2 = MessageParser::packMessage(msg2);
    auto p3 = MessageParser::packMessage(msg3);

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), p1.begin(), p1.end());
    combined.insert(combined.end(), p2.begin(), p2.end());
    combined.insert(combined.end(), p3.begin(), p3.end());

    size_t offset = 0;
    std::vector<std::string> results;

    while (offset < combined.size()) {
        std::vector<uint8_t> remaining(combined.begin() + static_cast<ptrdiff_t>(offset),
                                       combined.end());
        size_t consumed = 0;
        auto result = MessageParser::unpackMessage(remaining, consumed);

        if (!result.has_value())
            break;
        results.push_back(*result);
        offset += consumed;
    }

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0], msg1);
    EXPECT_EQ(results[1], msg2);
    EXPECT_EQ(results[2], msg3);
}

TEST_F(ProtocolEndianTest, Consistency_htonl_ManualEncoding) {
    std::vector<uint32_t> testValues = {0, 1, 255, 256, 65535, 65536, 16777216, 0x12345678};

    for (uint32_t val : testValues) {
        uint32_t netVal = htonl(val);

        uint8_t manual[4];
        manual[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
        manual[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
        manual[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
        manual[3] = static_cast<uint8_t>(val & 0xFF);

        uint8_t htonlBytes[4];
        std::memcpy(htonlBytes, &netVal, 4);

        EXPECT_EQ(manual[0], htonlBytes[0]) << "Mismatch for value " << val;
        EXPECT_EQ(manual[1], htonlBytes[1]) << "Mismatch for value " << val;
        EXPECT_EQ(manual[2], htonlBytes[2]) << "Mismatch for value " << val;
        EXPECT_EQ(manual[3], htonlBytes[3]) << "Mismatch for value " << val;
    }
}

TEST_F(ProtocolEndianTest, RandomValues_PackUnpack_Consistent) {
    std::mt19937 rng(42);

    for (int i = 0; i < 100; ++i) {
        size_t len = rng() % 10000 + 1;
        std::string msg(len, static_cast<char>('A' + (rng() % 26)));

        auto packed = MessageParser::packMessage(msg);
        size_t consumed = 0;
        auto unpacked = MessageParser::unpackMessage(packed, consumed);

        ASSERT_TRUE(unpacked.has_value()) << "Failed for length " << len;
        EXPECT_EQ(*unpacked, msg);
        EXPECT_EQ(consumed, packed.size());
    }
}

}  // namespace test
}  // namespace seabattle