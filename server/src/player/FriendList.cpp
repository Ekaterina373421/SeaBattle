#include "FriendList.hpp"

namespace seabattle {

FriendList::FriendList(const std::string& ownerGuid) : owner_guid_(ownerGuid) {}

bool FriendList::addFriend(const std::string& guid) {
    if (guid == owner_guid_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto result = friends_.insert(guid);
    return result.second;
}

bool FriendList::removeFriend(const std::string& guid) {
    std::lock_guard<std::mutex> lock(mutex_);
    return friends_.erase(guid) > 0;
}

bool FriendList::isFriend(const std::string& guid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return friends_.count(guid) > 0;
}

std::vector<std::string> FriendList::getFriends() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {friends_.begin(), friends_.end()};
}

size_t FriendList::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return friends_.size();
}

}  // namespace seabattle
