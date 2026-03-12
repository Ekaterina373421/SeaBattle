#pragma once

#include <gmock/gmock.h>

#include "player/IPlayer.hpp"

namespace seabattle {

class MockPlayer : public IPlayer {
   public:
    MOCK_METHOD(std::string, getGuid, (), (const, override));
    MOCK_METHOD(std::string, getNickname, (), (const, override));
    MOCK_METHOD(PlayerStatus, getStatus, (), (const, override));
    MOCK_METHOD(void, setStatus, (PlayerStatus status), (override));
    MOCK_METHOD(std::optional<std::string>, getCurrentGameId, (), (const, override));
    MOCK_METHOD(void, setCurrentGameId, (const std::optional<std::string>& gameId), (override));
    MOCK_METHOD(bool, isOnline, (), (const, override));
};

}