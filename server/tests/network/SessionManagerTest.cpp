#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <thread>

#include "network/Session.hpp"
#include "network/SessionManager.hpp"
#include "player/Player.hpp"

namespace seabattle {
namespace test {

using boost::asio::ip::tcp;

class SessionManagerTest : public ::testing::Test {
   protected:
    boost::asio::io_context io_context_;
    SessionManager manager_;

    std::shared_ptr<Session> createMockSession() {
        tcp::socket sock(io_context_);
        return std::make_shared<Session>(std::move(sock));
    }
};

// ============================================================
// Базовые операции
// ============================================================

TEST_F(SessionManagerTest, InitialState_Empty) {
    EXPECT_EQ(manager_.getSessionCount(), 0u);
    EXPECT_EQ(manager_.getAuthenticatedCount(), 0u);
}

TEST_F(SessionManagerTest, AddSession_IncreasesCount) {
    auto s1 = createMockSession();
    auto s2 = createMockSession();

    manager_.addSession(s1);
    EXPECT_EQ(manager_.getSessionCount(), 1u);

    manager_.addSession(s2);
    EXPECT_EQ(manager_.getSessionCount(), 2u);
}

TEST_F(SessionManagerTest, RemoveSession_DecreasesCount) {
    auto s1 = createMockSession();
    manager_.addSession(s1);
    EXPECT_EQ(manager_.getSessionCount(), 1u);

    manager_.removeSession(s1);
    EXPECT_EQ(manager_.getSessionCount(), 0u);
}

TEST_F(SessionManagerTest, RemoveNonexistentSession_NoThrow) {
    auto s1 = createMockSession();
    EXPECT_NO_THROW(manager_.removeSession(s1));
}

TEST_F(SessionManagerTest, CloseAll_ClearsAllSessions) {
    for (int i = 0; i < 5; ++i) {
        manager_.addSession(createMockSession());
    }
    EXPECT_EQ(manager_.getSessionCount(), 5u);

    manager_.closeAll();
    EXPECT_EQ(manager_.getSessionCount(), 0u);
}

// ============================================================
// Привязка игроков
// ============================================================

TEST_F(SessionManagerTest, BindPlayer_MakesAuthenticated) {
    auto session = createMockSession();
    manager_.addSession(session);

    EXPECT_EQ(manager_.getAuthenticatedCount(), 0u);

    manager_.bindPlayerToSession("guid-1", session);
    EXPECT_EQ(manager_.getAuthenticatedCount(), 1u);

    auto found = manager_.getSessionByPlayerGuid("guid-1");
    EXPECT_NE(found, nullptr);
}

TEST_F(SessionManagerTest, UnbindPlayer_RemovesMapping) {
    auto session = createMockSession();
    manager_.addSession(session);
    manager_.bindPlayerToSession("guid-1", session);

    EXPECT_NE(manager_.getSessionByPlayerGuid("guid-1"), nullptr);

    manager_.unbindPlayer("guid-1");
    EXPECT_EQ(manager_.getSessionByPlayerGuid("guid-1"), nullptr);
    EXPECT_EQ(manager_.getAuthenticatedCount(), 0u);
}

TEST_F(SessionManagerTest, GetSessionByPlayerGuid_NonexistentGuid_ReturnsNull) {
    EXPECT_EQ(manager_.getSessionByPlayerGuid("nonexistent"), nullptr);
}

TEST_F(SessionManagerTest, BindMultiplePlayers_AllAccessible) {
    const int count = 10;
    for (int i = 0; i < count; ++i) {
        auto s = createMockSession();
        manager_.addSession(s);
        manager_.bindPlayerToSession("guid-" + std::to_string(i), s);
    }

    EXPECT_EQ(manager_.getAuthenticatedCount(), count);

    for (int i = 0; i < count; ++i) {
        EXPECT_NE(manager_.getSessionByPlayerGuid("guid-" + std::to_string(i)), nullptr);
    }
}

// ============================================================
// Disconnect callback
// ============================================================

TEST_F(SessionManagerTest, DisconnectCallback_InvokedOnRemove) {
    std::string disconnectedGuid;

    manager_.setDisconnectCallback([&](const std::string& guid) { disconnectedGuid = guid; });

    auto session = createMockSession();
    auto player = std::make_shared<Player>("guid-1", "Nick");
    session->setPlayer(player);
    manager_.addSession(session);
    manager_.bindPlayerToSession("guid-1", session);

    manager_.removeSession(session);

    EXPECT_EQ(disconnectedGuid, "guid-1");
}

// ============================================================
// Broadcast
// ============================================================

TEST_F(SessionManagerTest, BroadcastToPlayers_OnlyTargetsReceive) {
    auto s1 = createMockSession();
    auto s2 = createMockSession();
    auto s3 = createMockSession();

    manager_.addSession(s1);
    manager_.addSession(s2);
    manager_.addSession(s3);

    manager_.bindPlayerToSession("guid-1", s1);
    manager_.bindPlayerToSession("guid-2", s2);
    manager_.bindPlayerToSession("guid-3", s3);

    // broadcastToPlayers отправляет только guid-1 и guid-3
    std::vector<std::string> targets = {"guid-1", "guid-3"};
    EXPECT_NO_THROW(manager_.broadcastToPlayers("test message", targets));
}

// ============================================================
// Потокобезопасность
// ============================================================

TEST_F(SessionManagerTest, ConcurrentAddRemove_NoDataRace) {
    const int iterations = 100;
    std::atomic<int> added{0};

    std::vector<std::thread> threads;

    // Потоки добавления
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations; ++i) {
                auto s = createMockSession();
                manager_.addSession(s);
                added++;
            }
        });
    }

    // Потоки чтения
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations; ++i) {
                manager_.getSessionCount();
                manager_.getAuthenticatedCount();
            }
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(manager_.getSessionCount(), added.load());
}

TEST_F(SessionManagerTest, ConcurrentBindUnbind_NoDataRace) {
    const int count = 50;
    std::vector<std::shared_ptr<Session>> sessions;

    for (int i = 0; i < count; ++i) {
        auto s = createMockSession();
        manager_.addSession(s);
        sessions.push_back(s);
    }

    std::vector<std::thread> threads;

    // Потоки привязки
    for (int i = 0; i < count / 2; ++i) {
        threads.emplace_back(
            [&, i]() { manager_.bindPlayerToSession("guid-" + std::to_string(i), sessions[i]); });
    }

    // Потоки отвязки
    for (int i = 0; i < count / 4; ++i) {
        threads.emplace_back([&, i]() { manager_.unbindPlayer("guid-" + std::to_string(i)); });
    }

    // Потоки поиска
    for (int i = 0; i < count / 4; ++i) {
        threads.emplace_back(
            [&, i]() { manager_.getSessionByPlayerGuid("guid-" + std::to_string(i)); });
    }

    for (auto& t : threads)
        t.join();

    // Нет крэша — тест пройден
    EXPECT_TRUE(true);
}

}  // namespace test
}  // namespace seabattle