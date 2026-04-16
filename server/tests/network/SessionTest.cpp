#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <chrono>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "network/Session.hpp"
#include "player/Player.hpp"
#include "protocol/MessageBuilder.hpp"
#include "protocol/MessageParser.hpp"

namespace seabattle {
namespace test {

using boost::asio::ip::tcp;

// ============================================================
// Вспомогательные функции
// ============================================================

static std::vector<uint8_t> makePackedMessage(const std::string& json) {
    return MessageParser::packMessage(json);
}

static std::string makeAuthJson(const std::string& nickname) {
    nlohmann::json j;
    j["type"] = protocol::message_type::REQUEST;
    j["action"] = protocol::action::AUTH;
    j["payload"] = {{"nickname", nickname}};
    return j.dump();
}

// ============================================================
// Фикстура: пара сокетов через loopback
// ============================================================

class SessionTest : public ::testing::Test {
   protected:
    boost::asio::io_context io_context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::vector<std::thread> threads_;

    SessionTest() : work_guard_(boost::asio::make_work_guard(io_context_)) {}

    void SetUp() override {
        for (int i = 0; i < 2; ++i) {
            threads_.emplace_back([this]() { io_context_.run(); });
        }
    }

    void TearDown() override {
        work_guard_.reset();
        io_context_.stop();
        for (auto& t : threads_) {
            if (t.joinable())
                t.join();
        }
    }

    std::pair<tcp::socket, tcp::socket> createSocketPair() {
        tcp::acceptor acceptor(io_context_, tcp::endpoint(tcp::v4(), 0));
        uint16_t port = acceptor.local_endpoint().port();

        tcp::socket client(io_context_);
        tcp::socket server_sock(io_context_);

        std::promise<void> accepted;
        acceptor.async_accept(server_sock, [&](const boost::system::error_code& ec) {
            ASSERT_FALSE(ec) << ec.message();
            accepted.set_value();
        });

        client.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port));
        accepted.get_future().wait();
        acceptor.close();

        return {std::move(client), std::move(server_sock)};
    }

    void sendRaw(tcp::socket& sock, const std::vector<uint8_t>& data) {
        boost::asio::write(sock, boost::asio::buffer(data));
    }

    std::string readOneMessage(tcp::socket& sock) {
        std::vector<uint8_t> header(protocol::LENGTH_HEADER_SIZE);
        boost::asio::read(sock, boost::asio::buffer(header));

        uint32_t netLen;
        std::memcpy(&netLen, header.data(), sizeof(netLen));
        uint32_t bodyLen = ntohl(netLen);

        std::vector<uint8_t> body(bodyLen);
        boost::asio::read(sock, boost::asio::buffer(body));

        return std::string(body.begin(), body.end());
    }
};

// ============================================================
// Тесты создания и базового состояния Session
// ============================================================

TEST_F(SessionTest, NewSession_IsOpen) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    EXPECT_TRUE(session->isOpen());
    EXPECT_EQ(session->getPlayer(), nullptr);

    session->close();
    client.close();
}

TEST_F(SessionTest, Close_MarksSessionClosed) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    session->close();
    EXPECT_FALSE(session->isOpen());

    client.close();
}

TEST_F(SessionTest, DoubleClose_NoThrow) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    EXPECT_NO_THROW(session->close());
    EXPECT_NO_THROW(session->close());

    client.close();
}

// ============================================================
// Тесты отправки сообщений (Session -> клиент)
// ============================================================

TEST_F(SessionTest, Send_SingleMessage_ReceivedByClient) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::string msg = MessageBuilder::buildSuccess(protocol::action::AUTH);
    session->send(msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string received = readOneMessage(client);
    auto parsed = nlohmann::json::parse(received);

    EXPECT_EQ(parsed["action"], protocol::action::AUTH);

    session->close();
    client.close();
}

TEST_F(SessionTest, Send_MultipleMessages_AllReceived) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    const int count = 10;
    for (int i = 0; i < count; ++i) {
        nlohmann::json j;
        j["type"] = protocol::message_type::RESPONSE;
        j["action"] = protocol::action::AUTH;
        j["payload"] = {{"index", i}};
        session->send(j.dump());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    for (int i = 0; i < count; ++i) {
        std::string received = readOneMessage(client);
        auto parsed = nlohmann::json::parse(received);
        EXPECT_EQ(parsed["payload"]["index"], i);
    }

    session->close();
    client.close();
}

TEST_F(SessionTest, Send_LargeMessage_ReceivedCorrectly) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::string largePayload(100000, 'X');
    nlohmann::json j;
    j["type"] = protocol::message_type::RESPONSE;
    j["action"] = protocol::action::AUTH;
    j["payload"] = {{"data", largePayload}};
    std::string msg = j.dump();
    session->send(msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::string received = readOneMessage(client);
    auto parsed = nlohmann::json::parse(received);
    EXPECT_EQ(parsed["payload"]["data"].get<std::string>().size(), 100000u);

    session->close();
    client.close();
}

