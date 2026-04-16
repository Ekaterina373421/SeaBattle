#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "game/Board.hpp"
#include "game/Game.hpp"
#include "game/GameManager.hpp"
#include "lobby/InviteManager.hpp"
#include "lobby/Lobby.hpp"
#include "lobby/Matchmaker.hpp"
#include "player/Player.hpp"
#include "player/PlayerManager.hpp"

namespace seabattle {
namespace test {

// ============================================================
// Потокобезопасность PlayerManager
// ============================================================

class PlayerManagerThreadTest : public ::testing::Test {
   protected:
    PlayerManager manager_;
};

TEST_F(PlayerManagerThreadTest, ConcurrentCreate_AllPlayersCreated) {
    const int threadCount = 10;
    const int playersPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> created{0};

    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < playersPerThread; ++i) {
                auto p = manager_.createPlayer("Thread" + std::to_string(t) + "_Player" +
                                               std::to_string(i));
                if (p)
                    created++;
            }
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(created.load(), threadCount * playersPerThread);
    EXPECT_EQ(manager_.getPlayerCount(), threadCount * playersPerThread);
}

TEST_F(PlayerManagerThreadTest, ConcurrentReadWrite_NoCorruption) {
    // Предсоздаём игроков
    std::vector<std::string> guids;
    for (int i = 0; i < 20; ++i) {
        auto p = manager_.createPlayer("Pre" + std::to_string(i));
        guids.push_back(p->getGuid());
    }

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Писатели: создают новых игроков
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([&, t]() {
            int i = 0;
            while (!stop) {
                manager_.createPlayer("Writer" + std::to_string(t) + "_" + std::to_string(i++));
            }
        });
    }

    // Читатели: запрашивают игроков
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([&]() {
            while (!stop) {
                for (const auto& guid : guids) {
                    auto p = manager_.getPlayer(guid);
                    if (p) {
                        // Читаем данные — не должно быть крэша
                        p->getNickname();
                        p->getStatus();
                    }
                }
                manager_.getOnlinePlayers();
                manager_.getPlayerCount();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop = true;

    for (auto& t : threads)
        t.join();

    EXPECT_GT(manager_.getPlayerCount(), 20u);
}

TEST_F(PlayerManagerThreadTest, ConcurrentOnlineOffline_Consistent) {
    const int count = 30;
    std::vector<std::shared_ptr<Player>> players;

    for (int i = 0; i < count; ++i) {
        players.push_back(manager_.createPlayer("Toggle" + std::to_string(i)));
    }

    std::vector<std::thread> threads;

    // Потоки переключают online/offline
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int round = 0; round < 100; ++round) {
                int idx = (t * 100 + round) % count;
                if (round % 2 == 0) {
                    manager_.setPlayerOnline(players[idx]->getGuid(), nullptr);
                } else {
                    manager_.setPlayerOffline(players[idx]->getGuid());
                }
            }
        });
    }

    // Потоки читают счётчики
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 200; ++i) {
                size_t online = manager_.getOnlineCount();
                size_t total = manager_.getPlayerCount();
                EXPECT_LE(online, total);
            }
        });
    }

    for (auto& t : threads)
        t.join();
}

// ============================================================
// Потокобезопасность GameManager
// ============================================================

class GameManagerThreadTest : public ::testing::Test {
   protected:
    PlayerManager playerManager_;
    std::shared_ptr<GameManager> gameManager_;

    void SetUp() override {
        gameManager_ = std::make_shared<GameManager>();
    }
};

