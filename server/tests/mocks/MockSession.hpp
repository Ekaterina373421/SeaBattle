#pragma once

#include <gmock/gmock.h>

#include "network/ISession.hpp"

namespace seabattle {

class MockSession : public ISession {
   public:
    MOCK_METHOD(void, send, (const std::string& message), (override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(std::shared_ptr<Player>, getPlayer, (), (const, override));
    MOCK_METHOD(void, setPlayer, (std::shared_ptr<Player> player), (override));
    MOCK_METHOD(bool, isOpen, (), (const, override));
};

}