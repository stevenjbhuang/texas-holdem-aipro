#pragma once
#include "core/PlayerView.hpp"
#include "core/Hand.hpp"
#include "core/Types.hpp"

namespace poker {

class IPlayer {
public:

    virtual ~IPlayer() = default;
    virtual PlayerId getId() const = 0;
    virtual void dealHoleCards(const Hand& cards) = 0;
    virtual Action getAction(const PlayerView& view) = 0;
};

}