#include "Matchmaker.hpp"

#include <algorithm>

#include "player/Player.hpp"

namespace seabattle {

void Matchmaker::addPlayer(std::shared_ptr<Player> player) {
    if (!player)
        return;

    std::lock_guard<std::mutex> lock(mutex_);

    cleanupExpired();

    std::string guid = player->getGuid();
    for (const auto& wp : queue_) {
        if (auto p = wp.lock()) {
            if (p->getGuid() == guid) {
                return;
            }
        }
    }

    queue_.push_back(player);
}

void Matchmaker::removePlayer(const std::string& playerGuid) {
    std::lock_guard<std::mutex> lock(mutex_);

    queue_.erase(std::remove_if(queue_.begin(), queue_.end(),
                                [&playerGuid](const std::weak_ptr<Player>& wp) {
                                    auto p = wp.lock();
                                    return !p || p->getGuid() == playerGuid;
                                }),
                 queue_.end());
}

std::optional<std::pair<std::shared_ptr<Player>, std::shared_ptr<Player>>> Matchmaker::tryMatch() {
    std::lock_guard<std::mutex> lock(mutex_);

    cleanupExpired();

    std::vector<std::shared_ptr<Player>> available;

    for (auto it = queue_.begin(); it != queue_.end() && available.size() < 2;) {
        if (auto p = it->lock()) {
            if (p->getStatus() == PlayerStatus::Online) {
                available.push_back(p);
                it = queue_.erase(it);
            } else {
                ++it;
            }
        } else {
            it = queue_.erase(it);
        }
    }

    if (available.size() >= 2) {
        return std::make_pair(available[0], available[1]);
    }

    for (auto& p : available) {
        queue_.push_front(p);
    }

    return std::nullopt;
}

bool Matchmaker::isInQueue(const std::string& playerGuid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& wp : queue_) {
        if (auto p = wp.lock()) {
            if (p->getGuid() == playerGuid) {
                return true;
            }
        }
    }
    return false;
}

size_t Matchmaker::getQueueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    for (const auto& wp : queue_) {
        if (!wp.expired()) {
            ++count;
        }
    }
    return count;
}

void Matchmaker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
}

void Matchmaker::cleanupExpired() {
    queue_.erase(std::remove_if(queue_.begin(), queue_.end(),
                                [](const std::weak_ptr<Player>& wp) { return wp.expired(); }),
                 queue_.end());
}

}  // namespace seabattle
