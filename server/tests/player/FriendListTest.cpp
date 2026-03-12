#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "player/FriendList.hpp"

using namespace seabattle;

class FriendListTest : public ::testing::Test {
   protected:
    FriendList friendList{"owner-guid-123"};
};

TEST_F(FriendListTest, Initial_Empty) {
    EXPECT_EQ(friendList.size(), 0);
    EXPECT_TRUE(friendList.getFriends().empty());
}

TEST_F(FriendListTest, AddFriend_Success) {
    EXPECT_TRUE(friendList.addFriend("friend-guid-1"));
    EXPECT_EQ(friendList.size(), 1);
}

TEST_F(FriendListTest, AddFriend_AlreadyExists_ReturnsFalse) {
    friendList.addFriend("friend-guid-1");

    EXPECT_FALSE(friendList.addFriend("friend-guid-1"));
    EXPECT_EQ(friendList.size(), 1);
}

TEST_F(FriendListTest, AddFriend_Self_ReturnsFalse) {
    EXPECT_FALSE(friendList.addFriend("owner-guid-123"));
    EXPECT_EQ(friendList.size(), 0);
}

TEST_F(FriendListTest, AddFriend_Multiple_Success) {
    friendList.addFriend("friend-1");
    friendList.addFriend("friend-2");
    friendList.addFriend("friend-3");

    EXPECT_EQ(friendList.size(), 3);
}

TEST_F(FriendListTest, RemoveFriend_Exists_Success) {
    friendList.addFriend("friend-guid-1");

    EXPECT_TRUE(friendList.removeFriend("friend-guid-1"));
    EXPECT_EQ(friendList.size(), 0);
}

TEST_F(FriendListTest, RemoveFriend_NotExists_ReturnsFalse) {
    EXPECT_FALSE(friendList.removeFriend("non-existent"));
}

TEST_F(FriendListTest, IsFriend_Exists_ReturnsTrue) {
    friendList.addFriend("friend-guid-1");

    EXPECT_TRUE(friendList.isFriend("friend-guid-1"));
}

TEST_F(FriendListTest, IsFriend_NotExists_ReturnsFalse) {
    EXPECT_FALSE(friendList.isFriend("non-existent"));
}

TEST_F(FriendListTest, GetFriends_ReturnsAll) {
    friendList.addFriend("friend-1");
    friendList.addFriend("friend-2");
    friendList.addFriend("friend-3");

    auto friends = friendList.getFriends();

    EXPECT_EQ(friends.size(), 3);
    EXPECT_NE(std::find(friends.begin(), friends.end(), "friend-1"), friends.end());
    EXPECT_NE(std::find(friends.begin(), friends.end(), "friend-2"), friends.end());
    EXPECT_NE(std::find(friends.begin(), friends.end(), "friend-3"), friends.end());
}

TEST_F(FriendListTest, DefaultConstructor_CanAddAnyFriend) {
    FriendList list;

    EXPECT_TRUE(list.addFriend("any-guid"));
    EXPECT_EQ(list.size(), 1);
}

TEST_F(FriendListTest, ThreadSafety_ConcurrentAdd) {
    const int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i]() { friendList.addFriend("friend-" + std::to_string(i)); });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(friendList.size(), numThreads);
}

TEST_F(FriendListTest, ThreadSafety_ConcurrentAddRemove) {
    friendList.addFriend("friend-0");

    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this]() {
            friendList.removeFriend("friend-0");
            friendList.addFriend("friend-0");
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_LE(friendList.size(), 1);
}