TEST_F(GameManagerThreadTest, ConcurrentCreateAndQuery_NoCorruption) {
    const int gameCount = 20;
    std::vector<std::pair<std::shared_ptr<Player>, std::shared_ptr<Player>>> playerPairs;

    for (int i = 0; i < gameCount; ++i) {
        auto p1 = playerManager_.createPlayer("GP1_" + std::to_string(i));
        auto p2 = playerManager_.createPlayer("GP2_" + std::to_string(i));
        p1->setStatus(PlayerStatus::Online);
        p2->setStatus(PlayerStatus::Online);
        playerPairs.emplace_back(p1, p2);
    }

    std::vector<std::string> gameIds(gameCount);
    std::vector<std::thread> threads;

    // Создаём игры параллельно
    for (int i = 0; i < gameCount; ++i) {
        threads.emplace_back([&, i]() {
            auto game = gameManager_->createGame(playerPairs[i].first, playerPairs[i].second);
            if (game)
                gameIds[i] = game->getId();
        });
    }

    for (auto& t : threads)
        t.join();
    threads.clear();

    // Параллельно запрашиваем
    std::atomic<int> found{0};
    for (int i = 0; i < gameCount; ++i) {
        threads.emplace_back([&, i]() {
            if (!gameIds[i].empty()) {
                auto game = gameManager_->getGame(gameIds[i]);
                if (game)
                    found++;
            }
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_GT(found.load(), 0);
}

TEST_F(GameManagerThreadTest, ConcurrentCreateAndRemove_NoDeadlock) {
    const int count = 15;
    std::vector<std::string> gameIds;
    std::mutex idsMutex;

    std::vector<std::thread> threads;

    // Создатели
    for (int i = 0; i < count; ++i) {
        threads.emplace_back([&, i]() {
            auto p1 = playerManager_.createPlayer("CR1_" + std::to_string(i));
            auto p2 = playerManager_.createPlayer("CR2_" + std::to_string(i));
            p1->setStatus(PlayerStatus::Online);
            p2->setStatus(PlayerStatus::Online);

            auto game = gameManager_->createGame(p1, p2);
            if (game) {
                std::lock_guard<std::mutex> lock(idsMutex);
                gameIds.push_back(game->getId());
            }
        });
    }

    for (auto& t : threads)
        t.join();
    threads.clear();

    // Удалители
    for (const auto& id : gameIds) {
        threads.emplace_back([&, id]() { gameManager_->removeGame(id); });
    }

    // Одновременно читатели
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                gameManager_->getActiveGameCount();
                gameManager_->getActiveGameIds();
            }
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(gameManager_->getActiveGameCount(), 0u);
}

// ============================================================
// Потокобезопасность Matchmaker
// ============================================================

class MatchmakerThreadTest : public ::testing::Test {
   protected:
    Matchmaker matchmaker_;
    PlayerManager playerManager_;
};

TEST_F(MatchmakerThreadTest, ConcurrentAddAndMatch_NoLostPlayers) {
    const int playerCount = 40;
    std::vector<std::shared_ptr<Player>> players;

    for (int i = 0; i < playerCount; ++i) {
        auto p = playerManager_.createPlayer("MM" + std::to_string(i));
        p->setStatus(PlayerStatus::Online);
        players.push_back(p);
    }

    std::vector<std::thread> threads;

    // Добавляем игроков параллельно
    for (int i = 0; i < playerCount; ++i) {
        threads.emplace_back([&, i]() { matchmaker_.addPlayer(players[i]); });
    }

    for (auto& t : threads)
        t.join();

    // Матчим параллельно
    std::atomic<int> matched{0};
    threads.clear();

    for (int i = 0; i < playerCount / 2; ++i) {
        threads.emplace_back([&]() {
            auto pair = matchmaker_.tryMatch();
            if (pair.has_value())
                matched++;
        });
    }

    for (auto& t : threads)
        t.join();

    // Все были сматчены или остались в очереди
    size_t remaining = matchmaker_.getQueueSize();
    EXPECT_EQ(matched.load() * 2 + remaining, playerCount);
}

// ============================================================
// Потокобезопасность InviteManager
// ============================================================

class InviteManagerThreadTest : public ::testing::Test {
   protected:
    InviteManager inviteManager_;
};

TEST_F(InviteManagerThreadTest, ConcurrentCreateAcceptDecline_NoCorruption) {
    const int count = 20;
    std::vector<std::string> inviteIds(count);
    std::mutex idsMutex;

    std::vector<std::thread> threads;

    // Создаём приглашения параллельно
    for (int i = 0; i < count; ++i) {
        threads.emplace_back([&, i]() {
            std::string id =
                inviteManager_.createInvite("from_" + std::to_string(i), "to_" + std::to_string(i));
            std::lock_guard<std::mutex> lock(idsMutex);
            inviteIds[i] = id;
        });
    }

    for (auto& t : threads)
        t.join();
    threads.clear();

    // Половину принимаем, половину отклоняем — параллельно
    for (int i = 0; i < count; ++i) {
        threads.emplace_back([&, i]() {
            if (!inviteIds[i].empty()) {
                if (i % 2 == 0) {
                    inviteManager_.acceptInvite(inviteIds[i]);
                } else {
                    inviteManager_.declineInvite(inviteIds[i]);
                }
            }
        });
    }

    for (auto& t : threads)
        t.join();

    // Все приглашения обработаны
    for (int i = 0; i < count; ++i) {
        if (!inviteIds[i].empty()) {
            auto invite = inviteManager_.getInvite(inviteIds[i]);
            EXPECT_FALSE(invite.has_value()) << "Invite " << i << " should be removed";
        }
    }
}

// ============================================================
// Потокобезопасность Game
// ============================================================

class GameThreadTest : public ::testing::Test {
   protected:
    PlayerManager playerManager_;
};

TEST_F(GameThreadTest, ConcurrentShots_NoCorruption) {
    auto p1 = playerManager_.createPlayer("Shooter1");
    auto p2 = playerManager_.createPlayer("Shooter2");
    p1->setStatus(PlayerStatus::Online);
    p2->setStatus(PlayerStatus::Online);

    auto game = std::make_shared<Game>("test-game", p1, p2);
    game->placeShipsRandomly(p1->getGuid());
    game->placeShipsRandomly(p2->getGuid());
    game->startBattle();

    std::string currentTurn = game->getCurrentTurnGuid();

    // Множество потоков пытаются стрелять одновременно
    std::vector<std::thread> threads;
    std::atomic<int> successfulShots{0};
    std::atomic<int> failedShots{0};

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 25; ++i) {
                int x = (t * 25 + i) % 10;
                int y = (t * 25 + i) / 10 % 10;
                try {
                    auto result = game->shoot(currentTurn, x, y);
                    if (result.result != ShotResult::InvalidCoordinates &&
                        result.result != ShotResult::AlreadyShot) {
                        successfulShots++;
                    }
                } catch (...) {
                    failedShots++;
                }
            }
        });
    }

    for (auto& t : threads)
        t.join();

    // Не должно быть крэшей, состояние игры должно быть консистентным
    EXPECT_TRUE(game->getState() == GameState::Battle || game->getState() == GameState::Finished);
}