TEST_F(SessionTest, Send_AfterClose_NoThrowNoCrash) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    session->close();

    EXPECT_NO_THROW(session->send("test message"));

    client.close();
}

TEST_F(SessionTest, Send_EmptyString_NoThrow) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    EXPECT_NO_THROW(session->send(""));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    session->close();
    client.close();
}

// ============================================================
// Тесты приёма сообщений (клиент -> Session)
// ============================================================

TEST_F(SessionTest, Receive_ValidMessage_CallbackInvoked) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::promise<std::string> messagePromise;
    auto messageFuture = messagePromise.get_future();

    session->setMessageCallback(
        [&messagePromise](std::shared_ptr<Session>, const std::string& msg) {
            messagePromise.set_value(msg);
        });

    session->start();

    std::string json = makeAuthJson("TestPlayer");
    sendRaw(client, makePackedMessage(json));

    auto status = messageFuture.wait_for(std::chrono::seconds(3));
    ASSERT_EQ(status, std::future_status::ready);

    std::string received = messageFuture.get();
    auto parsed = nlohmann::json::parse(received);
    EXPECT_EQ(parsed["action"], protocol::action::AUTH);
    EXPECT_EQ(parsed["payload"]["nickname"], "TestPlayer");

    session->close();
    client.close();
}

TEST_F(SessionTest, Receive_MultipleMessages_AllCallbacksInvoked) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::mutex mtx;
    std::vector<std::string> receivedMessages;
    std::condition_variable cv;
    const int count = 5;

    session->setMessageCallback([&](std::shared_ptr<Session>, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        receivedMessages.push_back(msg);
        cv.notify_one();
    });

    session->start();

    for (int i = 0; i < count; ++i) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::AUTH;
        j["payload"] = {{"nickname", "Player" + std::to_string(i)}};
        sendRaw(client, makePackedMessage(j.dump()));
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(5),
                    [&]() { return receivedMessages.size() >= static_cast<size_t>(count); });
    }

    ASSERT_EQ(receivedMessages.size(), static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        auto parsed = nlohmann::json::parse(receivedMessages[i]);
        EXPECT_EQ(parsed["payload"]["nickname"], "Player" + std::to_string(i));
    }

    session->close();
    client.close();
}

TEST_F(SessionTest, Receive_MessageExceedsMaxSize_ConnectionClosed) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::promise<boost::system::error_code> errorPromise;
    auto errorFuture = errorPromise.get_future();

    session->setErrorCallback(
        [&errorPromise](std::shared_ptr<Session>, const boost::system::error_code& ec) {
            errorPromise.set_value(ec);
        });

    session->start();

    uint32_t hugeLength = protocol::MAX_MESSAGE_SIZE + 1;
    uint32_t netLength = htonl(hugeLength);
    std::vector<uint8_t> fakeHeader(4);
    std::memcpy(fakeHeader.data(), &netLength, sizeof(netLength));
    sendRaw(client, fakeHeader);

    auto status = errorFuture.wait_for(std::chrono::seconds(3));
    ASSERT_EQ(status, std::future_status::ready);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(session->isOpen());

    client.close();
}

// ============================================================
// Тесты отключения клиента
// ============================================================

TEST_F(SessionTest, ClientDisconnect_ErrorCallbackInvoked) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::promise<void> errorPromise;
    auto errorFuture = errorPromise.get_future();

    session->setErrorCallback(
        [&errorPromise](std::shared_ptr<Session>, const boost::system::error_code&) {
            errorPromise.set_value();
        });

    session->start();

    client.shutdown(tcp::socket::shutdown_both);
    client.close();

    auto status = errorFuture.wait_for(std::chrono::seconds(3));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_FALSE(session->isOpen());
}

TEST_F(SessionTest, ClientDisconnect_DuringRead_Handled) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::promise<void> errorPromise;

    session->setErrorCallback(
        [&errorPromise](std::shared_ptr<Session>, const boost::system::error_code&) {
            errorPromise.set_value();
        });

    session->start();

    uint32_t length = 100;
    uint32_t netLength = htonl(length);
    std::vector<uint8_t> partialHeader(4);
    std::memcpy(partialHeader.data(), &netLength, sizeof(netLength));
    sendRaw(client, partialHeader);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    client.close();

    auto status = errorPromise.get_future().wait_for(std::chrono::seconds(3));
    EXPECT_EQ(status, std::future_status::ready);
}

TEST_F(SessionTest, ClientSendsPartialHeader_ThenDisconnects) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::promise<void> errorPromise;
    session->setErrorCallback(
        [&errorPromise](std::shared_ptr<Session>, const boost::system::error_code&) {
            errorPromise.set_value();
        });

    session->start();

    std::vector<uint8_t> partial = {0x00, 0x00};
    sendRaw(client, partial);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    client.close();

    auto status = errorPromise.get_future().wait_for(std::chrono::seconds(3));
    EXPECT_EQ(status, std::future_status::ready);
}

