#pragma once

#include <Types.hpp>
#include <optional>
#include <string>

namespace seabattle {

class IPlayer {
   public:
    virtual ~IPlayer() = default;

    virtual std::string getGuid() const = 0;
    virtual std::string getNickname() const = 0;
    virtual PlayerStatus getStatus() const = 0;
    virtual void setStatus(PlayerStatus status) = 0;
    virtual std::optional<std::string> getCurrentGameId() const = 0;
    virtual void setCurrentGameId(const std::optional<std::string>& gameId) = 0;
    virtual bool isOnline() const = 0;
};

}  // namespace seabattle