TEST_F(GameThreadTest, ConcurrentPlaceShips_NoCorruption) {
    auto p1 = playerManager_.createPlayer("Placer1");
    auto p2 = playerManager_.createPlayer("Placer2");
    p1->setStatus(PlayerStatus::Online);
    p2->setStatus(PlayerStatus::Online);

    auto game = std::make_shared<Game>("place-test", p1, p2);

    std::thread t1([&]() { game->placeShipsRandomly(p1->getGuid()); });

    std::thread t2([&]() { game->placeShipsRandomly(p2->getGuid()); });

    t1.join();
    t2.join();

    EXPECT_TRUE(game->isPlayerReady(p1->getGuid()));
    EXPECT_TRUE(game->isPlayerReady(p2->getGuid()));
}

// ============================================================
// Потокобезопасность Board (пограничный — Board не thread-safe,
// но Game защищает мьютексом — проверяем через Game)
// ============================================================

TEST_F(GameThreadTest, ConcurrentReadBoardState_ThroughGame) {
    auto p1 = playerManager_.createPlayer("Reader1");
    auto p2 = playerManager_.createPlayer("Reader2");
    p1->setStatus(PlayerStatus::Online);
    p2->setStatus(PlayerStatus::Online);

    auto game = std::make_shared<Game>("read-test", p1, p2);
    game->placeShipsRandomly(p1->getGuid());
    game->placeShipsRandomly(p2->getGuid());
    game->startBattle();

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Множество потоков читают состояние доски
    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([&]() {
            while (!stop) {
                auto fog1 = game->getBoardFogOfWar(p1->getGuid());
                auto fog2 = game->getBoardFogOfWar(p2->getGuid());
                auto full1 = game->getBoardFullState(p1->getGuid());
                auto full2 = game->getBoardFullState(p2->getGuid());
                (void)fog1;
                (void)fog2;
                (void)full1;
                (void)full2;
            }
        });
    }

    // Один поток стреляет
    threads.emplace_back([&]() {
        std::string turn = game->getCurrentTurnGuid();
        for (int x = 0; x < 10 && !game->isFinished(); ++x) {
            for (int y = 0; y < 10 && !game->isFinished(); ++y) {
                try {
                    game->shoot(turn, x, y);
                    turn = game->getCurrentTurnGuid();
                } catch (...) {
                }
            }
        }
        stop = true;
    });

    for (auto& t : threads)
        t.join();

    // Нет крэша — тест пройден
    EXPECT_TRUE(true);
}

