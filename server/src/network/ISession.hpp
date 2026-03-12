#pragma once

#include <memory>
#include <string>

namespace seabattle {

class Player;

class ISession {
   public:
    virtual ~ISession() = default;

    virtual void send(const std::string& message) = 0;
    virtual void close() = 0;

    virtual std::shared_ptr<Player> getPlayer() const = 0;
    virtual void setPlayer(std::shared_ptr<Player> player) = 0;

    virtual bool isOpen() const = 0;
};

}  // namespace seabattle