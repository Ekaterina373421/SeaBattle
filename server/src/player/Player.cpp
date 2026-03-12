#include "Player.hpp"

namespace seabattle {

Player::Player(const std::string& guid, const std::string& nickname)
    : guid_(guid), nickname_(nickname), friends_(guid) {}

std::string Player::getGuid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return guid_;
}

std::string Player::getNickname() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nickname_;
}

void Player::setNickname(const std::string& nickname) {
    std::lock_guard<std::mutex> lock(mutex_);
    nickname_ = nickname;
}

PlayerStatus Player::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void Player::setStatus(PlayerStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
}

std::optional<std::string> Player::getCurrentGameId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_game_id_;
}

void Player::setCurrentGameId(const std::optional<std::string>& gameId) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_game_id_ = gameId;
}

bool Player::isOnline() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_ != PlayerStatus::Offline;
}

std::weak_ptr<Session> Player::getSession() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_;
}

void Player::setSession(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_ = session;
}

FriendList& Player::getFriends() {
    return friends_;
}

const FriendList& Player::getFriends() const {
    return friends_;
}

PlayerStats& Player::getStats() {
    return stats_;
}

const PlayerStats& Player::getStats() const {
    return stats_;
}

PlayerInfo Player::toPlayerInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return PlayerInfo{guid_, nickname_, status_, current_game_id_};
}

}  // namespace seabattle