// ============================================================
// Потокобезопасность Player
// ============================================================

TEST_F(GameThreadTest, ConcurrentPlayerStatusChanges) {
    auto player = playerManager_.createPlayer("StatusPlayer");

    std::vector<std::thread> threads;
    std::atomic<bool> stop{false};

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            while (!stop) {
                switch (t % 3) {
                    case 0:
                        player->setStatus(PlayerStatus::Online);
                        break;
                    case 1:
                        player->setStatus(PlayerStatus::InGame);
                        break;
                    case 2:
                        player->setStatus(PlayerStatus::Offline);
                        break;
                }
            }
        });
    }

    // Читатели
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([&]() {
            while (!stop) {
                auto status = player->getStatus();
                auto nick = player->getNickname();
                auto guid = player->getGuid();
                auto info = player->toPlayerInfo();
                (void)status;
                (void)nick;
                (void)guid;
                (void)info;
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true;

    for (auto& t : threads)
        t.join();

    // Финальный статус — один из трёх
    auto finalStatus = player->getStatus();
    EXPECT_TRUE(finalStatus == PlayerStatus::Online || finalStatus == PlayerStatus::InGame ||
                finalStatus == PlayerStatus::Offline);
}

// ============================================================
// Потокобезопасность FriendList
// ============================================================

TEST(FriendListThreadTest, ConcurrentAddCheckRemove) {
    FriendList friendList("owner-guid");
    const int friendCount = 100;

    std::vector<std::thread> threads;

    // Добавляем друзей параллельно
    for (int i = 0; i < friendCount; ++i) {
        threads.emplace_back([&, i]() { friendList.addFriend("friend-" + std::to_string(i)); });
    }

    for (auto& t : threads)
        t.join();
    threads.clear();

    EXPECT_EQ(friendList.size(), friendCount);

    // Проверяем и удаляем параллельно
    std::atomic<int> foundCount{0};
    for (int i = 0; i < friendCount; ++i) {
        threads.emplace_back([&, i]() {
            if (friendList.isFriend("friend-" + std::to_string(i))) {
                foundCount++;
            }
            friendList.removeFriend("friend-" + std::to_string(i));
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(foundCount.load(), friendCount);
    EXPECT_EQ(friendList.size(), 0u);
}

// ============================================================
// Потокобезопасность PlayerStats
// ============================================================

TEST(PlayerStatsThreadTest, ConcurrentRecording) {
    PlayerStats stats;
    const int iterations = 1000;

    std::vector<std::thread> threads;

    // Потоки записи побед
    threads.emplace_back([&]() {
        for (int i = 0; i < iterations; ++i) {
            stats.recordWin();
        }
    });

    // Потоки записи поражений
    threads.emplace_back([&]() {
        for (int i = 0; i < iterations; ++i) {
            stats.recordLoss();
        }
    });

    // Потоки записи выстрелов
    threads.emplace_back([&]() {
        for (int i = 0; i < iterations; ++i) {
            stats.recordShot(true);
        }
    });

    threads.emplace_back([&]() {
        for (int i = 0; i < iterations; ++i) {
            stats.recordShot(false);
        }
    });

    // Читатели
    threads.emplace_back([&]() {
        for (int i = 0; i < iterations; ++i) {
            stats.getWinrate();
            stats.getAccuracy();
            stats.toJson();
        }
    });

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(stats.getWins(), iterations);
    EXPECT_EQ(stats.getLosses(), iterations);
    EXPECT_EQ(stats.getGamesPlayed(), iterations * 2);
    EXPECT_EQ(stats.getShotsFired(), iterations * 2);
    EXPECT_EQ(stats.getShotsHit(), iterations);
}

}  // namespace test
}  // namespace seabattle