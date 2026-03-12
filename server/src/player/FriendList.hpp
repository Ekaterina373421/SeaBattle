#pragma once

#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace seabattle {

class FriendList {
   public:
    FriendList() = default;
    explicit FriendList(const std::string& ownerGuid);

    bool addFriend(const std::string& guid);
    bool removeFriend(const std::string& guid);
    bool isFriend(const std::string& guid) const;

    std::vector<std::string> getFriends() const;
    size_t size() const;

   private:
    std::string owner_guid_;
    mutable std::mutex mutex_;
    std::set<std::string> friends_;
};

}  // namespace seabattle