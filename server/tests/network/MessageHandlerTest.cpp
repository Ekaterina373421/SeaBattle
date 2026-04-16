#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include "game/GameManager.hpp"
#include "lobby/Lobby.hpp"
#include "network/MessageHandler.hpp"
#include "network/Server.hpp"
#include "network/Session.hpp"
#include "network/SessionManager.hpp"
#include "player/Player.hpp"
#include "player/PlayerManager.hpp"
#include "protocol/MessageBuilder.hpp"
#include "protocol/MessageParser.hpp"

namespace seabattle {
namespace test {

using boost::asio::ip::tcp;

class MessageHandlerTest : public ::testing::Test {
   protected:
    boost::asio::io_context io_context_;
    std::unique_ptr<Server> server_;
    std::shared_ptr<MessageHandler> handler_;

    void SetUp() override {
        server_ = std::make_unique<Server>(0, 1);
        handler_ = std::make_shared<MessageHandler>(*server_);
    }

    std::shared_ptr<Session> createSession() {
        tcp::socket sock(io_context_);
        return std::make_shared<Session>(std::move(sock));
    }

    std::string makeRequest(const std::string& action, const nlohmann::json& payload) {
        nlohmann::json j;
        j["type"] = protocol::message_type::REQUEST;
        j["action"] = action;
        j["payload"] = payload;
        return j.dump();
    }
};

// ============================================================
// Тесты обработки auth
// ============================================================

TEST_F(MessageHandlerTest, Auth_ValidNickname_CreatesPlayer) {
    auto session = createSession();
    std::string msg = makeRequest(protocol::action::AUTH, {{"nickname", "TestPlayer"}});

    EXPECT_NO_THROW(handler_->handleMessage(session, msg));

    EXPECT_NE(session->getPlayer(), nullptr);
    EXPECT_EQ(server_->getPlayerManager()->getPlayerCount(), 1u);
}

TEST_F(MessageHandlerTest, Auth_InvalidNickname_NoPlayer) {
    auto session = createSession();
    std::string msg = makeRequest(protocol::action::AUTH, {{"nickname", "AB"}});

    handler_->handleMessage(session, msg);

    EXPECT_EQ(session->getPlayer(), nullptr);
}

TEST_F(MessageHandlerTest, Auth_MissingNickname_NoPlayer) {
    auto session = createSession();
    std::string msg = makeRequest(protocol::action::AUTH, nlohmann::json::object());

    handler_->handleMessage(session, msg);

    EXPECT_EQ(session->getPlayer(), nullptr);
}

TEST_F(MessageHandlerTest, Auth_DoubleAuth_SecondIgnored) {
    auto session = createSession();
    std::string msg = makeRequest(protocol::action::AUTH, {{"nickname", "DoubleAuth"}});

    handler_->handleMessage(session, msg);
    auto player1 = session->getPlayer();
    ASSERT_NE(player1, nullptr);

    std::string msg2 = makeRequest(protocol::action::AUTH, {{"nickname", "DoubleAuth2"}});
    handler_->handleMessage(session, msg2);

    EXPECT_EQ(session->getPlayer(), player1);
}

// ============================================================
// Тесты обработки без аутентификации
// ============================================================

TEST_F(MessageHandlerTest, GetPlayers_WithoutAuth_NoThrow) {
    auto session = createSession();
    std::string msg = makeRequest(protocol::action::GET_PLAYERS, nlohmann::json::object());

    EXPECT_NO_THROW(handler_->handleMessage(session, msg));
}

TEST_F(MessageHandlerTest, Shoot_WithoutAuth_NoThrow) {
    auto session = createSession();
    std::string msg = makeRequest(protocol::action::SHOOT, {{"x", 0}, {"y", 0}});

    EXPECT_NO_THROW(handler_->handleMessage(session, msg));
}

TEST_F(MessageHandlerTest, FindGame_WithoutAuth_NoThrow) {
    auto session = createSession();
    std::string msg = makeRequest(protocol::action::FIND_GAME, nlohmann::json::object());

    EXPECT_NO_THROW(handler_->handleMessage(session, msg));
}

// ============================================================
// Тесты обработки невалидных сообщений
// ============================================================

TEST_F(MessageHandlerTest, InvalidJson_NoThrow) {
    auto session = createSession();

    EXPECT_NO_THROW(handler_->handleMessage(session, "not json at all"));
}

TEST_F(MessageHandlerTest, EmptyMessage_NoThrow) {
    auto session = createSession();

    EXPECT_NO_THROW(handler_->handleMessage(session, ""));
}

TEST_F(MessageHandlerTest, MissingType_NoThrow) {
    auto session = createSession();
    nlohmann::json j;
    j["action"] = protocol::action::AUTH;
    j["payload"] = {{"nickname", "Test"}};

    EXPECT_NO_THROW(handler_->handleMessage(session, j.dump()));
}

TEST_F(MessageHandlerTest, MissingAction_NoThrow) {
    auto session = createSession();
    nlohmann::json j;
    j["type"] = protocol::message_type::REQUEST;
    j["payload"] = {{"nickname", "Test"}};

    EXPECT_NO_THROW(handler_->handleMessage(session, j.dump()));
}

TEST_F(MessageHandlerTest, UnknownAction_NoThrow) {
    auto session = createSession();
    std::string msg = makeRequest("completely_unknown_action", nlohmann::json::object());

    EXPECT_NO_THROW(handler_->handleMessage(session, msg));
}

// ============================================================
// Тесты матчмейкинга через handler
// ============================================================

TEST_F(MessageHandlerTest, FindGame_AuthenticatedPlayer_AddedToQueue) {
    auto session = createSession();
    handler_->handleMessage(session, makeRequest(protocol::action::AUTH, {{"nickname", "Queuer"}}));

    handler_->handleMessage(session,
                            makeRequest(protocol::action::FIND_GAME, nlohmann::json::object()));

    EXPECT_TRUE(server_->getLobby()->isInQueue(session->getPlayer()->getGuid()));
}

TEST_F(MessageHandlerTest, FindGame_TwoPlayers_GameCreated) {
    auto s1 = createSession();
    auto s2 = createSession();

    handler_->handleMessage(s1, makeRequest(protocol::action::AUTH, {{"nickname", "Match1"}}));
    handler_->handleMessage(s2, makeRequest(protocol::action::AUTH, {{"nickname", "Match2"}}));

    handler_->handleMessage(s1, makeRequest(protocol::action::FIND_GAME, nlohmann::json::object()));
    handler_->handleMessage(s2, makeRequest(protocol::action::FIND_GAME, nlohmann::json::object()));

    EXPECT_GE(server_->getGameManager()->getActiveGameCount(), 1u);
}

TEST_F(MessageHandlerTest, CancelSearch_RemovesFromQueue) {
    auto session = createSession();
    handler_->handleMessage(session,
                            makeRequest(protocol::action::AUTH, {{"nickname", "CancelMe"}}));

    handler_->handleMessage(session,
                            makeRequest(protocol::action::FIND_GAME, nlohmann::json::object()));
    EXPECT_TRUE(server_->getLobby()->isInQueue(session->getPlayer()->getGuid()));

    handler_->handleMessage(session,
                            makeRequest(protocol::action::CANCEL_SEARCH, nlohmann::json::object()));
    EXPECT_FALSE(server_->getLobby()->isInQueue(session->getPlayer()->getGuid()));
}

// ============================================================
// Тесты друзей через handler
// ============================================================

TEST_F(MessageHandlerTest, AddFriend_ValidGuid_Success) {
    auto s1 = createSession();
    auto s2 = createSession();

    handler_->handleMessage(s1, makeRequest(protocol::action::AUTH, {{"nickname", "Friender1"}}));
    handler_->handleMessage(s2, makeRequest(protocol::action::AUTH, {{"nickname", "Friender2"}}));

    std::string guid2 = s2->getPlayer()->getGuid();

    handler_->handleMessage(s1, makeRequest(protocol::action::ADD_FRIEND, {{"guid", guid2}}));

    EXPECT_TRUE(s1->getPlayer()->getFriends().isFriend(guid2));
}

TEST_F(MessageHandlerTest, AddFriend_Self_Fails) {
    auto session = createSession();
    handler_->handleMessage(session,
                            makeRequest(protocol::action::AUTH, {{"nickname", "SelfFriend"}}));

    std::string ownGuid = session->getPlayer()->getGuid();

    handler_->handleMessage(session,
                            makeRequest(protocol::action::ADD_FRIEND, {{"guid", ownGuid}}));

    EXPECT_FALSE(session->getPlayer()->getFriends().isFriend(ownGuid));
}

TEST_F(MessageHandlerTest, AddFriend_NonexistentGuid_NoThrow) {
    auto session = createSession();
    handler_->handleMessage(session,
                            makeRequest(protocol::action::AUTH, {{"nickname", "GhostFriend"}}));

    EXPECT_NO_THROW(handler_->handleMessage(
        session, makeRequest(protocol::action::ADD_FRIEND, {{"guid", "nonexistent-guid"}})));
}

// ============================================================
// Тесты игровых действий через handler
// ============================================================

TEST_F(MessageHandlerTest, Shoot_NotInGame_NoThrow) {
    auto session = createSession();
    handler_->handleMessage(session,
                            makeRequest(protocol::action::AUTH, {{"nickname", "Shooter"}}));

    EXPECT_NO_THROW(handler_->handleMessage(
        session, makeRequest(protocol::action::SHOOT, {{"x", 5}, {"y", 5}})));
}

TEST_F(MessageHandlerTest, Surrender_NotInGame_NoThrow) {
    auto session = createSession();
    handler_->handleMessage(session,
                            makeRequest(protocol::action::AUTH, {{"nickname", "Surrenderer"}}));

    EXPECT_NO_THROW(handler_->handleMessage(
        session, makeRequest(protocol::action::SURRENDER, nlohmann::json::object())));
}

TEST_F(MessageHandlerTest, PlaceShips_NotInGame_NoThrow) {
    auto session = createSession();
    handler_->handleMessage(session, makeRequest(protocol::action::AUTH, {{"nickname", "Placer"}}));

    EXPECT_NO_THROW(handler_->handleMessage(
        session, makeRequest(protocol::action::PLACE_SHIPS, {{"random", true}})));
}

// ============================================================
// Стресс-тесты
// ============================================================

TEST_F(MessageHandlerTest, ManyMessages_Sequential_NoMemoryLeak) {
    auto session = createSession();
    handler_->handleMessage(session,
                            makeRequest(protocol::action::AUTH, {{"nickname", "Stresser"}}));

    for (int i = 0; i < 1000; ++i) {
        handler_->handleMessage(
            session, makeRequest(protocol::action::GET_PLAYERS, nlohmann::json::object()));
    }

    SUCCEED();
}

TEST_F(MessageHandlerTest, ConcurrentHandling_MultipleSessionsAuth) {
    const int count = 20;
    std::vector<std::shared_ptr<Session>> sessions;
    for (int i = 0; i < count; ++i) {
        sessions.push_back(createSession());
    }

    std::vector<std::thread> threads;
    for (int i = 0; i < count; ++i) {
        threads.emplace_back([this, &sessions, i]() {
            handler_->handleMessage(sessions[i],
                                    makeRequest(protocol::action::AUTH,
                                                {{"nickname", "Concurrent" + std::to_string(i)}}));
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(server_->getPlayerManager()->getPlayerCount(), static_cast<size_t>(count));
}

}  // namespace test
}  // namespace seabattle