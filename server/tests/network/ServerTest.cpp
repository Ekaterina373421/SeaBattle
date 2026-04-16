#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <chrono>
#include <cstring>
#include <nlohmann/json.hpp>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <spdlog/spdlog.h>

#include "game/GameManager.hpp"
#include "lobby/Lobby.hpp"
#include "network/MessageHandler.hpp"
#include "network/Server.hpp"
#include "network/Session.hpp"
#include "network/SessionManager.hpp"
#include "player/PlayerManager.hpp"
#include "protocol/MessageParser.hpp"
#include "utils/Logger.hpp"

namespace seabattle {
namespace test {

using boost::asio::ip::tcp;

// ============================================================
// Тестовый клиент
// ============================================================

class TestClient {
   public:
    TestClient() : socket_(io_context_) {}

    void connect(uint16_t port) {
        tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), port);
        socket_.connect(endpoint);
    }

    void sendMessage(const std::string& json) {
        auto packed = MessageParser::packMessage(json);
        boost::asio::write(socket_, boost::asio::buffer(packed));
    }

    void sendAuth(const std::string& nickname) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::AUTH;
        j["payload"] = {{"nickname", nickname}};
        sendMessage(j.dump());
    }

    void sendAuth(const std::string& nickname, const std::string& guid) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::AUTH;
        j["payload"] = {{"nickname", nickname}, {"guid", guid}};
        sendMessage(j.dump());
    }

    void sendFindGame() {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::FIND_GAME;
        j["payload"] = nlohmann::json::object();
        sendMessage(j.dump());
    }

    void sendPlaceShipsRandom() {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::PLACE_SHIPS;
        j["payload"] = {{"random", true}};
        sendMessage(j.dump());
    }

    void sendShoot(int x, int y) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::SHOOT;
        j["payload"] = {{"x", x}, {"y", y}};
        sendMessage(j.dump());
    }

    void sendSurrender() {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::SURRENDER;
        j["payload"] = nlohmann::json::object();
        sendMessage(j.dump());
    }

    void sendGetPlayers() {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::GET_PLAYERS;
        j["payload"] = nlohmann::json::object();
        sendMessage(j.dump());
    }

    void sendGetStats(const std::string& targetGuid) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::GET_STATS;
        j["payload"] = {{"guid", targetGuid}};
        sendMessage(j.dump());
    }

    void sendAddFriend(const std::string& friendGuid) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::ADD_FRIEND;
        j["payload"] = {{"guid", friendGuid}};
        sendMessage(j.dump());
    }

    void sendInvite(const std::string& targetGuid) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::INVITE;
        j["payload"] = {{"guid", targetGuid}};
        sendMessage(j.dump());
    }

    void sendInviteResponse(const std::string& inviteId, bool accept) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = protocol::action::INVITE_RESPONSE;
        j["payload"] = {{"invite_id", inviteId}, {"accept", accept}};
        sendMessage(j.dump());
    }

    std::string readResponse() {
        std::vector<uint8_t> header(protocol::LENGTH_HEADER_SIZE);
        boost::asio::read(socket_, boost::asio::buffer(header));

        uint32_t netLen;
        std::memcpy(&netLen, header.data(), sizeof(netLen));
        uint32_t bodyLen = ntohl(netLen);

        std::vector<uint8_t> body(bodyLen);
        boost::asio::read(socket_, boost::asio::buffer(body));

        return std::string(body.begin(), body.end());
    }

    nlohmann::json readJson() {
        return nlohmann::json::parse(readResponse());
    }

    std::vector<nlohmann::json> readAllMessages(
        std::chrono::milliseconds duration = std::chrono::milliseconds(500)) {
        std::vector<nlohmann::json> messages;
        auto deadline = std::chrono::steady_clock::now() + duration;

        socket_.non_blocking(true);

        while (std::chrono::steady_clock::now() < deadline) {
            try {
                std::vector<uint8_t> header(protocol::LENGTH_HEADER_SIZE);
                boost::system::error_code ec;

                size_t readBytes = 0;
                while (readBytes < protocol::LENGTH_HEADER_SIZE) {
                    size_t n = socket_.read_some(
                        boost::asio::buffer(header.data() + readBytes,
                                            protocol::LENGTH_HEADER_SIZE - readBytes),
                        ec);
                    if (ec == boost::asio::error::would_block) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        if (std::chrono::steady_clock::now() >= deadline)
                            goto done;
                        continue;
                    }
                    if (ec)
                        goto done;
                    readBytes += n;
                }

                {
                    uint32_t netLen;
                    std::memcpy(&netLen, header.data(), sizeof(netLen));
                    uint32_t bodyLen = ntohl(netLen);

                    socket_.non_blocking(false);
                    std::vector<uint8_t> body(bodyLen);
                    boost::asio::read(socket_, boost::asio::buffer(body));
                    socket_.non_blocking(true);

                    messages.push_back(
                        nlohmann::json::parse(std::string(body.begin(), body.end())));
                }
            } catch (...) {
                break;
            }
        }
    done:
        socket_.non_blocking(false);
        return messages;
    }

    void disconnect() {
        boost::system::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

    bool isConnected() const {
        return socket_.is_open();
    }

   private:
    boost::asio::io_context io_context_;
    tcp::socket socket_;
};