// ============================================================
// Тесты привязки игрока к сессии
// ============================================================

TEST_F(SessionTest, SetPlayer_GetPlayer) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    EXPECT_EQ(session->getPlayer(), nullptr);

    auto player = std::make_shared<Player>("guid-1", "Nick");
    session->setPlayer(player);

    auto retrieved = session->getPlayer();
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getGuid(), "guid-1");

    session->close();
    client.close();
}

// ============================================================
// Тесты одновременной отправки и приёма
// ============================================================

TEST_F(SessionTest, SimultaneousSendAndReceive) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::atomic<int> receivedBySession{0};
    std::mutex mtx;
    std::condition_variable cv;

    session->setMessageCallback([&](std::shared_ptr<Session>, const std::string&) {
        receivedBySession++;
        cv.notify_one();
    });

    session->start();

    const int sendCount = 20;

    std::thread sender([&]() {
        for (int i = 0; i < sendCount; ++i) {
            nlohmann::json j;
            j["type"] = protocol::message_type::REQUEST;
            j["action"] = protocol::action::AUTH;
            j["payload"] = {{"nickname", "P" + std::to_string(i)}};
            sendRaw(client, makePackedMessage(j.dump()));
        }
    });

    for (int i = 0; i < sendCount; ++i) {
        session->send(MessageBuilder::buildSuccess(protocol::action::AUTH));
    }

    sender.join();

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(5),
                    [&]() { return receivedBySession.load() >= sendCount; });
    }

    EXPECT_EQ(receivedBySession.load(), sendCount);

    for (int i = 0; i < sendCount; ++i) {
        std::string msg = readOneMessage(client);
        EXPECT_FALSE(msg.empty());
    }

    session->close();
    client.close();
}

// ============================================================
// Тесты конкурентной отправки из нескольких потоков
// ============================================================

TEST_F(SessionTest, ConcurrentSend_FromMultipleThreads) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    const int threadCount = 5;
    const int messagesPerThread = 10;
    const int totalMessages = threadCount * messagesPerThread;

    std::vector<std::thread> senders;
    for (int t = 0; t < threadCount; ++t) {
        senders.emplace_back([&session, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                nlohmann::json j;
                j["type"] = protocol::message_type::RESPONSE;
                j["action"] = protocol::action::AUTH;
                j["payload"] = {{"thread", t}, {"index", i}};
                session->send(j.dump());
            }
        });
    }

    for (auto& t : senders)
        t.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::set<std::string> uniqueMessages;
    for (int i = 0; i < totalMessages; ++i) {
        std::string msg = readOneMessage(client);
        auto parsed = nlohmann::json::parse(msg);
        EXPECT_TRUE(parsed.contains("payload"));
        int thread = parsed["payload"]["thread"];
        int index = parsed["payload"]["index"];
        uniqueMessages.insert(std::to_string(thread) + "_" + std::to_string(index));
    }

    EXPECT_EQ(static_cast<int>(uniqueMessages.size()), totalMessages);

    session->close();
    client.close();
}

// ============================================================
// Тесты некорректных данных от клиента
// ============================================================

TEST_F(SessionTest, Receive_InvalidJson_NoThrow) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::promise<std::string> messagePromise;

    session->setMessageCallback(
        [&messagePromise](std::shared_ptr<Session>, const std::string& msg) {
            messagePromise.set_value(msg);
        });

    session->start();

    std::string invalidJson = "{broken json!!!";
    sendRaw(client, makePackedMessage(invalidJson));

    auto status = messagePromise.get_future().wait_for(std::chrono::seconds(2));
    EXPECT_EQ(status, std::future_status::ready);

    session->close();
    client.close();
}

TEST_F(SessionTest, Receive_BinaryGarbage_NoThrow) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::promise<std::string> messagePromise;

    session->setMessageCallback(
        [&messagePromise](std::shared_ptr<Session>, const std::string& msg) {
            messagePromise.set_value(msg);
        });

    session->start();

    std::vector<uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF, 0xFF, 0x00, 0x42};
    sendRaw(client, makePackedMessage(std::string(garbage.begin(), garbage.end())));

    auto status = messagePromise.get_future().wait_for(std::chrono::seconds(2));
    EXPECT_EQ(status, std::future_status::ready);

    session->close();
    client.close();
}

TEST_F(SessionTest, Receive_ZeroLengthBody) {
    auto [client, server_sock] = createSocketPair();
    auto session = std::make_shared<Session>(std::move(server_sock));

    std::atomic<bool> callbackCalled{false};

    session->setMessageCallback(
        [&callbackCalled](std::shared_ptr<Session>, const std::string&) { callbackCalled = true; });

    session->start();

    uint32_t zero = 0;
    uint32_t netZero = htonl(zero);
    std::vector<uint8_t> zeroHeader(4);
    std::memcpy(zeroHeader.data(), &netZero, sizeof(netZero));
    sendRaw(client, zeroHeader);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    SUCCEED();

    session->close();
    client.close();
}

}  // namespace test
}  // namespace seabattle