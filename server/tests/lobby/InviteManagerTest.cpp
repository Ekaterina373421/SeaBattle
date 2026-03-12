#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "lobby/InviteManager.hpp"

using namespace seabattle;

class InviteManagerTest : public ::testing::Test {
   protected:
    InviteManager manager;
};

TEST_F(InviteManagerTest, Initial_NoInvites) {
    EXPECT_EQ(manager.getInviteCount(), 0);
}

TEST_F(InviteManagerTest, CreateInvite_GeneratesId) {
    std::string id = manager.createInvite("from-guid", "to-guid");

    EXPECT_FALSE(id.empty());
    EXPECT_EQ(manager.getInviteCount(), 1);
}

TEST_F(InviteManagerTest, CreateInvite_StoresInvite) {
    std::string id = manager.createInvite("from-guid", "to-guid");

    auto invite = manager.getInvite(id);

    ASSERT_TRUE(invite.has_value());
    EXPECT_EQ(invite->from_guid, "from-guid");
    EXPECT_EQ(invite->to_guid, "to-guid");
}

TEST_F(InviteManagerTest, CreateInvite_DuplicatePair_ReturnsEmpty) {
    manager.createInvite("from-guid", "to-guid");

    std::string id2 = manager.createInvite("from-guid", "to-guid");

    EXPECT_TRUE(id2.empty());
    EXPECT_EQ(manager.getInviteCount(), 1);
}

TEST_F(InviteManagerTest, CreateInvite_DifferentPairs_CreatesMultiple) {
    manager.createInvite("from-1", "to-1");
    manager.createInvite("from-2", "to-2");

    EXPECT_EQ(manager.getInviteCount(), 2);
}

TEST_F(InviteManagerTest, CreateInvite_ReversePair_CreatesNew) {
    manager.createInvite("player-1", "player-2");
    std::string id2 = manager.createInvite("player-2", "player-1");

    EXPECT_FALSE(id2.empty());
    EXPECT_EQ(manager.getInviteCount(), 2);
}

TEST_F(InviteManagerTest, GetInvite_NotExists_ReturnsEmpty) {
    auto invite = manager.getInvite("non-existent");

    EXPECT_FALSE(invite.has_value());
}

TEST_F(InviteManagerTest, AcceptInvite_ReturnsPlayers) {
    std::string id = manager.createInvite("from-guid", "to-guid");

    auto result = manager.acceptInvite(id);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "from-guid");
    EXPECT_EQ(result->second, "to-guid");
}

TEST_F(InviteManagerTest, AcceptInvite_RemovesInvite) {
    std::string id = manager.createInvite("from-guid", "to-guid");

    manager.acceptInvite(id);

    EXPECT_EQ(manager.getInviteCount(), 0);
    EXPECT_FALSE(manager.getInvite(id).has_value());
}

TEST_F(InviteManagerTest, AcceptInvite_NotExists_ReturnsEmpty) {
    auto result = manager.acceptInvite("non-existent");

    EXPECT_FALSE(result.has_value());
}

TEST_F(InviteManagerTest, AcceptInvite_AlreadyAccepted_ReturnsEmpty) {
    std::string id = manager.createInvite("from-guid", "to-guid");
    manager.acceptInvite(id);

    auto result = manager.acceptInvite(id);

    EXPECT_FALSE(result.has_value());
}

TEST_F(InviteManagerTest, DeclineInvite_RemovesInvite) {
    std::string id = manager.createInvite("from-guid", "to-guid");

    bool result = manager.declineInvite(id);

    EXPECT_TRUE(result);
    EXPECT_EQ(manager.getInviteCount(), 0);
}

TEST_F(InviteManagerTest, DeclineInvite_NotExists_ReturnsFalse) {
    bool result = manager.declineInvite("non-existent");

    EXPECT_FALSE(result);
}

TEST_F(InviteManagerTest, CancelInvite_ByCreator_Succeeds) {
    std::string id = manager.createInvite("from-guid", "to-guid");

    bool result = manager.cancelInvite(id, "from-guid");

    EXPECT_TRUE(result);
    EXPECT_EQ(manager.getInviteCount(), 0);
}

TEST_F(InviteManagerTest, CancelInvite_ByOther_Fails) {
    std::string id = manager.createInvite("from-guid", "to-guid");

    bool result = manager.cancelInvite(id, "to-guid");

    EXPECT_FALSE(result);
    EXPECT_EQ(manager.getInviteCount(), 1);
}

TEST_F(InviteManagerTest, GetInvitesForPlayer_ReturnsCorrect) {
    manager.createInvite("from-1", "target");
    manager.createInvite("from-2", "target");
    manager.createInvite("from-3", "other");

    auto invites = manager.getInvitesForPlayer("target");

    EXPECT_EQ(invites.size(), 2);
}

TEST_F(InviteManagerTest, GetInvitesFromPlayer_ReturnsCorrect) {
    manager.createInvite("sender", "to-1");
    manager.createInvite("sender", "to-2");
    manager.createInvite("other", "to-3");

    auto invites = manager.getInvitesFromPlayer("sender");

    EXPECT_EQ(invites.size(), 2);
}

TEST_F(InviteManagerTest, RemoveInvitesForPlayer_RemovesAll) {
    manager.createInvite("player", "to-1");
    manager.createInvite("from-1", "player");
    manager.createInvite("other1", "other2");

    manager.removeInvitesForPlayer("player");

    EXPECT_EQ(manager.getInviteCount(), 1);
}

TEST_F(InviteManagerTest, HasActiveInvite_Exists_ReturnsTrue) {
    manager.createInvite("from-guid", "to-guid");

    EXPECT_TRUE(manager.hasActiveInvite("from-guid", "to-guid"));
}

TEST_F(InviteManagerTest, HasActiveInvite_NotExists_ReturnsFalse) {
    EXPECT_FALSE(manager.hasActiveInvite("from-guid", "to-guid"));
}

TEST_F(InviteManagerTest, HasActiveInvite_ReversePair_ReturnsFalse) {
    manager.createInvite("from-guid", "to-guid");

    EXPECT_FALSE(manager.hasActiveInvite("to-guid", "from-guid"));
}

TEST_F(InviteManagerTest, CleanupExpired_RemovesOldInvites) {
    EXPECT_EQ(manager.getInviteCount(), 0);

    manager.cleanupExpired();

    EXPECT_EQ(manager.getInviteCount(), 0);
}

TEST_F(InviteManagerTest, ThreadSafety_ConcurrentOperations) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i]() {
            std::string id =
                manager.createInvite("from-" + std::to_string(i), "to-" + std::to_string(i));

            if (!id.empty()) {
                manager.getInvite(id);
                if (i % 2 == 0) {
                    manager.acceptInvite(id);
                } else {
                    manager.declineInvite(id);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(manager.getInviteCount(), 0);
}