// Хелпер для поиска нужного ответа среди всех сообщений
nlohmann::json findAndReadResponse(
    TestClient& client, const std::string& expectedAction,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    auto messages = client.readAllMessages(timeout);
    for (const auto& msg : messages) {
        if (msg.contains("action") && msg["action"] == expectedAction) {
            return msg;
        }
    }
    // Если не нашли, возвращаем пустой JSON, что приведет к провалу теста
    return nlohmann::json{};
}

// ============================================================
// Фикстура: запущенный сервер
// ============================================================

class ServerTest : public ::testing::Test {
   protected:
    static constexpr uint16_t TEST_PORT_BASE = 19000;
    static std::atomic<uint16_t> port_counter;

    uint16_t port_;
    std::unique_ptr<Server> server_;
    std::shared_ptr<MessageHandler> handler_;
    std::thread server_thread_;

    void SetUp() override {
        port_ = TEST_PORT_BASE + port_counter.fetch_add(1);
        server_ = std::make_unique<Server>(port_, 4);  // Increased threads for concurrency tests
        handler_ = std::make_shared<MessageHandler>(*server_);
        server_->setMessageHandler(
            [h = handler_](std::shared_ptr<Session> s, const std::string& m) {
                h->handleMessage(s, m);
            });

        server_thread_ = std::thread([this]() { server_->run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        server_->stop();
        if (server_thread_.joinable())
            server_thread_.join();
    }

    std::unique_ptr<TestClient> createClient() {
        auto client = std::make_unique<TestClient>();
        client->connect(port_);
        return client;
    }

    std::string authenticateClient(TestClient& client, const std::string& nickname) {
        client.sendAuth(nickname);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
        while (std::chrono::steady_clock::now() < deadline) {
            auto msgs = client.readAllMessages(std::chrono::milliseconds(100));
            for (const auto& msg : msgs) {
                if (msg.contains("action") && msg["action"] == protocol::action::AUTH &&
                    msg.contains("payload") && msg["payload"].contains("success") &&
                    msg["payload"]["success"].get<bool>() && msg["payload"].contains("guid")) {
                    return msg["payload"]["guid"].get<std::string>();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return "";
    }
};

std::atomic<uint16_t> ServerTest::port_counter{0};

// ============================================================
// Тесты подключения
// ============================================================

TEST_F(ServerTest, ClientCanConnect) {
    auto client = createClient();
    EXPECT_TRUE(client->isConnected());
    client->disconnect();
}

TEST_F(ServerTest, MultipleClientsCanConnect) {
    const int count = 10;
    std::vector<std::unique_ptr<TestClient>> clients;

    for (int i = 0; i < count; ++i) {
        clients.push_back(createClient());
        EXPECT_TRUE(clients.back()->isConnected());
    }

    for (auto& c : clients)
        c->disconnect();
}

// ============================================================
// Тесты аутентификации
// ============================================================

TEST_F(ServerTest, Auth_ValidNickname_Success) {
    auto client = createClient();
    client->sendAuth("Player1");

    auto response = client->readJson();

    EXPECT_EQ(response["action"], protocol::action::AUTH);
    EXPECT_TRUE(response["payload"]["success"].get<bool>());
    EXPECT_FALSE(response["payload"]["guid"].get<std::string>().empty());
    EXPECT_EQ(response["payload"]["nickname"], "Player1");

    client->disconnect();
}

TEST_F(ServerTest, Auth_InvalidNickname_TooShort) {
    auto client = createClient();
    client->sendAuth("AB");

    auto response = client->readJson();

    EXPECT_FALSE(response["payload"]["success"].get<bool>());

    client->disconnect();
}

TEST_F(ServerTest, Auth_InvalidNickname_SpecialChars) {
    auto client = createClient();
    client->sendAuth("Player@#$");

    auto response = client->readJson();

    EXPECT_FALSE(response["payload"]["success"].get<bool>());

    client->disconnect();
}

TEST_F(ServerTest, Auth_EmptyNickname_Fails) {
    auto client = createClient();
    client->sendAuth("");

    auto response = client->readJson();

    EXPECT_FALSE(response["payload"]["success"].get<bool>());

    client->disconnect();
}

TEST_F(ServerTest, Auth_DuplicateNickname_Fails) {
    auto client1 = createClient();
    auto client2 = createClient();

    authenticateClient(*client1, "UniqueNick");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client2->readAllMessages(std::chrono::milliseconds(50));
    client2->sendAuth("UniqueNick");

    auto response = client2->readJson();
    EXPECT_FALSE(response["payload"]["success"].get<bool>());

    client1->disconnect();
    client2->disconnect();
}

TEST_F(ServerTest, Auth_Reconnect_WithGuid) {
    auto client1 = createClient();
    std::string guid = authenticateClient(*client1, "Reconnector");
    client1->disconnect();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto client2 = createClient();
    client2->sendAuth("Reconnector", guid);

    auto response = client2->readJson();
    EXPECT_TRUE(response["payload"]["success"].get<bool>());
    EXPECT_EQ(response["payload"]["guid"], guid);

    client2->disconnect();
}

// ============================================================
// Тесты запросов без аутентификации
// ============================================================

TEST_F(ServerTest, RequestWithoutAuth_ReturnsError) {
    auto client = createClient();
    client->sendGetPlayers();

    auto response = client->readJson();

    EXPECT_EQ(response["type"], protocol::message_type::RESPONSE);

    client->disconnect();
}

TEST_F(ServerTest, ShootWithoutAuth_ReturnsError) {
    auto client = createClient();
    client->sendShoot(5, 5);

    auto response = client->readJson();

    EXPECT_EQ(response["type"], protocol::message_type::RESPONSE);

    client->disconnect();
}

// ============================================================
// Тесты матчмейкинга
// ============================================================

TEST_F(ServerTest, FindGame_TwoPlayers_GameCreated) {
    auto client1 = createClient();
    auto client2 = createClient();

    authenticateClient(*client1, "Fighter1");
    authenticateClient(*client2, "Fighter2");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client1->sendFindGame();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    client2->sendFindGame();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto msgs1 = client1->readAllMessages();
    auto msgs2 = client2->readAllMessages();

    auto hasAction = [](const std::vector<nlohmann::json>& msgs, const std::string& action) {
        for (const auto& m : msgs) {
            if (m.contains("action") && m["action"] == action)
                return true;
        }
        return false;
    };

    EXPECT_TRUE(hasAction(msgs1, protocol::action::GAME_STARTED));
    EXPECT_TRUE(hasAction(msgs2, protocol::action::GAME_STARTED));

    client1->disconnect();
    client2->disconnect();
}

TEST_F(ServerTest, FindGame_SinglePlayer_NoGame) {
    auto client = createClient();
    authenticateClient(*client, "Lonely");

    client->sendFindGame();
    auto response = client->readJson();

    EXPECT_EQ(response["action"], protocol::action::FIND_GAME);

    client->disconnect();
}

TEST_F(ServerTest, CancelSearch_RemovesFromQueue) {
    auto client = createClient();
    authenticateClient(*client, "Canceller");

    client->sendFindGame();
    client->readJson();  // find_game response

    nlohmann::json cancelMsg;
    cancelMsg["type"] = protocol::message_type::REQUEST;
    cancelMsg["action"] = protocol::action::CANCEL_SEARCH;
    cancelMsg["payload"] = nlohmann::json::object();
    client->sendMessage(cancelMsg.dump());

    auto response = client->readJson();
    EXPECT_EQ(response["action"], protocol::action::CANCEL_SEARCH);

    client->disconnect();
}

// ============================================================
// Тесты полного игрового цикла
// ============================================================

TEST_F(ServerTest, FullGame_PlaceShips_Shoot_Surrender) {
    auto client1 = createClient();
    auto client2 = createClient();

    authenticateClient(*client1, "Warrior1");
    authenticateClient(*client2, "Warrior2");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client1->sendFindGame();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    client2->sendFindGame();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    client1->readAllMessages(std::chrono::milliseconds(200));
    client2->readAllMessages(std::chrono::milliseconds(200));

    client1->sendPlaceShipsRandom();
    client2->sendPlaceShipsRandom();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    client1->readAllMessages(std::chrono::milliseconds(300));
    client2->readAllMessages(std::chrono::milliseconds(300));

    client1->sendSurrender();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto msgs1 = client1->readAllMessages(std::chrono::milliseconds(500));
    auto msgs2 = client2->readAllMessages(std::chrono::milliseconds(500));

    auto hasAction = [](const std::vector<nlohmann::json>& msgs, const std::string& action) {
        for (const auto& m : msgs) {
            if (m.contains("action") && m["action"] == action)
                return true;
        }
        return false;
    };

    EXPECT_TRUE(hasAction(msgs1, protocol::action::GAME_OVER));
    EXPECT_TRUE(hasAction(msgs2, protocol::action::GAME_OVER));

    client1->disconnect();
    client2->disconnect();
}

// ============================================================
// Тесты приглашений
// ============================================================

TEST_F(ServerTest, Invite_SendAndAccept_GameCreated) {
    auto client1 = createClient();
    auto client2 = createClient();

    std::string guid1 = authenticateClient(*client1, "Inviter");
    std::string guid2 = authenticateClient(*client2, "Invitee");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Clear any "PLAYER_STATUS_CHANGED" messages
    client1->readAllMessages(std::chrono::milliseconds(100));
    client2->readAllMessages(std::chrono::milliseconds(100));

    client1->sendInvite(guid2);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto msgs2 = client2->readAllMessages(std::chrono::milliseconds(1000));
    std::string inviteId;
    for (const auto& m : msgs2) {
        if (m.contains("action") && m["action"] == protocol::action::INVITE_RECEIVED) {
            inviteId = m["payload"]["invite_id"].get<std::string>();
            break;
        }
    }
    ASSERT_FALSE(inviteId.empty()) << "Invite not received by invitee";

    client2->sendInviteResponse(inviteId, true);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto msgs1_after = client1->readAllMessages(std::chrono::milliseconds(500));
    auto msgs2_after = client2->readAllMessages(std::chrono::milliseconds(500));

    auto hasAction = [](const std::vector<nlohmann::json>& msgs, const std::string& action) {
        for (const auto& m : msgs) {
            if (m.contains("action") && m["action"] == action)
                return true;
        }
        return false;
    };

    EXPECT_TRUE(hasAction(msgs1_after, protocol::action::GAME_STARTED));
    EXPECT_TRUE(hasAction(msgs2_after, protocol::action::GAME_STARTED));

    client1->disconnect();
    client2->disconnect();
}

TEST_F(ServerTest, Invite_Decline) {
    auto client1 = createClient();
    auto client2 = createClient();

    std::string guid1 = authenticateClient(*client1, "Decliner1");
    std::string guid2 = authenticateClient(*client2, "Decliner2");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client1->readAllMessages(std::chrono::milliseconds(100));
    client2->readAllMessages(std::chrono::milliseconds(100));

    client1->sendInvite(guid2);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto msgs2 = client2->readAllMessages(std::chrono::milliseconds(1500));
    std::string inviteId;
    for (const auto& m : msgs2) {
        if (m.contains("action") && m["action"] == protocol::action::INVITE_RECEIVED) {
            inviteId = m["payload"]["invite_id"].get<std::string>();
        }
    }
    ASSERT_FALSE(inviteId.empty());

    client2->sendInviteResponse(inviteId, false);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(server_->getGameManager()->getActiveGameCount(), 0u);

    client1->disconnect();
    client2->disconnect();
}

// ============================================================
// Тесты друзей
// ============================================================

TEST_F(ServerTest, AddFriend_Success) {
    auto client1 = createClient();
    auto client2 = createClient();

    std::string guid1 = authenticateClient(*client1, "Friend1");
    std::string guid2 = authenticateClient(*client2, "Friend2");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    client1->readAllMessages(std::chrono::milliseconds(100));  // Clear broadcast messages

    client1->sendAddFriend(guid2);

    auto response = findAndReadResponse(*client1, protocol::action::ADD_FRIEND,
                                        std::chrono::milliseconds(3000));
    ASSERT_FALSE(response.is_null()) << "Response for ADD_FRIEND not found";
    EXPECT_TRUE(response["payload"]["success"]);

    client1->disconnect();
    client2->disconnect();
}

// ============================================================
// Тесты статистики
// ============================================================

TEST_F(ServerTest, GetStats_OwnStats) {
    auto client = createClient();
    std::string guid = authenticateClient(*client, "StatsGuy");
    client->readAllMessages(std::chrono::milliseconds(100));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client->sendGetStats(guid);
    auto response =
        findAndReadResponse(*client, protocol::action::GET_STATS, std::chrono::milliseconds(2000));
    ASSERT_FALSE(response.is_null()) << "Response for GET_STATS not found";

    EXPECT_TRUE(response["payload"].contains("games_played"));
    EXPECT_TRUE(response["payload"].contains("wins"));
    EXPECT_TRUE(response["payload"].contains("losses"));

    client->disconnect();
}

// ============================================================
// Тесты отключения во время игры
// ============================================================

TEST_F(ServerTest, Disconnect_DuringGame_OpponentWins) {
    auto client1 = createClient();
    auto client2 = createClient();

    authenticateClient(*client1, "Disconnecter");
    authenticateClient(*client2, "Stayer");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client1->sendFindGame();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    client2->sendFindGame();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    client1->readAllMessages(std::chrono::milliseconds(200));
    client2->readAllMessages(std::chrono::milliseconds(200));

    client1->sendPlaceShipsRandom();
    client2->sendPlaceShipsRandom();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    client2->readAllMessages(std::chrono::milliseconds(200));

    client1->disconnect();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto msgs2 = client2->readAllMessages(std::chrono::milliseconds(1000));

    auto hasAction = [](const std::vector<nlohmann::json>& msgs, const std::string& action) {
        for (const auto& m : msgs) {
            if (m.contains("action") && m["action"] == action)
                return true;
        }
        return false;
    };

    EXPECT_TRUE(hasAction(msgs2, protocol::action::GAME_OVER));

    client2->disconnect();
}

// ============================================================
// Тесты нагрузки
// ============================================================

TEST_F(ServerTest, ManyClients_ConnectAuthDisconnect) {
    const int count = 20;
    int success = 0;

    for (int i = 0; i < count; ++i) {
        try {
            TestClient client;
            client.connect(port_);
            client.sendAuth("Batch" + std::to_string(i));
            auto response = client.readJson();
            if (response["payload"]["success"].get<bool>()) {
                success++;
            }
            client.disconnect();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } catch (...) {
        }
    }
    EXPECT_EQ(success, count);
}

TEST_F(ServerTest, RapidConnectDisconnect_NoServerCrash) {
    const int count = 50;

    for (int i = 0; i < count; ++i) {
        try {
            TestClient client;
            client.connect(port_);
            client.disconnect();
        } catch (...) {
        }
    }

    auto client = createClient();
    std::string guid = authenticateClient(*client, "StillAlive");
    EXPECT_FALSE(guid.empty());

    client->disconnect();
}

TEST_F(ServerTest, ConnectAndDisconnectImmediately_NoLeak) {
    const int count = 30;
    std::vector<std::thread> threads;

    for (int i = 0; i < count; ++i) {
        threads.emplace_back([this]() {
            try {
                TestClient client;
                client.connect(port_);
                client.disconnect();
            } catch (...) {
            }
        });
    }

    for (auto& t : threads)
        t.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_LE(server_->getSessionManager()->getSessionCount(), 1u);
}

// ============================================================
// Тесты некорректных сообщений
// ============================================================

TEST_F(ServerTest, InvalidJson_ServerHandlesGracefully) {
    auto client = createClient();

    auto packed = MessageParser::packMessage("not a json {{{");
    TestClient rawClient;
    rawClient.connect(port_);
    rawClient.sendMessage("not a json {{{");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto client2 = createClient();
    std::string guid = authenticateClient(*client2, "AfterInvalid");
    EXPECT_FALSE(guid.empty());

    rawClient.disconnect();
    client->disconnect();
    client2->disconnect();
}

TEST_F(ServerTest, UnknownAction_ServerRespondsWithError) {
    auto client = createClient();
    authenticateClient(*client, "UnknownSender");

    nlohmann::json j;
    j["type"] = protocol::message_type::REQUEST;
    j["action"] = "nonexistent_action";
    j["payload"] = nlohmann::json::object();
    client->sendMessage(j.dump());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto client2 = createClient();
    EXPECT_TRUE(client2->isConnected());

    client->disconnect();
    client2->disconnect();
}

TEST_F(ServerTest, MissingPayload_ServerHandlesGracefully) {
    auto client = createClient();
    authenticateClient(*client, "NoPayload");

    nlohmann::json j;
    j["type"] = protocol::message_type::REQUEST;
    j["action"] = protocol::action::SHOOT;
    client->sendMessage(j.dump());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    SUCCEED();

    client->disconnect();
}

// ============================================================
// Тесты граничных случаев игры
// ============================================================

TEST_F(ServerTest, ShootWithoutGame_ReturnsError) {
    auto client = createClient();
    authenticateClient(*client, "NoGame");

    client->sendShoot(0, 0);

    auto msgs = client->readAllMessages(std::chrono::milliseconds(500));

    client->disconnect();
}

TEST_F(ServerTest, SurrenderWithoutGame_NoServerCrash) {
    auto client = createClient();
    authenticateClient(*client, "FakeSurrender");

    client->sendSurrender();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    SUCCEED();

    client->disconnect();
}

TEST_F(ServerTest, PlaceShipsWithoutGame_ReturnsError) {
    auto client = createClient();
    authenticateClient(*client, "NoGamePlacer");

    client->sendPlaceShipsRandom();

    auto msgs = client->readAllMessages(std::chrono::milliseconds(500));

    client->disconnect();
}

// ============================================================
// Тесты состояний игрока
// ============================================================

TEST_F(ServerTest, PlayerStatus_ChangesOnEvents) {
    auto client1 = createClient();
    auto client2 = createClient();
    auto observer = createClient();

    authenticateClient(*observer, "Observer");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    observer->readAllMessages(std::chrono::milliseconds(100));

    authenticateClient(*client1, "StatusP1");
    authenticateClient(*client2, "StatusP2");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto msgs = observer->readAllMessages(std::chrono::milliseconds(500));

    bool foundStatusChange = false;
    for (const auto& m : msgs) {
        if (m.contains("action") && m["action"] == protocol::action::PLAYER_STATUS_CHANGED) {
            foundStatusChange = true;
        }
    }
    EXPECT_TRUE(foundStatusChange);

    client1->disconnect();
    client2->disconnect();
    observer->disconnect();
}

}  // namespace test
}  // namespace seabattle

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    seabattle::Logger::init("seabattle_tests", spdlog::level::trace);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}